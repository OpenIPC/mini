#pragma once
#include <string.h>
#include <regex.h>
#include <stdint.h>
#include <stdbool.h>

const char* getMime(const char *path);
int parseRequestPath(const char *headers, char *path);

int compile_regex(regex_t *r, const char * regex_text);

int Base64encode_len(int len);
int Base64encode(char *encoded, const char *string, int len);

bool get_uint64(char* str, char* pattern, uint64_t *value);
bool get_uint32(char* str, char* pattern, uint32_t *value);
bool get_uint16(char* str, char* pattern, uint16_t *value);
bool get_uint8(char* str, char* pattern, uint8_t *value);

bool startsWith(const char* str, const char* prefix);
