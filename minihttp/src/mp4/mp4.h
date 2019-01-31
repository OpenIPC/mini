#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "bitbuf.h"
#include "moov.h"
#include "moof.h"
#include "nal.h"

struct Mp4State {
    bool header_sent;

    uint32_t sequence_number;
    uint64_t base_data_offset;
    uint64_t base_media_decode_time;
    uint32_t default_sample_duration;
};

enum BufError set_slice(const char* nal_data, const uint32_t nal_len);
void set_sps(const char* nal_data, const uint32_t nal_len);
void set_pps(const char* nal_data, const uint32_t nal_len);

enum BufError get_header(struct BitBuf *ptr);

enum BufError set_mp4_state(struct Mp4State *state);
enum BufError get_moof(struct BitBuf *ptr);
enum BufError get_mdat(struct BitBuf *ptr);



