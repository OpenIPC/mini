#include "app_config.h"
#include <string.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AppConfig app_config;

enum ConfigError parse_app_config(const char *path) {
    memset(&app_config, 0, sizeof(struct AppConfig));

    app_config.sensor_config[0] = 0;
    app_config.jpeg_enable = false;
    app_config.mp4_enable = false;
    app_config.rtsp_enable = false;
    app_config.osd_enable = false;
    app_config.motion_detect_enable = false;

    app_config.mjpeg_enable = false;
    app_config.mjpeg_fps = 15;
    app_config.mjpeg_width = 640;
    app_config.mjpeg_height = 480;
    app_config.mjpeg_bitrate = 1024;

    app_config.web_port = 8080;
    app_config.web_enable_static = false;

    app_config.isp_thread_stack_size = 16 * 1024;
    app_config.venc_stream_thread_stack_size = 16 * 1024;
    app_config.web_server_thread_stack_size = 16 * 1024;

    app_config.align_width = 64;
    app_config.blk_cnt = 4;
    app_config.max_pool_cnt = 16;

    app_config.night_mode_enable = false;
    app_config.ir_sensor_pin = 999;
    app_config.ir_cut_pin1 = 999;
    app_config.ir_cut_pin2 = 999;
    app_config.pin_switch_delay_us = 250;
    app_config.check_interval_s = 10;

    struct IniConfig ini;
    memset(&ini, 0, sizeof(struct IniConfig));

    // load config file to string
    ini.str = NULL;
    {
        char config_path[50];
        ssize_t len = sprintf(config_path, "/etc/%s", path);
        FILE *file = fopen("./mini.ini", "rb");
        if (!file) {
            file = fopen("/etc/mini.ini", "rb");
            if (!file) {
                printf(
                    "Can't find config mini.ini in:\n    ./mini.ini\n  "
                    "  /etc/mini.ini\n",
                    path);
                return -1;
            }
        }

        fseek(file, 0, SEEK_END);
        size_t length = (size_t)ftell(file);
        fseek(file, 0, SEEK_SET);

        ini.str = malloc(length + 1);
        if (!ini.str) {
            printf("Can't allocate buf in parse_app_config\n");
            fclose(file);
            return -1;
        }
        size_t n = fread(ini.str, 1, length, file);
        if (n != length) {
            printf("Can't read all file %s\n", path);
            fclose(file);
            free(ini.str);
            return -1;
        }
        fclose(file);
        ini.str[length] = 0;
    }

    enum ConfigError err;
    find_sections(&ini);

    err = parse_param_value(
        &ini, "system", "sensor_config", app_config.sensor_config);
    if (err != CONFIG_OK)
        goto RET_ERR;
    err =
        parse_int(&ini, "system", "web_port", 1, INT_MAX, &app_config.web_port);
    if (err != CONFIG_OK)
        goto RET_ERR;
    err = parse_bool(
        &ini, "system", "web_enable_static", &app_config.web_enable_static);
    if (err != CONFIG_OK)
        goto RET_ERR;
    err = parse_int(
        &ini, "system", "isp_thread_stack_size", 16 * 1024, INT_MAX,
        &app_config.isp_thread_stack_size);
    if (err != CONFIG_OK)
        goto RET_ERR;
    err = parse_int(
        &ini, "system", "venc_stream_thread_stack_size", 16 * 1024, INT_MAX,
        &app_config.venc_stream_thread_stack_size);
    if (err != CONFIG_OK)
        goto RET_ERR;
    err = parse_int(
        &ini, "system", "web_server_thread_stack_size", 16 * 1024, INT_MAX,
        &app_config.web_server_thread_stack_size);
    if (err != CONFIG_OK)
        goto RET_ERR;

