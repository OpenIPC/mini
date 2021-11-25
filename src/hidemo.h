#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <mpi_ae.h>
#include <mpi_af.h>
#include <mpi_awb.h>
#include <mpi_isp.h>
#include <mpi_sys.h>
#include <mpi_vb.h>
#include <mpi_venc.h>
#include <mpi_vi.h>
#include <mpi_vpss.h>

struct SDKState {
    ISP_DEV isp_dev;
    VI_DEV vi_dev;
    VI_CHN vi_chn;

    MD_CHN md_chn;
};
extern struct SDKState state;

int start_sdk();
int stop_sdk();

void set_color2gray(bool color2gray);

uint32_t take_next_free_channel(bool in_main_loop);
bool channel_is_enable(uint32_t channel);
bool channel_main_loop(uint32_t channel);
void set_channel_disable(uint32_t channel);

HI_S32 bind_vpss_venc(VENC_CHN venc_chn);
HI_S32 unbind_vpss_venc(VENC_CHN venc_chn);

HI_S32 create_venc_chn(VENC_CHN venc_chn, uint32_t fps_src, uint32_t fps_dst);
HI_S32 disable_venc_chn(VENC_CHN venc_chn);
