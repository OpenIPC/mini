#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define chk_err                                                                \
    if (err != BUF_OK) {                                                       \
        printf(                                                                \
            "Error buf: %s     %s(...)    %s:%d\n", buf_error_to_str(err),     \
            __func__, __FILE__, __LINE__);                                     \
        return err;                                                            \
    }
#define chk_err_continue                                                       \
    if (err != BUF_OK) {                                                       \
        printf(                                                                \
            "Error buf: %s     %s(...)    %s:%d\n", buf_error_to_str(err),     \
            __func__, __FILE__, __LINE__);                                     \
        continue;                                                              \
    }

enum BufError {
    BUF_OK = 0,
    BUF_ENDOFBUF_ERROR,
    BUF_MALLOC_ERROR,
    BUF_INCORRECT
};
char *buf_error_to_str(const enum BufError err);

struct BitBuf {
    char *buf;
    uint32_t size;
    uint32_t offset;
};

enum BufError put_skip(struct BitBuf *ptr, const uint32_t count);
enum BufError put_to_offset(
    struct BitBuf *ptr, const uint32_t offset, const char *data,
    const uint32_t size);
enum BufError put(struct BitBuf *ptr, const char *data, const uint32_t size);
enum BufError
put_u8_to_offset(struct BitBuf *ptr, const uint32_t offset, const uint8_t val);
enum BufError put_u8(struct BitBuf *ptr, uint8_t val);
enum BufError put_u16_be_to_offset(
    struct BitBuf *ptr, const uint32_t offset, const uint16_t val);
enum BufError put_u16_be(struct BitBuf *ptr, const uint16_t val);
enum BufError put_u16_le_to_offset(
    struct BitBuf *ptr, const uint32_t offset, const uint16_t val);
enum BufError put_u16_le(struct BitBuf *ptr, const uint16_t val);
enum BufError put_u32_be_to_offset(
    struct BitBuf *ptr, const uint32_t offset, const uint32_t val);
enum BufError put_u32_be(struct BitBuf *ptr, const uint32_t val);
enum BufError put_i32_be(struct BitBuf *ptr, const int32_t val);
enum BufError put_u64_be_to_offset(
    struct BitBuf *ptr, const uint32_t offset, const uint64_t val);
enum BufError put_u64_be(struct BitBuf *ptr, const uint64_t val);
enum BufError put_u32_le_to_offset(
    struct BitBuf *ptr, const uint32_t offset, const uint32_t val);
enum BufError put_u32_le(struct BitBuf *ptr, const uint32_t val);
enum BufError put_str4_to_offset(
    struct BitBuf *ptr, const uint32_t offset, const char str[4]);
enum BufError put_str4(struct BitBuf *ptr, const char str[4]);
enum BufError put_counted_str_to_offset(
    struct BitBuf *ptr, const uint32_t offset, const char *str,
    const uint32_t len);
enum BufError
put_counted_str(struct BitBuf *ptr, const char *str, const uint32_t len);
