#include "app_config.h"
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>


struct AppConfig app_config;

enum ConfigError parse_app_config(const char *path) {
    memset(&app_config, 0, sizeof(struct AppConfig));

    app_config.sensor_config[0] = 0;
    app_config.mjpg_enable = true;
    app_config.mp4_enable = false;
    app_config.rtsp_enable = false;
    app_config.web_port = 8080;

    app_config.isp_thread_stack_size = 16*1024;
    app_config.venc_stream_thread_stack_size = 16*1024;
    app_config.web_server_thread_stack_size = 16*1024;

    app_config.align_width = 64;
    app_config.blk_cnt = 4;
    app_config.max_pool_cnt = 16;

    struct IniConfig ini;
    memset(&ini, 0, sizeof(struct IniConfig));

    // load config file to string
    ini.str = NULL; {
        FILE * file = fopen (path, "rb");
        if (!file) { printf("Can't open file %s\n", path); return -1; }

        fseek(file, 0, SEEK_END);
        size_t length = (size_t)ftell(file);
        fseek(file, 0, SEEK_SET);

        ini.str = malloc(length + 1);
        if (!ini.str) { printf("Can't allocate buf in parse_app_config\n"); fclose(file); return -1; }
        size_t n = fread(ini.str, 1, length, file);
        if (n != length) { printf("Can't read all file %s\n", path); fclose(file); free(ini.str); return -1; }
        fclose (file);
        ini.str[length] = 0;
    }

    enum ConfigError err;
    find_sections(&ini);

    err = parse_param_value(&ini, "system", "sensor_config", app_config.sensor_config); if (err != CONFIG_OK) goto RET_ERR;
    err = parse_bool(&ini, "system", "rtsp_enable", &app_config.rtsp_enable); if (err != CONFIG_OK) goto RET_ERR;
    err = parse_bool(&ini, "system", "mp4_enable", &app_config.mp4_enable); if (err != CONFIG_OK) goto RET_ERR;
    err = parse_bool(&ini, "system", "mjpeg_enable", &app_config.mjpg_enable); if (err != CONFIG_OK) goto RET_ERR;
    err = parse_int(&ini, "system", "web_port", 1, INT_MAX, &app_config.web_port); if(err != CONFIG_OK) goto RET_ERR;

    err = parse_int(&ini, "system", "isp_thread_stack_size", 16*1024, INT_MAX, &app_config.isp_thread_stack_size); if(err != CONFIG_OK) goto RET_ERR;
    err = parse_int(&ini, "system", "venc_stream_thread_stack_size", 16*1024, INT_MAX, &app_config.venc_stream_thread_stack_size); if(err != CONFIG_OK) goto RET_ERR;
    err = parse_int(&ini, "system", "web_server_thread_stack_size", 16*1024, INT_MAX, &app_config.web_server_thread_stack_size); if(err != CONFIG_OK) goto RET_ERR;

    {
        const char* possible_values[] = { "1", "4", "16", "64", "128" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(&ini, "isp", "align_width", &app_config.align_width,  possible_values, count, 0); if(err != CONFIG_OK) goto RET_ERR;
        err = parse_int(&ini, "isp", "align_width", 0, INT_MAX, &app_config.align_width); if(err != CONFIG_OK) goto RET_ERR;
    }
    err = parse_int(&ini, "isp", "max_pool_cnt", 1, INT_MAX, &app_config.max_pool_cnt); if(err != CONFIG_OK) goto RET_ERR;
    {
        const char* possible_values[] = { "4", "10" };
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(&ini, "isp", "blk_cnt", &app_config.blk_cnt,  possible_values, count, 0); if(err != CONFIG_OK) goto RET_ERR;
        err = parse_int(&ini, "isp", "blk_cnt", 4, INT_MAX, &app_config.blk_cnt); if(err != CONFIG_OK) goto RET_ERR;
    }

    free(ini.str); return CONFIG_OK;
    RET_ERR: free(ini.str); return err;
}
