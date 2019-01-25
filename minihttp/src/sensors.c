#include "sensors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>

#include <dlfcn.h>
#include <inttypes.h>

#include "tools.h"

int (*sensor_register_callback_fn)(void);
int (*sensor_unregister_callback_fn)(void);
void *libsns_so = NULL;

int tryLoadLibrary(const char *path) {
    printf("try to load: %s\n", path);
    libsns_so = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    printf("libsns_so 0x%016" PRIXPTR "\n", (uintptr_t) libsns_so);
    if (libsns_so == NULL) {
        //printf("dlopen \"%s\" error:\n", path);
        printf("dlopen \"%s\" error: %s\n", path, dlerror());
        return 0;
    }
    return 1;
}
int LoadSensorLibrary(const char *libsns_name) {
    char path[250];
    sprintf(path, "%s", libsns_name);
    if (tryLoadLibrary(path) != 1) {
        sprintf(path, "./%s", libsns_name);
        if (tryLoadLibrary(path) != 1) {
            sprintf(path, "/usr/lib/%s", libsns_name);
            if (tryLoadLibrary(path) != 1) {
                return 0;
            }
        }
    }
    sensor_register_callback_fn = dlsym(libsns_so, "sensor_register_callback");
    sensor_unregister_callback_fn = dlsym(libsns_so, "sensor_unregister_callback");
    return 1;
}

void UnloadSensorLibrary() {
    dlclose(libsns_so);
    libsns_so = NULL;
}

int sensor_register_callback(void) {
    return sensor_register_callback_fn();
}
int sensor_unregister_callback(void) {
    return sensor_unregister_callback_fn();
}



// ^\[([a-zA-Z]+)\][.\s\S]+(Dll_File)\s*=\s*(\S[^;\s]*)\s*;?.*$
// ^\[([a-zA-Z]+)\][.\s\S][^\[\]]+(dev_attr)\s*=\s*(\S[^;\s]*)\s*;?.*$
char reg_buf[1024];
int getParamValue(const char *str, const char *param_name, char *param_value) {
    regex_t regex;
    char reg_buf_len = sprintf(reg_buf, "^[[:space:]]*%s[[:space:]]*=[[:space:]]*(.[^[:space:];]*)", param_name);
    reg_buf[reg_buf_len] = 0;
    if(compile_regex(&regex, reg_buf) < 0) { printf("compile_regex error\n"); return -1; };
    size_t n_matches = 2; // We have 1 capturing group + the whole match group
    regmatch_t m[n_matches];
    const char *p = str;
    int match = regexec(&regex, p, n_matches, m, 0);
    regfree(&regex);
    if (match) { printf("Can't find '%s'", param_name); return -1; }

    int res = sprintf(param_value, "%.*s", (int)(m[1].rm_eo - m[1].rm_so), &str[m[1].rm_so]);
    if (res <= 0 ) { return -1; }
    param_value[res] = 0;
    return res;
}

int parseEnum(const char *str, const char *param_name, char *enum_value, const char *possible_values[], const int possible_values_count, const int possible_values_offset) {
    char param_value[64];
    if (getParamValue(str, param_name, param_value) < 0) return -1;

    // try to parse as number
    char* end;
    long res = strtol(param_value, &end, 10);
    if (*end) { res = strtol(param_value, &end, 16); }
    if (!*end) { *enum_value = (char)res; return 0; }

    // try to find value in possible values
    for(unsigned int i = 0; i < possible_values_count; ++i)
        if (strcmp(param_value, possible_values[i])  == 0) { *enum_value = (char)(possible_values_offset+i); return 0; }

    // print error
    printf("Can't parse param '%s' value '%s'. Is not a number and is not in possible values: ", param_name, param_value);
    for(unsigned int i = 0; i < possible_values_count; ++i)  printf("'%s', ", possible_values[i]);
    return -1;
}

