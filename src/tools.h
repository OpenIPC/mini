#pragma once
#include <string.h>
#include <regex.h>

const char* getMime(const char *path);
int parseRequestPath(const char *headers, char *path);

int compile_regex(regex_t *r, const char * regex_text);
