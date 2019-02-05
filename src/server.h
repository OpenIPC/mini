#pragma once
#include <stdbool.h>

extern bool keepRunning;

int start_server();
int stop_server();

void send_jpeg(char *buf, ssize_t size);
void send_mjpeg(char *buf, ssize_t size);
void send_h264_to_client(const void *p);
void send_mp4_to_client(const void *p);
