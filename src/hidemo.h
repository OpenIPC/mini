#pragma once
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
    VPSS_CHN vpss_chn;

    VENC_CHN jpeg_chn;
    VENC_CHN mjpeg_chn;
    VENC_CHN h264_chn;

    MD_CHN md_chn;
};

int start_sdk(struct SDKState *state);
int stop_sdk(struct SDKState *state);
