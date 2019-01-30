#pragma once

extern int keepRunning;

int start_server();
int stop_server();

void send_jpeg(char *buf, ssize_t size);
void send_mjpeg(char *buf, ssize_t size);
