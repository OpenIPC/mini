#include "mp4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/app_config.h"
#include "config/sensor_config.h"

uint32_t default_sample_size = 40000;

enum BufError create_header();

void set_sps(
    struct Mp4Context *ctx, const char *nal_data, const uint32_t nal_len) {
    memcpy(ctx->buf_sps, nal_data, nal_len);
    ctx->buf_sps_len = nal_len;
    create_header(ctx);
}

void set_pps(
    struct Mp4Context *ctx, const char *nal_data, const uint32_t nal_len) {
    memcpy(ctx->buf_pps, nal_data, nal_len);
    ctx->buf_pps_len = nal_len;
    create_header(ctx);
}

enum BufError create_header(struct Mp4Context *ctx) {
    if (ctx->buf_header.offset > 0)
        return BUF_OK;
    if (ctx->buf_sps_len == 0)
        return BUF_OK;
    if (ctx->buf_pps_len == 0)
        return BUF_OK;

    struct MoovInfo moov_info;
    memset(&moov_info, 0, sizeof(struct MoovInfo));
    moov_info.profile_idc = 100;
    moov_info.level_idc = 41;
    moov_info.width = sensor_config.isp.isp_w;
    moov_info.height = sensor_config.isp.isp_h;
    moov_info.horizontal_resolution = 0x00480000; // 72 dpi
    moov_info.vertical_resolution = 0x00480000;   // 72 dpi
    moov_info.creation_time = 0;
    moov_info.timescale =
        default_sample_size * sensor_config.isp.isp_frame_rate;
    moov_info.sps = ctx->buf_sps;
    moov_info.sps_length = ctx->buf_sps_len;
    moov_info.pps = ctx->buf_pps;
    moov_info.pps_length = ctx->buf_pps_len;

    ctx->buf_header.offset = 0;
    enum BufError err = write_header(&ctx->buf_header, &moov_info);
    chk_err return BUF_OK;
}

enum BufError get_header(struct Mp4Context *ctx, struct BitBuf *ptr) {
    ptr->buf = ctx->buf_header.buf;
    ptr->size = ctx->buf_header.size;
    ptr->offset = ctx->buf_header.offset;
    return BUF_OK;
}

enum BufError set_slice(
    struct Mp4Context *ctx, const char *nal_data, const uint32_t nal_len,
    const enum NalUnitType unit_type) {
    enum BufError err;

    const uint32_t samples_info_len = 1;
    struct SampleInfo samples_info[1];
    memset(&samples_info[0], 0, sizeof(struct SampleInfo));
    samples_info[0].size = nal_len + 4; // add size of sample
    samples_info[0].composition_offset = default_sample_size;
    samples_info[0].decode_time = default_sample_size;
    samples_info[0].duration = default_sample_size;
    samples_info[0].flags = unit_type == NalUnitType_CodedSliceIdr ? 0 : 65536;

    ctx->buf_moof.offset = 0;
    err = write_moof(
        &ctx->buf_moof, 0, 0, 0, default_sample_size, samples_info,
        samples_info_len);
    chk_err

        ctx->buf_mdat.offset = 0;

    err = write_mdat(&ctx->buf_mdat, nal_data, nal_len);
    chk_err

        return BUF_OK;
}

enum BufError set_mp4_state(struct Mp4Context *ctx, struct Mp4State *state) {
    enum BufError err;
    if (pos_sequence_number > 0)
        err = put_u32_be_to_offset(
            &ctx->buf_moof, pos_sequence_number, state->sequence_number);
    chk_err if (pos_base_data_offset > 0) err = put_u64_be_to_offset(
        &ctx->buf_moof, pos_base_data_offset, state->base_data_offset);
    chk_err if (pos_base_media_decode_time > 0) err = put_u64_be_to_offset(
        &ctx->buf_moof, pos_base_media_decode_time,
        state->base_media_decode_time);
    chk_err state->sequence_number++;
    state->base_data_offset += ctx->buf_moof.offset + ctx->buf_mdat.offset;
    state->base_media_decode_time += state->default_sample_duration;
    return BUF_OK;
}
enum BufError get_moof(struct Mp4Context *ctx, struct BitBuf *ptr) {
    ptr->buf = ctx->buf_moof.buf;
    ptr->size = ctx->buf_moof.size;
    ptr->offset = ctx->buf_moof.offset;
    return BUF_OK;
}
enum BufError get_mdat(struct Mp4Context *ctx, struct BitBuf *ptr) {
    ptr->buf = ctx->buf_mdat.buf;
    ptr->size = ctx->buf_mdat.size;
    ptr->offset = ctx->buf_mdat.offset;
    return BUF_OK;
}
