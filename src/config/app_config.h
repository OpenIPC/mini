#pragma once
#include "config.h"

struct AppConfig {
    // [system]
    char sensor_config[128];

    bool rtsp_enable;
    bool mp4_enable;
    bool mjpg_enable;
    bool jpeg_enable;

    bool osd_enable;
    bool motion_detect_enable;

    uint32_t web_port;
    bool web_enable_static;

    uint32_t isp_thread_stack_size;
    uint32_t venc_stream_thread_stack_size;
    uint32_t web_server_thread_stack_size;


    uint32_t align_width;
    uint32_t max_pool_cnt;
    uint32_t blk_cnt;
};

extern struct AppConfig app_config;

enum ConfigError parse_app_config(const char *path);
