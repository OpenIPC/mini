#pragma once
#include <stdbool.h>
#include <stdint.h>

extern bool keepRunning;

// void http_post_send_jpeg(uint8_t chn_index, char *buf, ssize_t size);
void start_http_post_send();
