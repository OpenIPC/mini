#include "hidemo.h"

#include "compat.h"

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <mpi_ae.h>
#include <mpi_af.h>
#include <mpi_awb.h>
#include <mpi_isp.h>
#include <mpi_region.h>
#include <mpi_sys.h>
#include <mpi_vb.h>
#include <mpi_venc.h>
#include <mpi_vi.h>
#include <mpi_vpss.h>

#include "config/app_config.h"
#include "config/sensor_config.h"
#include "hi_jpeg.h"
#include "hierrors.h"
#include "http_post.h"
#include "motion_detect.h"
#include "rtsp/ringfifo.h"
#include "rtsp/rtputils.h"
#include "rtsp/rtspservice.h"
#include "sensor.h"
#include "server.h"

#if HISILICON_SDK_GEN <= 2
#define MPEG_ATTR stAttrMjpeg
#elif HISILICON_SDK_GEN == 3
#define MPEG_ATTR stAttrMjpege
#endif

#define MIPI_DEV "/dev/hi_mipi"

struct SDKState state;

HI_S32 HI_MPI_SYS_GetChipId(HI_U32 *pu32ChipId);

HI_VOID *Test_ISP_Run(HI_VOID *param) {
    ISP_DEV isp_dev = 0;
    HI_S32 s32Ret = HI_MPI_ISP_Run(isp_dev);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_ISP_Run failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret));
    }
    printf("Shutdown isp_run thread\n");
    return HI_NULL;
}

HI_S32 VENC_SaveH264(int chn_index, VENC_STREAM_S *pstStream) {
    if (app_config.mp4_enable)
        send_mp4_to_client(chn_index, pstStream);
    if (app_config.mp4_enable)
        send_h264_to_client(chn_index, pstStream);
    if (app_config.rtsp_enable)
        HisiPutH264DataToBuffer(pstStream);
    return HI_SUCCESS;
}

HI_S32 VENC_SaveJpeg(int chn_index, VENC_STREAM_S *pstStream) {
    static char *jpeg_buf;
    static ssize_t jpeg_buf_size = 0;
    ssize_t buf_size = 0;
    for (HI_U32 i = 0; i < pstStream->u32PackCount; i++) {
        VENC_PACK_S *pstData = &pstStream->pstPack[i];
        ssize_t need_size = buf_size + pstData->u32Len - pstData->u32Offset + 2;
        if (need_size > jpeg_buf_size) {
            jpeg_buf = realloc(jpeg_buf, need_size);
            jpeg_buf_size = need_size;
        }
        memcpy(
            jpeg_buf + buf_size, pstData->pu8Addr + pstData->u32Offset,
            pstData->u32Len - pstData->u32Offset);
        buf_size += pstData->u32Len - pstData->u32Offset;
    }
    if (app_config.jpeg_enable)
        send_jpeg(chn_index, jpeg_buf, buf_size);
    return HI_SUCCESS;
}

HI_S32 VENC_SaveMJpeg(int chn_index, VENC_STREAM_S *pstStream) {
    if (app_config.mjpeg_enable) {
        static char *mjpeg_buf;
        static ssize_t mjpeg_buf_size = 0;
        ssize_t buf_size = 0;
        for (HI_U32 i = 0; i < pstStream->u32PackCount; i++) {
            VENC_PACK_S *pstData = &pstStream->pstPack[i];
            ssize_t need_size =
                buf_size + pstData->u32Len - pstData->u32Offset + 2;
            if (need_size > mjpeg_buf_size) {
                mjpeg_buf = realloc(mjpeg_buf, need_size);
                mjpeg_buf_size = need_size;
            }
            memcpy(
                mjpeg_buf + buf_size, pstData->pu8Addr + pstData->u32Offset,
                pstData->u32Len - pstData->u32Offset);
            buf_size += pstData->u32Len - pstData->u32Offset;
        }
        send_mjpeg(chn_index, mjpeg_buf, buf_size);
    }
    return HI_SUCCESS;
}

HI_S32 VENC_SaveStream(
    int chn_index, PAYLOAD_TYPE_E enType, VENC_STREAM_S *pstStream) {
    HI_S32 s32Ret;
    if (PT_H264 == enType) {
        s32Ret = VENC_SaveH264(chn_index, pstStream);
    } else if (PT_MJPEG == enType) {
        s32Ret = VENC_SaveMJpeg(chn_index, pstStream);
    } else if (PT_JPEG == enType) {
        s32Ret = VENC_SaveJpeg(chn_index, pstStream);
    } else {
        return HI_FAILURE;
    }
    return s32Ret;
}

pthread_mutex_t mutex;

struct ChnInfo {
    bool enable;
    bool in_main_loop;
};

struct ChnInfo VencS[VENC_MAX_CHN_NUM] = {0};

