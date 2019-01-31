#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_SECTIONS 16
struct IniConfig {
    char path[256];
    char *str;
    struct Section {
        char name[64];
        int pos;
    } sections[MAX_SECTIONS];
};


enum ConfigError {
    CONFIG_OK = 0,
    CONFIG_SECTION_NOT_FOUND,
    CONFIG_PARAM_NOT_FOUND,
    CONFIG_PARAM_ISNT_NUMBER,
    CONFIG_PARAM_ISNT_IN_RANGE,
    CONFIG_ENUM_INCORRECT_STRING,
    CONFIG_REGEX_ERROR,
};

enum ConfigError find_sections(struct IniConfig *ini);
enum ConfigError section_pos(struct IniConfig *ini, const char *section, int *start_pos, int *end_pos);
enum ConfigError parse_param_value(struct IniConfig *ini, const char *section, const char *param_name, char *param_value);
enum ConfigError parse_enum(struct IniConfig *ini, const char *section, const char *param_name, void *enum_value, const char *possible_values[], const int possible_values_count, const int possible_values_offset);
enum ConfigError parse_int(struct IniConfig *ini, const char *section, const char *param_name, const int min, const int max, int *int_value);
enum ConfigError parse_bool(struct IniConfig *ini, const char *section, const char *param_name, bool *bool_value);
enum ConfigError parse_array(struct IniConfig *ini, const char *section, const char *param_name, int *array, const int array_size);
