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

int parseEnum(const char *str, const char *param_name, void *enum_value, const char *possible_values[], const int possible_values_count, const int possible_values_offset) {
    char param_value[64];
    if (getParamValue(str, param_name, param_value) < 0) return -1;

    // try to parse as number
    char* end;
    long res = strtol(param_value, &end, 10);
    if (*end) { res = strtol(param_value, &end, 16); }
    if (!*end) { *(VI_INPUT_MODE_E*)(enum_value) = (VI_INPUT_MODE_E)res; return 0; }

    // try to find value in possible values
    for(unsigned int i = 0; i < possible_values_count; ++i)
        if (strcmp(param_value, possible_values[i])  == 0) { *(VI_INPUT_MODE_E*)(enum_value) = (VI_INPUT_MODE_E)(possible_values_offset+i); return 0; }

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
    if (strcmp(param_value, "true")  == 0) { *int_value = 1; return 0; }
    if (strcmp(param_value, "TRUE")  == 0) { *int_value = 1; return 0; }
    if (strcmp(param_value, "false")  == 0) { *int_value = 0; return 0; }
    if (strcmp(param_value, "FALSE")  == 0) { *int_value = 0; return 0; }

    printf("Can't parse param '%s' value '%s'. Is not a integer (dec or hex) number.", param_name, param_value);
    return -1;
}

int parseArray(const char *str, const char *param_name, char **array, const int array_size) {
    char param_value[256];
    if (getParamValue(str, param_name, param_value) < 0) return -1;

    return 1;
}

int parse_config_lvds(const char *str, struct SensorLVDS *lvds) {
    if (parseInt(str, "img_size_w", INT_MIN, INT_MAX, &lvds->img_size_w) < 0) return -1;
    if (parseInt(str, "img_size_h", INT_MIN, INT_MAX, &lvds->img_size_h) < 0) return -1;
    {
        const char* possible_values[] = { "HI_WDR_MODE_NONE", "HI_WDR_MODE_2F", "HI_WDR_MODE_3F", "HI_WDR_MODE_4F", "HI_WDR_MODE_DOL_2F", "HI_WDR_MODE_DOL_3F", "HI_WDR_MODE_DOL_4F" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "wdr_mode", (void *)&lvds->wdr_mode,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "LVDS_SYNC_MODE_SOL", "LVDS_SYNC_MODE_SAV" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "sync_mode", (void *)&lvds->sync_mode,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "RAW_DATA_8BIT", "RAW_DATA_10BIT", "RAW_DATA_12BIT", "RAW_DATA_14BIT" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "raw_data_type", (void *)&lvds->raw_data_type,  possible_values, count, 1) < 0) return -1;
    }
    {
        const char* possible_values[] = { "LVDS_ENDIAN_LITTLE", "LVDS_ENDIAN_BIG" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "data_endian", (void *)&lvds->data_endian,  possible_values, count, 1) < 0) return -1;
        if (parseEnum(str, "sync_code_endian", (void *)&lvds->sync_code_endian,  possible_values, count, 1) < 0) return -1;
    }
    if (parseArray(str, "lane_id", &lvds->lane_id, 8) < 0) return -1;
    if (parseInt(str, "lvds_lane_num", INT_MIN, INT_MAX, &lvds->lvds_lane_num) < 0) return -1;
    if (parseInt(str, "wdr_vc_num", INT_MIN, INT_MAX, &lvds->wdr_vc_num) < 0) return -1;
    if (parseInt(str, "sync_code_num", INT_MIN, INT_MAX, &lvds->sync_code_num) < 0) return -1;
    if (parseArray(str, "sync_code_0", &lvds->sync_code_0, 16) < 0) return -1;
    if (parseArray(str, "sync_code_1", &lvds->sync_code_1, 16) < 0) return -1;
    if (parseArray(str, "sync_code_2", &lvds->sync_code_2, 16) < 0) return -1;
    if (parseArray(str, "sync_code_3", &lvds->sync_code_3, 16) < 0) return -1;
    if (parseArray(str, "sync_code_4", &lvds->sync_code_4, 16) < 0) return -1;
    if (parseArray(str, "sync_code_5", &lvds->sync_code_5, 16) < 0) return -1;
    if (parseArray(str, "sync_code_6", &lvds->sync_code_6, 16) < 0) return -1;
    if (parseArray(str, "sync_code_7", &lvds->sync_code_7, 16) < 0) return -1;
    return 1;
}