uint32_t take_next_free_channel(bool in_main_loop) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < VENC_MAX_CHN_NUM; i++) {
        if (!VencS[i].enable) {
            VencS[i].enable = true;
            VencS[i].in_main_loop = in_main_loop;
            pthread_mutex_unlock(&mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1;
}

bool channel_is_enable(uint32_t channel) {
    pthread_mutex_lock(&mutex);
    bool enable = VencS[channel].enable;
    pthread_mutex_unlock(&mutex);
    return enable;
}

bool channel_main_loop(uint32_t channel) {
    pthread_mutex_lock(&mutex);
    bool in_main_loop = VencS[channel].in_main_loop;
    pthread_mutex_unlock(&mutex);
    return in_main_loop;
}

void set_channel_disable(uint32_t channel) {
    pthread_mutex_lock(&mutex);
    VencS[channel].enable = false;
    pthread_mutex_unlock(&mutex);
}

void set_color2gray(bool color2gray) {
    pthread_mutex_lock(&mutex);
    for (int venc_chn = 0; venc_chn < VENC_MAX_CHN_NUM; venc_chn++) {
        if (VencS[venc_chn].enable) {
            VENC_COLOR2GREY_S pstChnColor2Grey;
            pstChnColor2Grey.bColor2Grey = color2gray;
            HI_S32 s32Ret =
                HI_MPI_VENC_SetColor2Grey(venc_chn, &pstChnColor2Grey);
        }
    }
    pthread_mutex_unlock(&mutex);
}

HI_VOID *VENC_GetVencStreamProc(HI_VOID *p) {
    HI_S32 maxfd = 0;
    HI_S32 s32Ret;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];

    HI_S32 VencFd[VENC_MAX_CHN_NUM] = {0};

    for (HI_S32 chn_index = 0; chn_index < VENC_MAX_CHN_NUM; chn_index++) {
        if (!channel_is_enable(chn_index))
            continue;
        if (!channel_main_loop(chn_index))
            continue;

        VENC_CHN_ATTR_S stVencChnAttr;
        s32Ret = HI_MPI_VENC_GetChnAttr(chn_index, &stVencChnAttr);
        if (s32Ret != HI_SUCCESS) {
            printf(
                "HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n%s\n",
                chn_index, s32Ret, hi_errstr(s32Ret));
            return NULL;
        }
        enPayLoadType[chn_index] = stVencChnAttr.stVeAttr.enType;

        VencFd[chn_index] = HI_MPI_VENC_GetFd(chn_index);
        if (VencFd[chn_index] < 0) {
            printf(
                "HI_MPI_VENC_GetFd chn[%d] failed with %#x!\n%s\n", chn_index,
                VencFd[chn_index], hi_errstr(VencFd[chn_index]));
            return NULL;
        }
        if (maxfd <= VencFd[chn_index]) {
            maxfd = VencFd[chn_index];
        }
    }

    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    struct timeval TimeoutVal;
    fd_set read_fds;
    while (keepRunning) {
        FD_ZERO(&read_fds);
        for (HI_S32 chn_index = 0; chn_index < VENC_MAX_CHN_NUM; chn_index++) {
            if (!channel_is_enable(chn_index))
                continue;
            if (!channel_main_loop(chn_index))
                continue;

            FD_SET(VencFd[chn_index], &read_fds);
        }

        TimeoutVal.tv_sec = 2;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0) {
            printf("select failed!\n");
            break;
        } else if (s32Ret == 0) {
            printf("get main loop stream time out, exit thread\n");
            continue;
        } else {
            for (HI_S32 chn_index = 0; chn_index < VENC_MAX_CHN_NUM;
                 chn_index++) {
                if (!channel_is_enable(chn_index))
                    continue;
                if (!channel_main_loop(chn_index))
                    continue;

                if (FD_ISSET(VencFd[chn_index], &read_fds)) {
                    // printf("fd_was_set! chn: %d\n", chn_index);
                    memset(&stStream, 0, sizeof(stStream));
                    s32Ret = HI_MPI_VENC_Query(chn_index, &stStat);
                    if (HI_SUCCESS != s32Ret) {
                        printf(
                            "HI_MPI_VENC_Query chn[%d] failed with %#x!\n%s\n",
                            chn_index, s32Ret, hi_errstr(s32Ret));
                        break;
                    }

                    if (0 == stStat.u32CurPacks) {
                        printf("NOTE: Current frame is NULL!\n");
                        continue;
                    }

                    stStream.pstPack = (VENC_PACK_S *)malloc(
                        sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack) {
                        printf(
                            "malloc stream chn[%d] pack failed!\n", chn_index);
                        break;
                    }
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret =
                        HI_MPI_VENC_GetStream(chn_index, &stStream, HI_TRUE);
                    if (HI_SUCCESS != s32Ret) {
                        printf(
                            "HI_MPI_VENC_GetStream chn[%d] failed with "
                            "%#x!\n%s\n",
                            chn_index, s32Ret, hi_errstr(s32Ret));
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }
                    s32Ret = VENC_SaveStream(
                        chn_index, enPayLoadType[chn_index], &stStream);
                    if (HI_SUCCESS != s32Ret) {
                        printf(
                            "VENC_SaveStream chn[%d] failed with %#x!\n%s\n",
                            chn_index, s32Ret, hi_errstr(s32Ret));
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }
                    s32Ret = HI_MPI_VENC_ReleaseStream(chn_index, &stStream);
                    if (HI_SUCCESS != s32Ret) {
                        printf(
                            "HI_MPI_VENC_ReleaseStream chn[%d] failed with "
                            "%#x!\n%s\n",
                            chn_index, s32Ret, hi_errstr(s32Ret));
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                }
            }
        }
    }
    printf("Shutdown hisdk venc thread\n");
    return NULL;
}

HI_S32 create_venc_chn(VENC_CHN venc_chn, uint32_t fps_src, uint32_t fps_dst) {
    VPSS_GRP vpss_grp = venc_chn / VPSS_MAX_CHN_NUM;
    VPSS_CHN vpss_chn = venc_chn - vpss_grp * VPSS_MAX_CHN_NUM;
    printf(
        "new venc: %d   vpss_grp: %d,   vpss_chn: %d\n", venc_chn, vpss_grp,
        vpss_chn);

    VPSS_CHN_ATTR_S vpss_chn_attr;
    memset(&vpss_chn_attr, 0, sizeof(VPSS_CHN_ATTR_S));
    vpss_chn_attr.bSpEn = HI_FALSE;
    vpss_chn_attr.bBorderEn = HI_FALSE;
    vpss_chn_attr.bMirror = HI_FALSE;
    vpss_chn_attr.bFlip = HI_FALSE;
    vpss_chn_attr.s32SrcFrameRate = fps_src;
    vpss_chn_attr.s32DstFrameRate = fps_dst;
    vpss_chn_attr.stBorder.u32TopWidth = 0;
    vpss_chn_attr.stBorder.u32BottomWidth = 0;
    vpss_chn_attr.stBorder.u32LeftWidth = 0;
    vpss_chn_attr.stBorder.u32RightWidth = 0;
    vpss_chn_attr.stBorder.u32Color = 0;
    HI_S32 s32Ret = HI_MPI_VPSS_SetChnAttr(vpss_grp, vpss_chn, &vpss_chn_attr);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VPSS_SetChnAttr(%d, %d, ...) failed with %#x!\n%s\n",
            vpss_grp, vpss_chn, s32Ret, hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    VPSS_CHN_MODE_S vpss_chn_mode;
    memset(&vpss_chn_mode, 0, sizeof(VPSS_CHN_MODE_S));
    vpss_chn_mode.enChnMode = VPSS_CHN_MODE_USER;
    vpss_chn_mode.u32Width = sensor_config.vichn.dest_size_width;
    vpss_chn_mode.u32Height = sensor_config.vichn.dest_size_height;
    vpss_chn_mode.bDouble = HI_FALSE;
    vpss_chn_mode.enPixelFormat = sensor_config.vichn.pix_format;
    vpss_chn_mode.enCompressMode = sensor_config.vichn.compress_mode;
    s32Ret = HI_MPI_VPSS_SetChnMode(vpss_grp, vpss_chn, &vpss_chn_mode);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VPSS_SetChnMode failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    s32Ret = HI_MPI_VPSS_EnableChn(vpss_grp, vpss_chn);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VPSS_EnableChn failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    MPP_CHN_S src_chn;
    memset(&src_chn, 0, sizeof(MPP_CHN_S));
    src_chn.enModId = HI_ID_VPSS;
    src_chn.s32DevId = vpss_grp;
    src_chn.s32ChnId = vpss_chn;
    MPP_CHN_S dest_chn;
    memset(&dest_chn, 0, sizeof(MPP_CHN_S));
    dest_chn.enModId = HI_ID_VENC;
    dest_chn.s32DevId = 0;
    dest_chn.s32ChnId = venc_chn;
    s32Ret = HI_MPI_SYS_Bind(&src_chn, &dest_chn);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_SYS_Bind failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 bind_vpss_venc(VENC_CHN venc_chn) {
    VPSS_GRP vpss_grp = venc_chn / VPSS_MAX_CHN_NUM;
    VPSS_CHN vpss_chn = venc_chn - vpss_grp * VPSS_MAX_CHN_NUM;

    HI_S32 s32Ret = HI_MPI_VPSS_EnableChn(vpss_grp, vpss_chn);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VPSS_EnableChn failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    MPP_CHN_S src_chn;
    memset(&src_chn, 0, sizeof(MPP_CHN_S));
    src_chn.enModId = HI_ID_VPSS;
    src_chn.s32DevId = vpss_grp;
    src_chn.s32ChnId = vpss_chn;
    MPP_CHN_S dest_chn;
    memset(&dest_chn, 0, sizeof(MPP_CHN_S));
    dest_chn.enModId = HI_ID_VENC;
    dest_chn.s32DevId = 0;
    dest_chn.s32ChnId = venc_chn;
    s32Ret = HI_MPI_SYS_Bind(&src_chn, &dest_chn);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_SYS_Bind failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 unbind_vpss_venc(VENC_CHN venc_chn) {
    VPSS_GRP vpss_grp = venc_chn / VPSS_MAX_CHN_NUM;
    VPSS_CHN vpss_chn = venc_chn - vpss_grp * VPSS_MAX_CHN_NUM;

    HI_S32 s32Ret = HI_MPI_VPSS_DisableChn(vpss_grp, vpss_chn);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VPSS_DisableChn failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    MPP_CHN_S src_chn;
    memset(&src_chn, 0, sizeof(MPP_CHN_S));
    src_chn.enModId = HI_ID_VPSS;
    src_chn.s32DevId = vpss_grp;
    src_chn.s32ChnId = vpss_chn;
    MPP_CHN_S dest_chn;
    memset(&dest_chn, 0, sizeof(MPP_CHN_S));
    dest_chn.enModId = HI_ID_VENC;
    dest_chn.s32DevId = 0;
    dest_chn.s32ChnId = venc_chn;
    s32Ret = HI_MPI_SYS_UnBind(&src_chn, &dest_chn);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_SYS_UnBind failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 disable_venc_chn(VENC_CHN venc_chn) {
    HI_S32 s32Ret;
    VPSS_GRP vpss_grp = venc_chn / VPSS_MAX_CHN_NUM;
    VPSS_CHN vpss_chn = venc_chn - vpss_grp * VPSS_MAX_CHN_NUM;

    s32Ret = HI_MPI_VENC_StopRecvPic(venc_chn);
    {
        MPP_CHN_S src_chn;
        src_chn.enModId = HI_ID_VPSS;
        src_chn.s32DevId = vpss_grp;
        src_chn.s32ChnId = vpss_chn;
        MPP_CHN_S dest_chn;
        dest_chn.enModId = HI_ID_VENC;
        dest_chn.s32DevId = 0;
        dest_chn.s32ChnId = venc_chn;
        s32Ret = HI_MPI_SYS_UnBind(&src_chn, &dest_chn);
    }
    s32Ret = HI_MPI_VENC_DestroyChn(venc_chn);

    s32Ret = HI_MPI_VPSS_DisableChn(vpss_grp, vpss_chn);

    set_channel_disable(venc_chn);
    return HI_SUCCESS;
};

int SYS_CalcPicVbBlkSize(
    unsigned int width, unsigned int height, PIXEL_FORMAT_E enPixFmt,
    HI_U32 u32AlignWidth) {
    if (16 != u32AlignWidth && 32 != u32AlignWidth && 64 != u32AlignWidth) {
        printf("system align width[%d] input failed!\n", u32AlignWidth);
        return -1;
    }
    HI_U32 u32VbSize =
        (CEILING_2_POWER(width, u32AlignWidth) *
         CEILING_2_POWER(height, u32AlignWidth) *
         ((PIXEL_FORMAT_YUV_SEMIPLANAR_422 == enPixFmt) ? 2 : 1.5));
    HI_U32 u32HeaderSize;
    VB_PIC_HEADER_SIZE(width, height, enPixFmt, u32HeaderSize);
    u32VbSize += u32HeaderSize;
    return u32VbSize;
}

#if HISILICON_SDK_GEN == 2
static HI_S32 HISDK_COMM_VI_SetMipiAttr(void) {
    /* mipi reset unrest */
    HI_S32 fd = open(MIPI_DEV, O_RDWR);
    if (fd < 0) {
        printf("warning: open hi_mipi dev failed\n");
        return EXIT_FAILURE;
    }
    combo_dev_attr_t mipi_attr = {.input_mode = sensor_config.input_mode, {}};
    if (ioctl(fd, _IOW('m', 0x01, combo_dev_attr_t), &mipi_attr)) {
        printf("set mipi attr failed\n");
        close(fd);
        return EXIT_FAILURE;
    }
    close(fd);
    return HI_SUCCESS;
}
#endif

#if HISILICON_SDK_GEN == 3
static HI_S32 HISDK_COMM_VI_SetMipiAttr(void) {
    HI_S32 fd;
    combo_dev_attr_t *pstcomboDevAttr = NULL;

    /* mipi reset unrest */
    fd = open(MIPI_DEV, O_RDWR);
    if (fd < 0) {
        printf("warning: open hi_mipi dev failed\n");
        return EXIT_FAILURE;
    }

    combo_dev_attr_t mipi_attr = {
        .devno = 0, .input_mode = sensor_config.input_mode, {}};

    if (sensor_config.videv.input_mod == VI_MODE_MIPI) {
        mipi_attr.mipi_attr.wdr_mode = HI_MIPI_WDR_MODE_NONE;
        mipi_attr.mipi_attr.raw_data_type = sensor_config.mipi.data_type;
        for (int i = 0; i < 4; i++) {
            mipi_attr.mipi_attr.lane_id[i] = sensor_config.mipi.lane_id[i];
        }
    }

    if (sensor_config.videv.input_mod == VI_MODE_LVDS) {
        mipi_attr.lvds_attr.img_size.width = sensor_config.lvds.img_size_w;
        mipi_attr.lvds_attr.img_size.height = sensor_config.lvds.img_size_h;
        mipi_attr.lvds_attr.raw_data_type = sensor_config.lvds.raw_data_type;
        mipi_attr.lvds_attr.wdr_mode = sensor_config.lvds.wdr_mode;
        mipi_attr.lvds_attr.sync_mode = sensor_config.lvds.sync_mode;
        mipi_attr.lvds_attr.vsync_type.sync_type = LVDS_VSYNC_NORMAL;
        mipi_attr.lvds_attr.vsync_type.hblank1 = 0;
        mipi_attr.lvds_attr.vsync_type.hblank2 = 0;
        mipi_attr.lvds_attr.fid_type.fid = LVDS_FID_NONE;
        mipi_attr.lvds_attr.fid_type.output_fil = HI_TRUE;
        mipi_attr.lvds_attr.data_endian = sensor_config.lvds.data_endian;
        mipi_attr.lvds_attr.sync_code_endian =
            sensor_config.lvds.sync_code_endian;
        for (int i = 0; i < 4; i++) {
            mipi_attr.lvds_attr.lane_id[i] = sensor_config.lvds.lane_id[i];
        }
        for (int i = 0; i < LVDS_LANE_NUM; i++) {
            for (int j = 0; j < WDR_VC_NUM; j++) {
                for (int m = 0; m < SYNC_CODE_NUM; m++) {
                    mipi_attr.lvds_attr.sync_code[i][j][m] =
                        sensor_config.lvds.sync_code[i][j * 4 + m];
                }
            }
        }
    }
    pstcomboDevAttr = &mipi_attr;

    /* 1. reset mipi */
    ioctl(fd, HI_MIPI_RESET_MIPI, &pstcomboDevAttr->devno);

    /* 2. reset sensor */
    ioctl(fd, HI_MIPI_RESET_SENSOR, &pstcomboDevAttr->devno);

    /* 3. set mipi attr */
    if (ioctl(fd, HI_MIPI_SET_DEV_ATTR, pstcomboDevAttr)) {
        printf("ioctl HI_MIPI_SET_DEV_ATTR failed\n");
        close(fd);
        return HI_FAILURE;
    }

    usleep(10000);

    /* 4. unreset mipi */
    ioctl(fd, HI_MIPI_UNRESET_MIPI, &pstcomboDevAttr->devno);

    /* 5. unreset sensor */
    ioctl(fd, HI_MIPI_UNRESET_SENSOR, &pstcomboDevAttr->devno);

    close(fd);
    return HI_SUCCESS;
}
#endif

pthread_t gs_VencPid = 0;
pthread_t gs_IspPid = 0;

int start_sdk() {
    printf("App build with headers MPP version: %s\n", MPP_VERSION);
    MPP_VERSION_S version;
    HI_MPI_SYS_GetVersion(&version);
    printf("Current MPP version:     %s\n", version.aVersion);

    struct SensorConfig sensor_config;
    if (parse_sensor_config(app_config.sensor_config, &sensor_config) !=
        CONFIG_OK) {
        printf("Can't load config\n");
        return EXIT_FAILURE;
    }
    LoadSensorLibrary(sensor_config.dll_file);

    unsigned int width = sensor_config.isp.isp_w;
    unsigned int height = sensor_config.isp.isp_h;
    unsigned int frame_rate = sensor_config.isp.isp_frame_rate;

    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();

    int u32AlignWidth = app_config.align_width;
    printf("u32AlignWidth: %d\n", app_config.align_width);

    VB_CONF_S vb_conf;
    memset(&vb_conf, 0, sizeof(VB_CONF_S));
    vb_conf.u32MaxPoolCnt = app_config.max_pool_cnt;
    int u32BlkSize = SYS_CalcPicVbBlkSize(
        width, height, sensor_config.vichn.pix_format, u32AlignWidth);
    vb_conf.astCommPool[0].u32BlkSize = u32BlkSize;
    vb_conf.astCommPool[0].u32BlkCnt =
        app_config.blk_cnt; // HI3516C = 10;  HI3516E = 4;
    printf(
        "vb_conf: u32MaxPoolCnt %d    [0]u32BlkSize %d   [0]u32BlkCnt %d\n",
        vb_conf.u32MaxPoolCnt, u32BlkSize, app_config.blk_cnt);

    HI_S32 s32Ret = HI_MPI_VB_SetConf(&vb_conf);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VB_SetConf failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    VB_SUPPLEMENT_CONF_S supplement_conf;
    memset(&supplement_conf, 0, sizeof(VB_SUPPLEMENT_CONF_S));
    supplement_conf.u32SupplementConf = 1;
    s32Ret = HI_MPI_VB_SetSupplementConf(&supplement_conf);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VB_SetSupplementConf failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    s32Ret = HI_MPI_VB_Init();
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VB_Init failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    MPP_SYS_CONF_S sys_conf;
    memset(&sys_conf, 0, sizeof(MPP_SYS_CONF_S));
    sys_conf.u32AlignWidth = u32AlignWidth;
    s32Ret = HI_MPI_SYS_SetConf(&sys_conf);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_SYS_SetConf failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    s32Ret = HI_MPI_SYS_Init();
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_SYS_Init failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    s32Ret = HISDK_COMM_VI_SetMipiAttr();
    if (HI_SUCCESS != s32Ret) {
        return EXIT_FAILURE;
    }

    sensor_register_callback();

    state.isp_dev = 0;
    ALG_LIB_S lib;
    memset(&lib, 0, sizeof(ALG_LIB_S));
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_ae_lib");
    s32Ret = HI_MPI_AE_Register(state.isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_AE_Register failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_awb_lib");
    s32Ret = HI_MPI_AWB_Register(state.isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_AWB_Register failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_af_lib");
    s32Ret = HI_MPI_AF_Register(state.isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_AF_Register failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    s32Ret = HI_MPI_ISP_MemInit(state.isp_dev);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_ISP_MemInit failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    HI_U32 chipId;
    s32Ret = HI_MPI_SYS_GetChipId(&chipId);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_SYS_GetChipId failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }
    printf("HI_MPI_SYS_GetChipId: %#X\n", chipId);

    if (app_config.motion_detect_enable) {
        s32Ret = motion_detect_init();
        if (HI_SUCCESS != s32Ret) {
            printf(
                "Can't init motion detect system. Failed with %#x!\n%s\n",
                s32Ret, hi_errstr(s32Ret));
            return EXIT_FAILURE;
        }
    }

    ISP_WDR_MODE_S wdr_mode;
    memset(&wdr_mode, 0, sizeof(ISP_WDR_MODE_S));
    wdr_mode.enWDRMode = sensor_config.mode;
    s32Ret = HI_MPI_ISP_SetWDRMode(state.isp_dev, &wdr_mode);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_ISP_SetWDRMode failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    ISP_PUB_ATTR_S pub_attr;
    memset(&pub_attr, 0, sizeof(ISP_PUB_ATTR_S));
    pub_attr.stWndRect.s32X = sensor_config.isp.isp_x;
    pub_attr.stWndRect.s32Y = sensor_config.isp.isp_y;
    pub_attr.stWndRect.u32Width = sensor_config.isp.isp_w;
    pub_attr.stWndRect.u32Height = sensor_config.isp.isp_h;
    pub_attr.f32FrameRate = sensor_config.isp.isp_frame_rate;
    pub_attr.enBayer = sensor_config.isp.isp_bayer;
    s32Ret = HI_MPI_ISP_SetPubAttr(state.isp_dev, &pub_attr);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_ISP_SetPubAttr failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    s32Ret = HI_MPI_ISP_Init(state.isp_dev);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_ISP_Init failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    {
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        size_t stacksize;
        pthread_attr_getstacksize(&thread_attr, &stacksize);
        size_t new_stacksize = app_config.isp_thread_stack_size;
        if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) {
            printf("Error:  Can't set stack size %zu\n", new_stacksize);
        }
        if (0 != pthread_create(
                     &gs_IspPid, &thread_attr, (void *(*)(void *))Test_ISP_Run,
                     NULL)) {
            printf("%s: create isp running thread failed!\n", __FUNCTION__);
            return EXIT_FAILURE;
        }
        if (pthread_attr_setstacksize(&thread_attr, stacksize)) {
            printf("Error:  Can't set stack size %zu\n", stacksize);
        }
        pthread_attr_destroy(&thread_attr);
    }
    usleep(1000);

    state.vi_dev = 0;
    VI_DEV_ATTR_S vi_dev_attr;
    memset(&vi_dev_attr, 0, sizeof(VI_DEV_ATTR_S));
    vi_dev_attr.enIntfMode = VI_MODE_DIGITAL_CAMERA;
    vi_dev_attr.enWorkMode = sensor_config.videv.work_mod;
    vi_dev_attr.au32CompMask[0] = sensor_config.videv.mask_0;
    vi_dev_attr.au32CompMask[1] = sensor_config.videv.mask_1;
    vi_dev_attr.enScanMode = sensor_config.videv.scan_mode;
    vi_dev_attr.s32AdChnId[0] = -1;
    vi_dev_attr.s32AdChnId[1] = -1;
    vi_dev_attr.s32AdChnId[2] = -1;
    vi_dev_attr.s32AdChnId[3] = -1;
    vi_dev_attr.enDataSeq = sensor_config.videv.data_seq;
    vi_dev_attr.stSynCfg.enVsync = sensor_config.videv.vsync;
    vi_dev_attr.stSynCfg.enVsyncNeg = sensor_config.videv.vsync_neg;
    vi_dev_attr.stSynCfg.enHsync = sensor_config.videv.hsync;
    vi_dev_attr.stSynCfg.enHsyncNeg = sensor_config.videv.hsync_neg;
    vi_dev_attr.stSynCfg.enVsyncValid = sensor_config.videv.vsync_valid;
    vi_dev_attr.stSynCfg.enVsyncValidNeg = sensor_config.videv.vsync_valid_neg;
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncHfb =
        sensor_config.videv.timing_blank_hsync_hfb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncAct =
        sensor_config.videv.timing_blank_hsync_act;
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncHbb =
        sensor_config.videv.timing_blank_hsync_hbb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVfb =
        sensor_config.videv.timing_blank_vsync_vfb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVact =
        sensor_config.videv.timing_blank_vsync_vact;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbb =
        sensor_config.videv.timing_blank_vsync_vbb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbfb =
        sensor_config.videv.timing_blank_vsync_vbfb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbact =
        sensor_config.videv.timing_blank_vsync_vbact;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbbb =
        sensor_config.videv.timing_blank_vsync_vbbb;
    vi_dev_attr.enDataPath = sensor_config.videv.data_path;
    vi_dev_attr.enInputDataType = sensor_config.videv.input_data_type;
    vi_dev_attr.bDataRev = sensor_config.videv.data_rev;
    vi_dev_attr.stDevRect.s32X = sensor_config.videv.dev_rect_x;
    vi_dev_attr.stDevRect.s32Y = sensor_config.videv.dev_rect_y;
    vi_dev_attr.stDevRect.u32Width = sensor_config.videv.dev_rect_w;
    vi_dev_attr.stDevRect.u32Height = sensor_config.videv.dev_rect_h;

    s32Ret = HI_MPI_VI_SetDevAttr(state.vi_dev, &vi_dev_attr);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VI_SetDevAttr failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    VI_WDR_ATTR_S wdr_addr;
    memset(&wdr_addr, 0, sizeof(VI_WDR_ATTR_S));
    wdr_addr.enWDRMode = WDR_MODE_NONE;
    wdr_addr.bCompress = HI_FALSE;
    s32Ret = HI_MPI_VI_SetWDRAttr(state.vi_dev, &wdr_addr);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VI_SetWDRAttr failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    s32Ret = HI_MPI_VI_EnableDev(state.vi_dev);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VI_EnableDev failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    state.vi_chn = 0;
    VI_CHN_ATTR_S chn_attr;
    memset(&chn_attr, 0, sizeof(VI_CHN_ATTR_S));
    chn_attr.stCapRect.s32X = sensor_config.vichn.cap_rect_x;
    chn_attr.stCapRect.s32Y = sensor_config.vichn.cap_rect_y;
    chn_attr.stCapRect.u32Width = sensor_config.vichn.cap_rect_width;
    chn_attr.stCapRect.u32Height = sensor_config.vichn.cap_rect_height;
    chn_attr.stDestSize.u32Width = sensor_config.vichn.dest_size_width;
    chn_attr.stDestSize.u32Height = sensor_config.vichn.dest_size_height;
    chn_attr.enCapSel = sensor_config.vichn.cap_sel;
    chn_attr.enPixFormat = sensor_config.vichn.pix_format;
    chn_attr.bMirror = HI_FALSE;
    chn_attr.bFlip = HI_FALSE;
    chn_attr.s32SrcFrameRate = -1;
    chn_attr.s32DstFrameRate = -1;
    chn_attr.enCompressMode = sensor_config.vichn.compress_mode;
    s32Ret = HI_MPI_VI_SetChnAttr(state.vi_chn, &chn_attr);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VI_SetChnAttr failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    s32Ret = HI_MPI_VI_EnableChn(state.vi_chn);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VI_EnableChn failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    HI_U32 mode;
    s32Ret = HI_MPI_SYS_GetViVpssMode(&mode);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_SYS_GetViVpssMode failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }
    bool online_mode = mode == 1;
    printf("SDK is in '%s' mode\n", online_mode ? "online" : "offline");

    {
        VPSS_GRP vpss_grp = 0;
        VPSS_GRP_ATTR_S vpss_grp_attr;
        memset(&vpss_grp_attr, 0, sizeof(VPSS_GRP_ATTR_S));
        vpss_grp_attr.u32MaxW = sensor_config.vichn.dest_size_width;
        vpss_grp_attr.u32MaxH = sensor_config.vichn.dest_size_height;
        vpss_grp_attr.enPixFmt = sensor_config.vichn.pix_format;
        vpss_grp_attr.bIeEn = HI_FALSE;
        vpss_grp_attr.bDciEn = HI_FALSE;
        vpss_grp_attr.bNrEn = HI_TRUE;
        vpss_grp_attr.bHistEn = HI_FALSE;
        vpss_grp_attr.enDieMode = VPSS_DIE_MODE_NODIE;
        s32Ret = HI_MPI_VPSS_CreateGrp(vpss_grp, &vpss_grp_attr);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_VPSS_CreateGrp(%d, ...) failed with %#x!\n%s\n",
                vpss_grp, s32Ret, hi_errstr(s32Ret));
            return EXIT_FAILURE;
        }

        s32Ret = HI_MPI_VPSS_StartGrp(vpss_grp);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_VPSS_StartGrp(%d) failed with %#x!\n%s\n", vpss_grp,
                s32Ret, hi_errstr(s32Ret));
            return EXIT_FAILURE;
        }

        {
            MPP_CHN_S src_chn;
            src_chn.enModId = HI_ID_VIU;
            src_chn.s32DevId = 0;
            src_chn.s32ChnId = 0;
            MPP_CHN_S dest_chn;
            dest_chn.enModId = HI_ID_VPSS;
            dest_chn.s32DevId = vpss_grp;
            dest_chn.s32ChnId = 0;
            s32Ret = HI_MPI_SYS_Bind(&src_chn, &dest_chn);
            if (HI_SUCCESS != s32Ret) {
                printf(
                    "HI_MPI_SYS_Bind failed with %#x!\n%s\n", s32Ret,
                    hi_errstr(s32Ret));
                return EXIT_FAILURE;
            }
        }
    }

    // config venc
    if (app_config.mp4_enable) {
        VENC_CHN venc_chn = take_next_free_channel(true);

        s32Ret = create_venc_chn(
            venc_chn, sensor_config.isp.isp_frame_rate, app_config.mp4_fps);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "create_vpss_chn failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return EXIT_FAILURE;
        }

        unsigned int width = app_config.mp4_width;
        unsigned int height = app_config.mp4_height;

        VENC_CHN_ATTR_S venc_chn_attr = {
            .stVeAttr =
                {
                    .enType = PT_H264,
                    .stAttrH264e =
                        {
                            .u32MaxPicWidth = width,
                            .u32MaxPicHeight = height,
                            .u32PicWidth = width,         /*the picture width*/
                            .u32PicHeight = height,       /*the picture height*/
                            .u32BufSize = width * height, /*stream buffer size*/
                            .u32Profile =
                                0, /*0: baseline; 1:MP; 2:HP;  3:svc_t */
                            .bByFrame = HI_TRUE, /*get stream mode is slice mode
                                                    or frame mode?*/
#if HISILICON_SDK_GEN <= 2
                            .u32RefNum =
                                1, /* 0: default; number of reference frames*/
#endif
                        },
                },
            .stRcAttr =
                {
                    .enRcMode = VENC_RC_MODE_H264CBR,
                    .stAttrH264Cbr =
                        {
                            .u32Gop = app_config.mp4_fps,
                            .u32StatTime = 1, /* stream rate statics time(s) */
                            .u32SrcFrmRate =
                                app_config.mp4_fps, /* input (vi) frame rate */
                            .fr32DstFrmRate =
                                app_config.mp4_fps, /* target frame rate */
                            .u32BitRate = app_config.mp4_bitrate,
                        },
                },
        };

        s32Ret = HI_MPI_VENC_CreateChn(venc_chn, &venc_chn_attr);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_VENC_CreateChn failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return EXIT_FAILURE;
        }

        s32Ret = HI_MPI_VENC_StartRecvPic(venc_chn);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_VENC_StartRecvPic failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return EXIT_FAILURE;
        }
    }

    if (app_config.mjpeg_enable) {
        VENC_CHN venc_chn = take_next_free_channel(true);

        s32Ret = create_venc_chn(
            venc_chn, sensor_config.isp.isp_frame_rate, app_config.mjpeg_fps);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "create_vpss_chn failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return EXIT_FAILURE;
        }

        unsigned int width = app_config.mjpeg_width;
        unsigned int height = app_config.mjpeg_height;

        VENC_CHN_ATTR_S venc_chn_attr = {
            .stVeAttr =
                {
                    .enType = PT_MJPEG,
                    .MPEG_ATTR =
                        {
                            .u32MaxPicWidth = width,
                            .u32MaxPicHeight = height,
                            .u32PicWidth = width,
                            .u32PicHeight = height,
                            .u32BufSize = (((width + 15) >> 4) << 4) *
                                          (((height + 15) >> 4) << 4),
                            .bByFrame = HI_TRUE, // use full frames
                        },

                },
            .stRcAttr =
                {
                    .enRcMode = VENC_RC_MODE_MJPEGCBR,
                    .stAttrMjpegeCbr =
                        {
                            .u32StatTime = 1,
                            .u32SrcFrmRate = app_config.mjpeg_fps,
                            .fr32DstFrmRate = app_config.mjpeg_fps,
                            .u32BitRate = app_config.mjpeg_bitrate,
                        },
                },
        };

        s32Ret = HI_MPI_VENC_CreateChn(venc_chn, &venc_chn_attr);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_VENC_CreateChn failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return EXIT_FAILURE;
        }

        s32Ret = HI_MPI_VENC_StartRecvPic(venc_chn);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_VENC_StartRecvPic failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return EXIT_FAILURE;
        }
    }

    s32Ret = InitJPEG();
    if (HI_SUCCESS != s32Ret) {
        printf("Init_JPEG() failed with %#x!\n", s32Ret);
        return EXIT_FAILURE;
    }

    {
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        size_t stacksize;
        pthread_attr_getstacksize(&thread_attr, &stacksize);
        size_t new_stacksize = app_config.venc_stream_thread_stack_size;
        if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) {
            printf("Error:  Can't set stack size %zu\n", new_stacksize);
        }
        if (0 != pthread_create(
                     &gs_VencPid, &thread_attr, VENC_GetVencStreamProc, NULL)) {
            printf(
                "%s: create VENC_GetVencStreamProc running thread failed!\n",
                __FUNCTION__);
            return EXIT_FAILURE;
        }
        if (pthread_attr_setstacksize(&thread_attr, stacksize)) {
            printf("Error:  Can't set stack size %zu\n", stacksize);
        }
        pthread_attr_destroy(&thread_attr);
    }

    printf("Start sdk Ok!\n");
    return EXIT_SUCCESS;
}