int parseInt(const char *str, const char *param_name, const int min, const int max, int *int_value) {
    char param_value[64];
    if (getParamValue(str, param_name, param_value) < 0) return -1;

    // try to parse as number
    char* end = NULL;
    long res = strtol(param_value, &end, 10);
    if (*end) { res = strtol(param_value, &end, 16); }
    if (!*end) {
        if (res < min || res > max) {
            printf("Can't parse param '%s' value '%s'. Value '%ld' is not in a range [%d; %d].", param_name, param_value, res, min, max);
            return -1;
        }
        *int_value = (int)res;
        return 0;
    }
    printf("Can't parse param '%s' value '%s'. Is not a integer (dec or hex) number.", param_name, param_value);
    return -1;
}

int parse_sensor_config(const char *path, struct SensorConfig *config) {
    if (config == NULL) return -1;

    char *str = NULL; {
        FILE * file = fopen (path, "rb");
        if (!file) { printf("Can't open file %s\n", path); return -1; }

        fseek (file, 0, SEEK_END);
        size_t length = (size_t)ftell(file);
        fseek (file, 0, SEEK_SET);

        str = malloc(length + 1);
        if (!str) { printf("Can't allocate buf in parse_sensor_config\n"); fclose(file); return -1; }
        size_t n = fread(str, 1, length, file);
        if (n != length) { printf("Can't read all file %s\n", path); fclose(file); free(str); return -1; }
        fclose (file);
        str[length] = 0;
    }

    if (getParamValue(str, "sensor_type", config->type) < 0) goto RET_ERR;
    if (getParamValue(str, "DllFile", config->so_path) < 0) goto RET_ERR;
    {
        const char* possible_values[] = {
            "WDR_MODE_NONE", "WDR_MODE_BUILT_IN",
            "WDR_MODE_2To1_LINE", "WDR_MODE_2To1_FRAME", "WDR_MODE_2To1_FRAME_FULL_RATE",
            "WDR_MODE_3To1_LINE", "WDR_MODE_3To1_FRAME", "WDR_MODE_3To1_FRAME_FULL_RATE",
            "WDR_MODE_4To1_LINE", "WDR_MODE_4To1_FRAME", "WDR_MODE_4To1_FRAME_FULL_RATE"
        };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "mode", (char *)&config->mode, possible_values, count, 0) < 0) goto RET_ERR;
    }
    {
        const char* possible_values[] = {
            "INPUT_MODE_MIPI",         // = 0x0,              /* mipi */
            "INPUT_MODE_SUBLVDS",      // = 0x1,              /* SUB_LVDS */
            "INPUT_MODE_LVDS",         // = 0x2,              /* LVDS */
            "INPUT_MODE_HISPI",        // = 0x3,              /* HISPI */
            "INPUT_MODE_CMOS_18V",     // = 0x4,              /* CMOS 1.8V */
            "INPUT_MODE_CMOS_33V",     // = 0x5,              /* CMOS 3.3V */
            "INPUT_MODE_BT1120",       // = 0x6,              /* BT1120 */
            "INPUT_MODE_BYPASS",       // = 0x7,              /* MIPI Bypass */
        };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "input_mode", (char *)&config->input_mode, possible_values, count, 0) < 0) goto RET_ERR;
    }

    if (parseInt(str, "dev_attr", 0, 2, &config->dev_attr) < 0) goto RET_ERR;

    {
        const char* possible_values[] = { // starts from 1 !!!!!
            "RAW_DATA_8BIT", "RAW_DATA_10BIT", "RAW_DATA_12BIT", "RAW_DATA_14BIT"
        };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "data_type", (char *)&config->data_type,  possible_values, count, 1) < 0) goto RET_ERR;
    }

    {
        const char* possible_values[] = { "BAYER_RGGB", "BAYER_GRBG", "BAYER_GBRG", "BAYER_BGGR" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "isp_bayer", (char *)&config->isp_bayer,  possible_values, count, 0) < 0) goto RET_ERR;
    }

    if (parseInt(str, "Mask_num", 0, 2, &config->mask_num) < 0) goto RET_ERR;
    if (parseInt(str, "Mask_0", 0, INT_MAX, &config->mask_0) < 0) goto RET_ERR;
    if (parseInt(str, "Mask_1", 0, INT_MAX, &config->mask_1) < 0) goto RET_ERR;

    free(str); return 0;
    RET_ERR: free(str); return -1;
}
