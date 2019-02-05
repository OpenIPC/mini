#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "bitbuf.h"
#include "moov.h"
#include "moof.h"
#include "nal.h"

extern uint32_t default_sample_size;

struct Mp4State {
    bool header_sent;

    uint32_t sequence_number;
    uint64_t base_data_offset;
    uint64_t base_media_decode_time;
    uint32_t default_sample_duration;

    uint32_t nals_count;
};

struct Mp4Context {
    char buf_sps[128];
    uint16_t buf_sps_len;
    char buf_pps[128];
    uint16_t buf_pps_len;

    struct BitBuf buf_header;
    struct BitBuf buf_moof;
    struct BitBuf buf_mdat;
};


enum BufError set_slice(struct Mp4Context *ctx, const char* nal_data, const uint32_t nal_len, const enum NalUnitType unit_type);
void set_sps(struct Mp4Context *ctx, const char* nal_data, const uint32_t nal_len);
void set_pps(struct Mp4Context *ctx, const char* nal_data, const uint32_t nal_len);

enum BufError get_header(struct Mp4Context *ctx, struct BitBuf *ptr);

enum BufError set_mp4_state(struct Mp4Context *ctx, struct Mp4State *state);
enum BufError get_moof(struct Mp4Context *ctx, struct BitBuf *ptr);
enum BufError get_mdat(struct Mp4Context *ctx, struct BitBuf *ptr);



