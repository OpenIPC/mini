#pragma once
#include <string.h>

const char* getMime(const char *path);
int parseRequestPath(const char *headers, char *path);
