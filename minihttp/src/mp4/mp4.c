#include "mp4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum BufError create_header();

char buf_sps[128]; uint16_t buf_sps_len = 0;
void set_sps(const char* nal_data, const uint32_t nal_len) {
    memcpy(buf_sps, nal_data, nal_len);
    buf_sps_len = nal_len;
    create_header();
}

char buf_pps[128]; uint16_t buf_pps_len = 0;
void set_pps(const char* nal_data, const uint32_t nal_len) {
    memcpy(buf_pps, nal_data, nal_len);
    buf_pps_len = nal_len;
    create_header();
}

struct BitBuf buf_header;
enum BufError create_header() {
    if (buf_header.offset > 0) return BUF_OK;
    if (buf_sps_len == 0) return BUF_OK;
    if (buf_pps_len == 0) return BUF_OK;

    struct MoovInfo moov_info;
    memset(&moov_info, 0, sizeof(struct MoovInfo));
    moov_info.sps = buf_sps;
    moov_info.sps_length = buf_sps_len;
    moov_info.pps = buf_pps;
    moov_info.pps_length = buf_pps_len;

    buf_header.offset = 0;
    enum BufError err = write_header(&buf_header, &moov_info);  chk_err
    return BUF_OK;
}

enum BufError get_header(struct BitBuf *ptr) {
    ptr->buf =  buf_header.buf;
    ptr->size =  buf_header.size;
    ptr->offset =  buf_header.offset;
    return BUF_OK;
}

struct BitBuf buf_moof;
struct BitBuf buf_mdat;
enum BufError set_slice(const char* nal_data, const uint32_t nal_len) {
    enum BufError err;

    const uint32_t samples_info_len = 1;
    struct SampleInfo samples_info[1];
    memset(&samples_info[0], 0, sizeof(struct SampleInfo));

    buf_moof.offset = 0;
    err = write_moof(&buf_moof, 0, 0, 0, 100, samples_info, samples_info_len); chk_err
    buf_mdat.offset = 0;
    err = write_mdat(&buf_mdat, nal_data, nal_len); chk_err

    return BUF_OK;
}

enum BufError set_mp4_state(struct Mp4State *state) {
    enum BufError err;
    err = put_u32_be_to_offset(&buf_moof, pos_sequence_number, state->sequence_number); chk_err
    err = put_u64_be_to_offset(&buf_moof, pos_base_data_offset, state->base_data_offset); chk_err
    err = put_u64_be_to_offset(&buf_moof, pos_base_media_decode_time, state->base_media_decode_time); chk_err
    return BUF_OK;
}
enum BufError get_moof(struct BitBuf *ptr) {
    ptr->buf =  buf_moof.buf;
    ptr->size =  buf_moof.size;
    ptr->offset =  buf_moof.offset;
    return BUF_OK;
}
enum BufError get_mdat(struct BitBuf *ptr) {
    ptr->buf =  buf_mdat.buf;
    ptr->size =  buf_mdat.size;
    ptr->offset =  buf_mdat.offset;
    return BUF_OK;
}
