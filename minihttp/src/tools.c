#include "tools.h"

#include <stdio.h>
#include <stdlib.h>
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
    int status = regcomp(r, regex_text, REG_EXTENDED|REG_NEWLINE|REG_ICASE);
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
