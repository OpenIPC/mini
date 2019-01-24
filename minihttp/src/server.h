#pragma once

extern int keepRunning;
extern int write_pump_fd;

extern int mjpeg;

int start_server();
int stop_server();