    err =
        parse_bool(&ini, "night_mode", "enable", &app_config.night_mode_enable);
    if (err != CONFIG_OK)
        goto RET_ERR;
    if (app_config.night_mode_enable) {
#define PIN_MAX 95
        err = parse_int(
            &ini, "night_mode", "ir_sensor_pin", 0, PIN_MAX,
            &app_config.ir_sensor_pin);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "night_mode", "check_interval_s", 0, 600,
            &app_config.check_interval_s);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "night_mode", "ir_cut_pin1", 0, PIN_MAX,
            &app_config.ir_cut_pin1);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "night_mode", "ir_cut_pin2", 0, PIN_MAX,
            &app_config.ir_cut_pin2);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "night_mode", "pin_switch_delay_us", 0, 1000,
            &app_config.pin_switch_delay_us);
        if (err != CONFIG_OK)
            goto RET_ERR;
    }

    {
        const char *possible_values[] = {"1", "4", "16", "64", "128"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            &ini, "isp", "align_width", &app_config.align_width,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "isp", "align_width", 0, INT_MAX, &app_config.align_width);
        if (err != CONFIG_OK)
            goto RET_ERR;
    }
    err = parse_int(
        &ini, "isp", "max_pool_cnt", 1, INT_MAX, &app_config.max_pool_cnt);
    if (err != CONFIG_OK)
        goto RET_ERR;
    {
        const char *possible_values[] = {"4", "10"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            &ini, "isp", "blk_cnt", &app_config.blk_cnt, possible_values, count,
            0);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err =
            parse_int(&ini, "isp", "blk_cnt", 4, INT_MAX, &app_config.blk_cnt);
        if (err != CONFIG_OK)
            goto RET_ERR;
    }

    err = parse_bool(&ini, "rtsp", "enable", &app_config.rtsp_enable);
    if (err != CONFIG_OK)
        goto RET_ERR;

    err = parse_bool(&ini, "mp4", "enable", &app_config.mp4_enable);
    if (err != CONFIG_OK)
        goto RET_ERR;
    if (app_config.mp4_enable) {
        err = parse_int(
            &ini, "mp4", "width", 160, INT_MAX, &app_config.mp4_width);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "mp4", "height", 120, INT_MAX, &app_config.mp4_height);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(&ini, "mp4", "fps", 1, INT_MAX, &app_config.mp4_fps);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "mp4", "bitrate", 32, INT_MAX, &app_config.mp4_bitrate);
        if (err != CONFIG_OK)
            goto RET_ERR;
    }

    err = parse_bool(&ini, "jpeg", "enable", &app_config.jpeg_enable);
    if (err != CONFIG_OK)
        goto RET_ERR;
    if (app_config.jpeg_enable) {
        err = parse_int(
            &ini, "jpeg", "width", 160, INT_MAX, &app_config.jpeg_width);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "jpeg", "height", 120, INT_MAX, &app_config.jpeg_height);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err =
            parse_int(&ini, "jpeg", "qfactor", 1, 99, &app_config.jpeg_qfactor);
        if (err != CONFIG_OK)
            goto RET_ERR;
    }

    err = parse_bool(&ini, "mjpeg", "enable", &app_config.mjpeg_enable);
    if (err != CONFIG_OK)
        goto RET_ERR;
    if (app_config.mjpeg_enable) {
        err = parse_int(
            &ini, "mjpeg", "width", 160, INT_MAX, &app_config.mjpeg_width);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "mjpeg", "height", 120, INT_MAX, &app_config.mjpeg_height);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err =
            parse_int(&ini, "mjpeg", "fps", 1, INT_MAX, &app_config.mjpeg_fps);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "mjpeg", "bitrate", 32, INT_MAX, &app_config.mjpeg_bitrate);
        if (err != CONFIG_OK)
            goto RET_ERR;
    }

    parse_bool(&ini, "http_post", "enable", &app_config.http_post_enable);
    if (app_config.http_post_enable) {
        err = parse_param_value(
            &ini, "http_post", "host", &app_config.http_post_host);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_param_value(
            &ini, "http_post", "url", &app_config.http_post_url);
        if (err != CONFIG_OK)
            goto RET_ERR;

        err = parse_param_value(
            &ini, "http_post", "login", &app_config.http_post_login);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_param_value(
            &ini, "http_post", "password", &app_config.http_post_password);
        if (err != CONFIG_OK)
            goto RET_ERR;

        err = parse_int(
            &ini, "http_post", "width", 160, INT_MAX,
            &app_config.http_post_width);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "http_post", "height", 120, INT_MAX,
            &app_config.http_post_height);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "http_post", "interval", 1, INT_MAX,
            &app_config.http_post_interval);
        if (err != CONFIG_OK)
            goto RET_ERR;
        err = parse_int(
            &ini, "http_post", "qfactor", 1, 99, &app_config.http_post_qfactor);
        if (err != CONFIG_OK)
            goto RET_ERR;
    }

    free(ini.str);
    return CONFIG_OK;
RET_ERR:
    free(ini.str);
    return err;
}
