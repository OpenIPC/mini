#pragma once

extern int keepRunning;
extern int write_pump_h264_fd;
extern int write_pump_mjpeg_fd;

int start_server();
int stop_server();