int parse_config_videv(const char *str, struct SensorConfig *config) {
    {
        const char* possible_values[] = { "VI_INPUT_MODE_BT656", "VI_INPUT_MODE_BT601", "VI_INPUT_MODE_DIGITAL_CAMERA", "VI_INPUT_MODE_INTERLEAVED", "VI_INPUT_MODE_MIPI", "VI_INPUT_MODE_LVDS", "VI_INPUT_MODE_HISPI" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "Input_mod", (void *)&config->input_mod,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_WORK_MODE_1Multiplex", "VI_WORK_MODE_2Multiplex", "VI_WORK_MODE_4Multiplex" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "Work_mod", (void *)&config->work_mod,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_COMBINE_COMPOSITE", "VI_COMBINE_SEPARATE" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "Combine_mode", (void *)&config->combine_mode,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_COMP_MODE_SINGLE", "VI_COMP_MODE_DOUBLE" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "Comp_mode", (void *)&config->comp_mode,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_CLK_EDGE_SINGLE_UP", "VI_CLK_EDGE_SINGLE_DOWN", "VI_CLK_EDGE_DOUBLE" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "Clock_edge", (void *)&config->clock_edge,  possible_values, count, 0) < 0) return -1;
    }
    if (parseInt(str, "Mask_num", 0, 2, &config->mask_num) < 0) return -1;
    if (parseInt(str, "Mask_0", 0, INT_MAX, &config->mask_0) < 0) return -1;
    if (parseInt(str, "Mask_1", 0, INT_MAX, &config->mask_1) < 0) return -1;
    {
        const char* possible_values[] = { "VI_SCAN_INTERLACED", "VI_SCAN_PROGRESSIVE" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "Scan_mode", (void *)&config->scan_mode,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_INPUT_DATA_VUVU", "VI_INPUT_DATA_UVUV", "VI_INPUT_DATA_UYVY", "VI_INPUT_DATA_VYUY", "VI_INPUT_DATA_YUYV", "VI_INPUT_DATA_YVYU" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "Data_seq", (void *)&config->data_seq,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_VSYNC_FIELD", "VI_VSYNC_PULSE" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "Vsync", (void *)&config->vsync,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_VSYNC_NEG_HIGH", "VI_VSYNC_NEG_LOW" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "VsyncNeg", (void *)&config->vsync_neg,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_HSYNC_VALID_SINGNAL", "VI_HSYNC_PULSE" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "Hsync", (void *)&config->hsync,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_HSYNC_NEG_HIGH", "VI_HSYNC_NEG_LOW" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "HsyncNeg", (void *)&config->hsync_neg,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_VSYNC_NORM_PULSE", "VI_VSYNC_VALID_SINGAL" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "VsyncValid", (void *)&config->vsync_valid,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_VSYNC_VALID_NEG_HIGH", "VI_VSYNC_VALID_NEG_LOW" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "VsyncValidNeg", (void *)&config->vsync_valid_neg,  possible_values, count, 0) < 0) return -1;
    }
    if (parseInt(str, "Timingblank_HsyncHfb", 0, INT_MAX, &config->timing_blank_hsync_hfb) < 0) return -1;
    if (parseInt(str, "Timingblank_HsyncAct", 0, INT_MAX, &config->timing_blank_hsync_act) < 0) return -1;
    if (parseInt(str, "Timingblank_HsyncHbb", 0, INT_MAX, &config->timing_blank_hsync_hbb) < 0) return -1;
    if (parseInt(str, "Timingblank_VsyncVfb", 0, INT_MAX, &config->timing_blank_vsync_vfb) < 0) return -1;
    if (parseInt(str, "Timingblank_VsyncVact", 0, INT_MAX, &config->timing_blank_vsync_vact) < 0) return -1;
    if (parseInt(str, "Timingblank_VsyncVbb", 0, INT_MAX, &config->timing_blank_vsync_vbb) < 0) return -1;
    if (parseInt(str, "Timingblank_VsyncVbfb", 0, INT_MAX, &config->timing_blank_vsync_vbfb) < 0) return -1;
    if (parseInt(str, "Timingblank_VsyncVbact", 0, INT_MAX, &config->timing_blank_vsync_vbact) < 0) return -1;
    if (parseInt(str, "Timingblank_VsyncVbbb", 0, INT_MAX, &config->timing_blank_vsync_vbbb) < 0) return -1;
    {
        const char* possible_values[] = { "BT656_FIXCODE_1", "BT656_FIXCODE_0" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "FixCode", (void *)&config->fix_code,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "BT656_FIELD_POLAR_STD", "BT656_FIELD_POLAR_NSTD" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "FieldPolar", (void *)&config->field_polar,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_PATH_BYPASS", "VI_PATH_ISP", "VI_PATH_RAW" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "DataPath", (void *)&config->data_path,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "VI_DATA_TYPE_YUV", "VI_DATA_TYPE_RGB" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "InputDataType", &config->input_data_type,  possible_values, count, 0) < 0) return -1;
    }
    if (parseInt(str, "DataRev", 0, INT_MAX, &config->data_rev) < 0) return -1;
    if (parseInt(str, "DevRect_x", 0, INT_MAX, &config->dev_rect_x) < 0) return -1;
    if (parseInt(str, "DevRect_y", 0, INT_MAX, &config->dev_rect_y) < 0) return -1;
    if (parseInt(str, "DevRect_w", 0, INT_MAX, &config->dev_rect_w) < 0) return -1;
    if (parseInt(str, "DevRect_h", 0, INT_MAX, &config->dev_rect_h) < 0) return -1;
    return 1;
}

int parse_config_vichn(const char *str, struct SensorConfig *config) {
    if (parseInt(str, "CapRect_X", 0, INT_MAX, &config->cap_rect_x) < 0) return -1;
    if (parseInt(str, "CapRect_Y", 0, INT_MAX, &config->cap_rect_y) < 0) return -1;
    if (parseInt(str, "CapRect_Width", 0, INT_MAX, &config->cap_rect_width) < 0) return -1;
    if (parseInt(str, "CapRect_Height", 0, INT_MAX, &config->cap_rect_height) < 0) return -1;
    if (parseInt(str, "DestSize_Width", 0, INT_MAX, &config->dest_size_width) < 0) return -1;
    if (parseInt(str, "DestSize_Height", 0, INT_MAX, &config->dest_size_height) < 0) return -1;
    {
        const char* possible_values[] = { "VI_CAPSEL_TOP", "VI_CAPSEL_BOTTOM", "VI_CAPSEL_BOTH" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "CapSel", (void *)&config->cap_sel,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = {
                "PIXEL_FORMAT_RGB_1BPP", "PIXEL_FORMAT_RGB_2BPP", "PIXEL_FORMAT_RGB_4BPP", "PIXEL_FORMAT_RGB_8BPP", "PIXEL_FORMAT_RGB_444",
                "PIXEL_FORMAT_RGB_4444", "PIXEL_FORMAT_RGB_555", "PIXEL_FORMAT_RGB_565", "PIXEL_FORMAT_RGB_1555",
                "PIXEL_FORMAT_RGB_888", "PIXEL_FORMAT_RGB_8888",
                "PIXEL_FORMAT_RGB_PLANAR_888", "PIXEL_FORMAT_RGB_BAYER_8BPP", "PIXEL_FORMAT_RGB_BAYER_10BPP", "PIXEL_FORMAT_RGB_BAYER_12BPP", "PIXEL_FORMAT_RGB_BAYER_14BPP",
                "PIXEL_FORMAT_RGB_BAYER",
                "PIXEL_FORMAT_YUV_A422", "PIXEL_FORMAT_YUV_A444",
                "PIXEL_FORMAT_YUV_PLANAR_422", "PIXEL_FORMAT_YUV_PLANAR_420",
                "PIXEL_FORMAT_YUV_PLANAR_444",
                "PIXEL_FORMAT_YUV_SEMIPLANAR_422", "PIXEL_FORMAT_YUV_SEMIPLANAR_420", "PIXEL_FORMAT_YUV_SEMIPLANAR_444",
                "PIXEL_FORMAT_UYVY_PACKAGE_422", "PIXEL_FORMAT_YUYV_PACKAGE_422", "PIXEL_FORMAT_VYUY_PACKAGE_422", "PIXEL_FORMAT_YCbCr_PLANAR",
                "PIXEL_FORMAT_YUV_400" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "PixFormat", (void *)&config->pix_format,  possible_values, count, 0) < 0) return -1;
    }
    {
        const char* possible_values[] = { "COMPRESS_MODE_NONE", "COMPRESS_MODE_SEG", "COMPRESS_MODE_SEG128", "COMPRESS_MODE_LINE", "COMPRESS_MODE_FRAME" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "CompressMode", (void *)&config->compress_mode,  possible_values, count, 0) < 0) return -1;
    }
    if (parseInt(str, "SrcFrameRate", INT_MIN, INT_MAX, &config->src_frame_rate) < 0) return -1;
    if (parseInt(str, "FrameRate", INT_MIN, INT_MAX, &config->frame_rate) < 0) return -1;
    return 1;
}

