#pragma once
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>


struct JpegData {
    char *buf;
    ssize_t buf_size;
    ssize_t jpeg_size;
};

int32_t InitJPEG();
int32_t DestroyJPEG();
int32_t request_pic(uint32_t width, uint32_t height, uint32_t qfactor, struct JpegData *jpeg_buf);
