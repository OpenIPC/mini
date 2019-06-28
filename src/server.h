#pragma once
#include <stdbool.h>
#include <stdint.h>

extern bool keepRunning;

int start_server();
int stop_server();

void send_jpeg(uint8_t chn_index, char *buf, ssize_t size);
void send_mjpeg(uint8_t chn_index, char *buf, ssize_t size);
void send_h264_to_client(uint8_t chn_index, const void *p);
void send_mp4_to_client(uint8_t chn_index, const void *p);