int parse_sensor_config(const char *path, struct SensorConfig *config) {
    if (config == NULL) return -1;
    memset(config, 0, sizeof(struct SensorConfig));

    // load config file to string
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

    // [sensor]
    if (getParamValue(str, "sensor_type", config->sensor_type) < 0) goto RET_ERR;
    {
        const char* possible_values[] = {
            "WDR_MODE_NONE", "WDR_MODE_BUILT_IN",
            "WDR_MODE_2To1_LINE", "WDR_MODE_2To1_FRAME", "WDR_MODE_2To1_FRAME_FULL_RATE",
            "WDR_MODE_3To1_LINE", "WDR_MODE_3To1_FRAME", "WDR_MODE_3To1_FRAME_FULL_RATE",
            "WDR_MODE_4To1_LINE", "WDR_MODE_4To1_FRAME", "WDR_MODE_4To1_FRAME_FULL_RATE"
        };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "mode", (void *)&config->mode, possible_values, count, 0) < 0) goto RET_ERR;
    }
    if (getParamValue(str, "DllFile", config->dll_file) < 0) goto RET_ERR;

    // [mode]
    {
        const char* possible_values[] = { "INPUT_MODE_MIPI", "INPUT_MODE_SUBLVDS", "INPUT_MODE_LVDS", "INPUT_MODE_HISPI", "INPUT_MODE_CMOS_18V", "INPUT_MODE_CMOS_33V", "INPUT_MODE_BT1120", "INPUT_MODE_BYPASS" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "input_mode", (void *)&config->input_mode, possible_values, count, 0) < 0) goto RET_ERR;
    }
    if (parseInt(str, "dev_attr", 0, 2, &config->dev_attr) < 0) goto RET_ERR;

    // [mipi]
    {
        const char* possible_values[] = { // starts from 1 !!!!!
            "RAW_DATA_8BIT", "RAW_DATA_10BIT", "RAW_DATA_12BIT", "RAW_DATA_14BIT"
        };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "data_type", (void *)&config->data_type,  possible_values, count, 1) < 0) goto RET_ERR;
    }
    if (parseArray(str, "lane_id", &config->lane_id, 8) < 0) goto RET_ERR;

    // [lvds]
    if (parse_config_lvds(str, &config->lvds) < 0) goto RET_ERR;

    // [isp_image]
    if (parseInt(str, "Isp_x", 0, INT_MAX, &config->isp_x) < 0) goto RET_ERR;
    if (parseInt(str, "Isp_y", 0, INT_MAX, &config->isp_y) < 0) goto RET_ERR;
    if (parseInt(str, "Isp_W", 0, INT_MAX, &config->isp_w) < 0) goto RET_ERR;
    if (parseInt(str, "Isp_H", 0, INT_MAX, &config->isp_h) < 0) goto RET_ERR;
    if (parseInt(str, "Isp_FrameRate", 0, INT_MAX, &config->isp_frame_rate) < 0) goto RET_ERR;
    {
        const char* possible_values[] = { "BAYER_RGGB", "BAYER_GRBG", "BAYER_GBRG", "BAYER_BGGR" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        if (parseEnum(str, "isp_bayer", (void *)&config->isp_bayer,  possible_values, count, 0) < 0) goto RET_ERR;
    }

    // [vi_dev]
    if (parse_config_videv(str, config) < 0) goto RET_ERR;
    // [vi_chn]
    if (parse_config_vichn(str, config) < 0) goto RET_ERR;

    free(str); return 0;
    RET_ERR: free(str); return -1;
}
