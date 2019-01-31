#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <hi_comm_vi.h>

#include "tools.h"

enum ConfigError find_sections(struct IniConfig *ini) {
    for (unsigned int i = 0; i < MAX_SECTIONS; ++i) ini->sections[i].pos = -1;

    regex_t regex;
    if (compile_regex(&regex, "^[[:space:]]*\\[([a-zA-Z0-9_]+)\\][[:space:]]*") < 0) { printf("compile_regex error\n"); return CONFIG_REGEX_ERROR; };
    size_t n_matches = 2; // We have 1 capturing group + the whole match group
    regmatch_t m[n_matches];

    int section_pos = 0;
    int section_index = 0;
    while (1) {
        if (section_index >= MAX_SECTIONS) break;
        int match = regexec(&regex, ini->str + section_pos, n_matches, m, 0);
        if (match != 0) break;
        int len = sprintf(ini->sections[section_index].name, "%.*s", (int)(m[1].rm_eo - m[1].rm_so), ini->str + section_pos + m[1].rm_so);
        ini->sections[section_index].name[len] = 0;
        section_pos = section_pos + (int)m[1].rm_eo;
        ini->sections[section_index].pos = section_pos;
        section_index++;
    }
    regfree(&regex);
    return CONFIG_OK;
}

enum ConfigError section_pos(struct IniConfig *ini, const char *section, int *start_pos, int *end_pos) {
    for (unsigned int i = 0; i < MAX_SECTIONS; ++i) {
        if (ini->sections[i].pos > 0 && strcmp(ini->sections[i].name, section) == 0) {
            *start_pos = ini->sections[i].pos;
            if (i+1 < MAX_SECTIONS && ini->sections[i+i].pos > 0)
                *end_pos = ini->sections[i+1].pos;
            else {
                *end_pos = -1;
            }
            return CONFIG_OK;
        }
    }
    return CONFIG_SECTION_NOT_FOUND;
}

enum ConfigError parse_param_value(struct IniConfig *ini, const char *section, const char *param_name, char *param_value) {
    int start_pos = 0;
    int end_pos = 0;
    if (strlen(section) > 0) {
        enum ConfigError err = section_pos(ini, section, &start_pos, &end_pos);
        if (err == CONFIG_SECTION_NOT_FOUND) {
            printf("Section '%s' doesn't exists in config '%s'.\n", section, ini->path);
            return err;
        } else if (err != CONFIG_OK) return err;
    }

    regex_t regex;
    char reg_buf[128];
    ssize_t reg_buf_len = sprintf(reg_buf, "^[[:space:]]*%s[[:space:]]*=[[:space:]]*(.[^[:space:];]*)", param_name);
    reg_buf[reg_buf_len] = 0;
    if(compile_regex(&regex, reg_buf) < 0) { printf("compile_regex error\n"); return CONFIG_REGEX_ERROR; };
    size_t n_matches = 2; // We have 1 capturing group + the whole match group
    regmatch_t m[n_matches];
    int match = regexec(&regex, ini->str + start_pos, n_matches, m, 0);
    regfree(&regex);
    if (match > 0 || (end_pos >= 0 && end_pos - start_pos < m[1].rm_so)) {
        printf("Can't find '%s' in section '%s'.\n", param_name, section);
        return CONFIG_PARAM_NOT_FOUND;
    }
    int res = sprintf(param_value, "%.*s", (int)(m[1].rm_eo - m[1].rm_so), ini->str + start_pos + m[1].rm_so);
    // if (res <= 0 ) { return -1; }
    param_value[res] = 0;
    return CONFIG_OK;
}

enum ConfigError parse_enum(struct IniConfig *ini, const char *section, const char *param_name, void *enum_value, const char *possible_values[], const int possible_values_count, const int possible_values_offset) {
    char param_value[64];
    enum ConfigError err = parse_param_value(ini, section, param_name, param_value);
    if (err != CONFIG_OK) return err;

    // try to parse as number
    char* end;
    long res = strtol(param_value, &end, 10);
    if (*end) { res = strtol(param_value, &end, 16); }
    if (!*end) { *(VI_INPUT_MODE_E*)(enum_value) = (VI_INPUT_MODE_E)res; return CONFIG_OK; }

    // try to find value in possible values
    for(unsigned int i = 0; i < possible_values_count; ++i)
        if (strcmp(param_value, possible_values[i])  == 0) { *(VI_INPUT_MODE_E*)(enum_value) = (VI_INPUT_MODE_E)(possible_values_offset+i); return CONFIG_OK; }

    // print error
    printf("Can't parse param '%s' value '%s'. Is not a number and is not in possible values: ", param_name, param_value);
    for(unsigned int i = 0; i < possible_values_count; ++i)  printf("'%s', ", possible_values[i]);
    return CONFIG_ENUM_INCORRECT_STRING;
}
enum ConfigError parse_bool(struct IniConfig *ini, const char *section, const char *param_name, bool *bool_value) {
    const char* possible_values[] = { "0", "1", "false", "true", "n", "y", "no", "yes" };
    const int count = sizeof(possible_values) / sizeof(const char *);
    VI_INPUT_MODE_E val;
    enum ConfigError err = parse_enum(ini, section, param_name, (void *)&val, possible_values, count, 0); if (err != CONFIG_OK) return err;
    if (val == 0 || val == 2 || val == 4 || val == 6) { *bool_value = false; return CONFIG_OK; }
    if (val == 1 || val == 3 || val == 5 || val == 7) { *bool_value = true; return CONFIG_OK; }
    *bool_value = false;
    return CONFIG_OK;
}

enum ConfigError parse_int(struct IniConfig *ini, const char *section, const char *param_name, const int min, const int max, int *int_value) {
    char param_value[64];
    enum ConfigError err = parse_param_value(ini, section, param_name, param_value);
    if (err != CONFIG_OK) return err;

    // try to parse as number
    char* end = NULL;
    long res = strtol(param_value, &end, 10);
    if (*end) { res = strtol(param_value, &end, 16); }
    if (!*end) {
        if (res < min || res > max) {
            printf("Can't parse param '%s' value '%s'. Value '%ld' is not in a range [%d; %d].", param_name, param_value, res, min, max);
            return CONFIG_PARAM_ISNT_IN_RANGE;
        }
        *int_value = (int)res;
        return CONFIG_OK;
    }
    if (strcmp(param_value, "true")  == 0) { *int_value = 1; return CONFIG_OK; }
    if (strcmp(param_value, "TRUE")  == 0) { *int_value = 1; return CONFIG_OK; }
    if (strcmp(param_value, "false")  == 0) { *int_value = 0; return CONFIG_OK; }
    if (strcmp(param_value, "FALSE")  == 0) { *int_value = 0; return CONFIG_OK; }

    printf("Can't parse param '%s' value '%s'. Is not a integer (dec or hex) number.", param_name, param_value);
    return CONFIG_PARAM_ISNT_NUMBER;
}

enum ConfigError parse_array(struct IniConfig *ini, const char *section, const char *param_name, int *array, const int array_size) {
    char param_value[256];
    enum ConfigError err = parse_param_value(ini, section, param_name, param_value);
    if (err != CONFIG_OK) return err;

    return CONFIG_OK;
}