int stop_sdk() {
    HI_S32 s32Ret;
    pthread_join(gs_VencPid, NULL);

    s32Ret = DestroyJPEG();

    for (int i = 0; i < VENC_MAX_CHN_NUM; i++)
        if (channel_is_enable(i))
            disable_venc_chn(i);

    if (app_config.motion_detect_enable) {
        s32Ret = motion_detect_deinit();
        if (HI_SUCCESS != s32Ret) {
            printf(
                "motion_detect_deinit failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return EXIT_FAILURE;
        }
    }

    for (VPSS_GRP vpss_grp = 0; vpss_grp < VPSS_MAX_GRP_NUM; ++vpss_grp) {
        for (VPSS_CHN vpss_chn = 0; vpss_chn < VPSS_MAX_CHN_NUM; ++vpss_chn) {
            s32Ret = HI_MPI_VPSS_DisableChn(vpss_grp, vpss_chn);
        }

        {
            MPP_CHN_S src_chn;
            src_chn.enModId = HI_ID_VIU;
            src_chn.s32DevId = 0;
            src_chn.s32ChnId = 0;
            MPP_CHN_S dest_chn;
            dest_chn.enModId = HI_ID_VPSS;
            dest_chn.s32DevId = vpss_grp;
            dest_chn.s32ChnId = 0;
            s32Ret = HI_MPI_SYS_UnBind(&src_chn, &dest_chn);
            if (HI_SUCCESS != s32Ret) {
                printf(
                    "HI_MPI_SYS_UnBind failed with %#x!\n%s\n", s32Ret,
                    hi_errstr(s32Ret));
                return EXIT_FAILURE;
            }
        }

        s32Ret = HI_MPI_VPSS_StopGrp(vpss_grp);
        s32Ret = HI_MPI_VPSS_DestroyGrp(vpss_grp);
    }
    s32Ret = HI_MPI_VI_DisableChn(state.vi_chn);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VI_DisableChn failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }
    s32Ret = HI_MPI_VI_DisableDev(state.vi_dev);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VI_DisableDev failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    s32Ret = HI_MPI_ISP_Exit(state.isp_dev);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_ISP_Exit failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }
    pthread_join(gs_IspPid, NULL);

    ALG_LIB_S lib;
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_af_lib\0        ");
    s32Ret = HI_MPI_AF_UnRegister(state.isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_AF_UnRegister failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_awb_lib\0       ");
    s32Ret = HI_MPI_AWB_UnRegister(state.isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_AWB_UnRegister failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_ae_lib\0        ");
    s32Ret = HI_MPI_AE_UnRegister(state.isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_AE_UnRegister failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }
    sensor_unregister_callback();

    s32Ret = HI_MPI_SYS_Exit();
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_SYS_Exit failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }
    s32Ret = HI_MPI_VB_Exit();
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_VB_Exit failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    UnloadSensorLibrary();

    printf("Stop sdk Ok!\n");
    return EXIT_SUCCESS;
}
