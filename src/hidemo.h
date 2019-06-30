#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <mpi_sys.h>
#include <mpi_vb.h>
#include <mpi_vi.h>
#include <mpi_venc.h>
#include <mpi_vpss.h>
#include <mpi_isp.h>
#include <mpi_ae.h>
#include <mpi_awb.h>
#include <mpi_af.h>

struct SDKState {
    ISP_DEV isp_dev;
    VI_DEV vi_dev;
    VI_CHN vi_chn;
    VPSS_GRP vpss_grp;

    MD_CHN md_chn;
};
extern struct SDKState state;
// extern bool VencEnabled[VENC_MAX_CHN_NUM];

int start_sdk();
int stop_sdk();

uint32_t take_next_free_channel(bool in_main_loop);
bool channel_is_enable(uint32_t channel);
bool channel_main_loop(uint32_t channel);
void set_channel_disable(uint32_t channel);

HI_S32 create_vpss_chn(int vpss_grp, int vpss_chn, uint32_t fps_src, uint32_t fps_dst);
HI_S32 disable_channel(uint32_t vpss_grp, uint32_t channel_id);
