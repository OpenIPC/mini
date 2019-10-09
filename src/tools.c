#include "tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <regex.h>

const char* getExt(const char *path) {
    const char *dot = strrchr(path, '.');
    if(!dot || dot == path) return "";
    return dot + 1;
}

const char* getMime(const char *path) {
    const char *ext = getExt(path);
    if (strlen(ext) == 0) return "";
    if (strncmp(ext, "html", strlen("html")) == 0) {        return "text/html"; }
    else if (strncmp(ext, "css", strlen("css")) == 0) {     return "text/css"; }
    else if (strncmp(ext, "js", strlen("js")) == 0) {       return "application/javascript"; }
    else if (strncmp(ext, "json", strlen("json")) == 0) {   return "application/json"; }
    else if (strncmp(ext, "jpg", strlen("jpg")) == 0) {     return "image/jpeg"; }
    else if (strncmp(ext, "jpeg", strlen("jpeg")) == 0) {   return "image/jpeg"; }
    else if (strncmp(ext, "gif", strlen("gif")) == 0) {     return "image/gif"; }
    else if (strncmp(ext, "png", strlen("png")) == 0) {     return "image/png"; }
    else if (strncmp(ext, "svg", strlen("svg")) == 0) {     return "image/svg+xml"; }
    else if (strncmp(ext, "mp4", strlen("mp4")) == 0) {     return "video/mp4"; }
    return "";
}


#define MAX_ERROR_MSG 0x1000
int compile_regex(regex_t * r, const char * regex_text) {
    int status = regcomp(r, regex_text, REG_EXTENDED | REG_NEWLINE | REG_ICASE);
    if (status != 0) {
        char error_message[MAX_ERROR_MSG];
        regerror(status, r, error_message, MAX_ERROR_MSG);
        printf("Regex error compiling '%s': %s\n", regex_text, error_message); fflush(stdout);
        return -1;
    }
    return 1;
}

int parseRequestPath(const char *headers, char *path) {
    regex_t regex;
    compile_regex (&regex, "^GET (/.*) HTTP");
    size_t n_matches = 2; // We have 1 capturing group + the whole match group
    regmatch_t m[n_matches];
    const char * p = headers;
    int match = regexec(&regex, p, n_matches, m, 0);
    regfree(&regex);
    if (match) { return -1; }
    sprintf(path, ".%.*s", (int)(m[1].rm_eo - m[1].rm_so), &headers[m[1].rm_so]);
    return 1;
}



/* aaaack but it's fast and const should make it shared text page. */
static const unsigned char pr2six[256] = {
    /* ASCII table */
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

static const char basis_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int Base64encode_len(int len) {
    return ((len + 2) / 3 * 4) + 1;
}

int Base64encode(char *encoded, const char *string, int len) {
    int i;
    char *p;

    p = encoded;
    for (i = 0; i < len - 2; i += 3) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        *p++ = basis_64[((string[i] & 0x3) << 4) |
                        ((int) (string[i + 1] & 0xF0) >> 4)];
        *p++ = basis_64[((string[i + 1] & 0xF) << 2) |
                        ((int) (string[i + 2] & 0xC0) >> 6)];
        *p++ = basis_64[string[i + 2] & 0x3F];
    }
    if (i < len) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        if (i == (len - 1)) {
            *p++ = basis_64[((string[i] & 0x3) << 4)];
            *p++ = '=';
        }
        else {
            *p++ = basis_64[((string[i] & 0x3) << 4) |
                            ((int) (string[i + 1] & 0xF0) >> 4)];
            *p++ = basis_64[((string[i + 1] & 0xF) << 2)];
        }
        *p++ = '=';
    }

    *p++ = '\0';
    return p - encoded;
}

bool startsWith(const char* str, const char* prefix) {
    size_t lenpre = strlen(prefix), lenstr = strlen(str);
    return lenstr < lenpre ? false : memcmp(prefix, str, lenpre) == 0;
}

bool get_uint64(char* str, char* pattern, uint64_t *value) {
    char reg_buf[128];
    ssize_t reg_buf_len = sprintf(reg_buf, "%s([0-9]+)", pattern);
    reg_buf[reg_buf_len] = 0;

    regex_t regex;
    if (compile_regex(&regex, reg_buf) < 0) { printf("get_uint64 compile_regex error\n"); return false; };
    size_t n_matches = 2; // We have 1 capturing group + the whole match group
    regmatch_t m[n_matches];

    char value_str[16] = {0};
    int match = regexec(&regex, str, n_matches, m, 0);
    if (match != 0) return false;
    int len = sprintf(value_str, "%.*s", (int)(m[1].rm_eo - m[1].rm_so), str + m[1].rm_so);
    value_str[len] = 0;

    char* pEnd;
    *value = strtoll(value_str, &pEnd, 10);
    regfree(&regex);

    return true;
}
bool get_uint32(char* str, char* pattern, uint32_t *value) {
    uint64_t val64 = 0;
    if (!get_uint64(str, pattern, &val64)) return false;
    *value = val64;
    return true;
}
bool get_uint16(char* str, char* pattern, uint16_t *value) {
    uint64_t val64 = 0;
    if (!get_uint64(str, pattern, &val64)) return false;
    *value = val64;
    return true;
}
bool get_uint8(char* str, char* pattern, uint8_t *value) {
    uint64_t val64 = 0;
    if (!get_uint64(str, pattern, &val64)) return false;
    *value = val64;
    return true;
}

