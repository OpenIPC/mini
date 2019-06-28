#include "hidemo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include <mpi_sys.h>
#include <mpi_vb.h>
#include <mpi_vi.h>
#include <mpi_venc.h>
#include <mpi_vpss.h>
#include <mpi_isp.h>
#include <mpi_ae.h>
#include <mpi_awb.h>
#include <mpi_af.h>
#include <mpi_region.h>

#include "config/sensor_config.h"
#include "sensor.h"
#include "server.h"
#include "hierrors.h"

#include "rtsp/rtspservice.h"
#include "rtsp/rtputils.h"
#include "rtsp/ringfifo.h"

#include "config/app_config.h"

#include "motion_detect.h"

HI_S32 HI_MPI_SYS_GetChipId(HI_U32 *pu32ChipId);

HI_VOID* Test_ISP_Run(HI_VOID *param) {
    ISP_DEV isp_dev = 0;
    HI_S32 s32Ret = HI_MPI_ISP_Run(isp_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_Run failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); }
    printf("Shutdown isp_run thread\n");
    return HI_NULL;
}

HI_S32 VENC_SaveH264(int chn_index, VENC_STREAM_S *pstStream) {
    if (app_config.mp4_enable) send_mp4_to_client(chn_index, pstStream);
    if (app_config.mp4_enable) send_h264_to_client(chn_index, pstStream);
    if (app_config.rtsp_enable) HisiPutH264DataToBuffer(pstStream);
    return HI_SUCCESS;
}

HI_S32 VENC_SaveJpeg(int chn_index, VENC_STREAM_S *pstStream) {
    if (app_config.jpeg_enable) {
        static char *jpeg_buf;
        static ssize_t jpeg_buf_size = 0;
        ssize_t buf_size = 0;
        for (HI_U32 i = 0; i < pstStream->u32PackCount; i++) {
            VENC_PACK_S*  pstData = &pstStream->pstPack[i];
            ssize_t need_size = buf_size + pstData->u32Len-pstData->u32Offset + 2;
            if (need_size > jpeg_buf_size) {
                jpeg_buf = realloc(jpeg_buf, need_size);
                jpeg_buf_size = need_size;
            }
            memcpy(jpeg_buf + buf_size, pstData->pu8Addr+pstData->u32Offset, pstData->u32Len-pstData->u32Offset);
            buf_size += pstData->u32Len-pstData->u32Offset;
        }
        send_jpeg(chn_index, jpeg_buf, buf_size);
    }
    return HI_SUCCESS;
}

HI_S32 VENC_SaveMJpeg(int chn_index, VENC_STREAM_S *pstStream) {
    if (app_config.mjpeg_enable) {
        static char *mjpeg_buf;
        static ssize_t mjpeg_buf_size = 0;
        ssize_t buf_size = 0;
        //fwrite(g_SOI, 1, sizeof(g_SOI), fpJpegFile); //in Hi3531, user needn't write SOI!
        for (HI_U32 i = 0; i < pstStream->u32PackCount; i++) {
            VENC_PACK_S*  pstData = &pstStream->pstPack[i];
            ssize_t need_size = buf_size + pstData->u32Len-pstData->u32Offset + 2;
            if (need_size > mjpeg_buf_size) {
                mjpeg_buf = realloc(mjpeg_buf, need_size);
                mjpeg_buf_size = need_size;
            }
            memcpy(mjpeg_buf + buf_size, pstData->pu8Addr+pstData->u32Offset, pstData->u32Len-pstData->u32Offset);
            buf_size += pstData->u32Len-pstData->u32Offset;
        }
        send_mjpeg(chn_index, mjpeg_buf, buf_size);
    }
    return HI_SUCCESS;
}
HI_S32 VENC_SaveStream(int chn_index, PAYLOAD_TYPE_E enType, VENC_STREAM_S *pstStream) {
    // printf("Chn: %d   VENC_SaveStream %d  packs: %d\n", chn_index, enType, pstStream->u32PackCount, pstStream);
    HI_S32 s32Ret;
    if (PT_H264 == enType) { s32Ret = VENC_SaveH264(chn_index,  pstStream); }
    else if (PT_MJPEG == enType) { s32Ret = VENC_SaveMJpeg(chn_index, pstStream); }
    else if (PT_JPEG == enType) { s32Ret = VENC_SaveJpeg(chn_index, pstStream); }
    // else if (PT_H265 == enType) { s32Ret = SAMPLE_COMM_VENC_SaveH265(pFd, pstStream); }
    else { return HI_FAILURE; }
    return s32Ret;
}


bool VencEnabled[VENC_MAX_CHN_NUM] = {0};

HI_VOID* VENC_GetVencStreamProc(HI_VOID *p) {
    HI_S32 maxfd = 0;
    HI_S32 s32Ret;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];

    HI_S32 VencFd[VENC_MAX_CHN_NUM] = {0};

    for (HI_S32 chn_index = 0; chn_index < VENC_MAX_CHN_NUM; chn_index++) {
        if (!VencEnabled[chn_index]) continue;

        VENC_CHN_ATTR_S stVencChnAttr;
        s32Ret = HI_MPI_VENC_GetChnAttr(chn_index, &stVencChnAttr);
        if(s32Ret != HI_SUCCESS) { printf("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n%s\n", chn_index, s32Ret, hi_errstr(s32Ret)); return NULL; }
        enPayLoadType[chn_index] = stVencChnAttr.stVeAttr.enType;

        VencFd[chn_index] = HI_MPI_VENC_GetFd(chn_index);
        if (VencFd[chn_index] < 0) { printf("HI_MPI_VENC_GetFd chn[%d] failed with %#x!\n%s\n", chn_index, VencFd[chn_index], hi_errstr(VencFd[chn_index])); return NULL; }
        if (maxfd <= VencFd[chn_index]) {  maxfd = VencFd[chn_index]; }

        printf("set VencFd chn: %d,  fd: %d\n", chn_index, VencFd[chn_index]);
    }

    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    struct timeval TimeoutVal;
    fd_set read_fds;
    while (keepRunning) {
        FD_ZERO(&read_fds);
        for (HI_S32 chn_index = 0; chn_index < VENC_MAX_CHN_NUM; chn_index++) {
            if (!VencEnabled[chn_index]) continue;

            // printf("fd_set chn: %d,  fd: %d\n", chn_index, VencFd[chn_index]);
            FD_SET(VencFd[chn_index], &read_fds);
        }

        TimeoutVal.tv_sec  = 2;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0) { printf("select failed!\n"); break; }
        else if (s32Ret == 0) { printf("get sample_venc stream time out, exit thread\n"); continue; }
        else {
            for (HI_S32 chn_index = 0; chn_index < VENC_MAX_CHN_NUM; chn_index++) {
                if (!VencEnabled[chn_index]) continue;

                if (FD_ISSET(VencFd[chn_index], &read_fds)) {
                    // printf("fd_was_set! chn: %d\n", chn_index);
                    memset(&stStream, 0, sizeof(stStream));
                    s32Ret = HI_MPI_VENC_Query(chn_index, &stStat);
                    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_Query chn[%d] failed with %#x!\n%s\n", chn_index, s32Ret, hi_errstr(s32Ret)); break; }

                    if(0 == stStat.u32CurPacks) { printf("NOTE: Current frame is NULL!\n"); continue; }

                    stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack) { printf("malloc stream chn[%d] pack failed!\n", chn_index); break; }
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = HI_MPI_VENC_GetStream(chn_index, &stStream, HI_TRUE);
                    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_GetStream chn[%d] failed with %#x!\n%s\n", chn_index, s32Ret, hi_errstr(s32Ret)); free(stStream.pstPack); stStream.pstPack = NULL; break; }
                    s32Ret = VENC_SaveStream(chn_index, enPayLoadType[chn_index], &stStream);
                    if (HI_SUCCESS != s32Ret) { printf("VENC_SaveStream chn[%d] failed with %#x!\n%s\n", chn_index, s32Ret, hi_errstr(s32Ret)); free(stStream.pstPack); stStream.pstPack = NULL; break; }
                    s32Ret = HI_MPI_VENC_ReleaseStream(chn_index, &stStream);
                    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_ReleaseStream chn[%d] failed with %#x!\n%s\n", chn_index, s32Ret, hi_errstr(s32Ret)); free(stStream.pstPack); stStream.pstPack = NULL;  break; }
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                }
            }
        }
    }
    printf("Shutdown hisdk venc thread\n");
    return NULL;
}

HI_S32 create_vpss_chn(int vpss_grp, int vpss_chn, uint32_t fps_src, uint32_t fps_dst) {
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
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_SetChnAttr failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    VPSS_CHN_MODE_S vpss_chn_mode;
    memset(&vpss_chn_mode, 0, sizeof(VPSS_CHN_MODE_S));
    vpss_chn_mode.enChnMode = VPSS_CHN_MODE_USER;
    vpss_chn_mode.u32Width = sensor_config.vichn.dest_size_width;
    vpss_chn_mode.u32Height = sensor_config.vichn.dest_size_height;
    vpss_chn_mode.bDouble = HI_FALSE;
    vpss_chn_mode.enPixelFormat = sensor_config.vichn.pix_format;
    vpss_chn_mode.enCompressMode = sensor_config.vichn.compress_mode;
    s32Ret = HI_MPI_VPSS_SetChnMode(vpss_grp, vpss_chn, &vpss_chn_mode);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_SetChnMode failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VPSS_EnableChn(vpss_grp, vpss_chn);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_EnableChn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    return HI_SUCCESS;
}

int SYS_CalcPicVbBlkSize(unsigned int width, unsigned int height, PIXEL_FORMAT_E enPixFmt, HI_U32 u32AlignWidth) {
//    if (PIXEL_FORMAT_YUV_SEMIPLANAR_422 != enPixFmt && PIXEL_FORMAT_YUV_SEMIPLANAR_420 != enPixFmt) {
//        printf("pixel format[%d] input failed! %#x\n\n", enPixFmt);
//        return -1;
//    }
    if (16!=u32AlignWidth && 32!=u32AlignWidth && 64!=u32AlignWidth) {
        printf("system align width[%d] input failed!\n", u32AlignWidth);
        return -1;
    }
    //PRT("w:%d, u32AlignWidth:%d\n", CEILING_2_POWER(stSize.u32Width,u32AlignWidth), u32AlignWidth);
    HI_U32 u32VbSize = (CEILING_2_POWER(width, u32AlignWidth) * \
            CEILING_2_POWER(height,u32AlignWidth) * \
           ((PIXEL_FORMAT_YUV_SEMIPLANAR_422 == enPixFmt)?2:1.5));
    HI_U32 u32HeaderSize;
    VB_PIC_HEADER_SIZE(width, height, enPixFmt, u32HeaderSize);
    u32VbSize += u32HeaderSize;
    return u32VbSize;
}

pthread_t gs_VencPid = 0;
pthread_t gs_IspPid = 0;

int start_sdk(struct SDKState *state) {
    printf("App build with headers MPP version: %s\n", MPP_VERSION);
    MPP_VERSION_S version;
    HI_MPI_SYS_GetVersion(&version);
    printf("Current MPP version:     %s\n", version.aVersion);

    struct SensorConfig sensor_config;
    if (parse_sensor_config(app_config.sensor_config, &sensor_config) != CONFIG_OK) {
        printf("Can't load config\n");
        return EXIT_FAILURE;
    }
    LoadSensorLibrary(sensor_config.dll_file);

    unsigned int width = sensor_config.isp.isp_w;
    unsigned int height = sensor_config.isp.isp_h;
    unsigned int frame_rate = sensor_config.isp.isp_frame_rate;

    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();

    VB_CONF_S vb_conf;
    memset(&vb_conf, 0, sizeof(VB_CONF_S));
    vb_conf.u32MaxPoolCnt = app_config.max_pool_cnt;

    int u32AlignWidth = app_config.align_width;

    int u32BlkSize = SYS_CalcPicVbBlkSize(width, height, sensor_config.vichn.pix_format, u32AlignWidth);
    vb_conf.astCommPool[0].u32BlkSize = u32BlkSize;
    vb_conf.astCommPool[0].u32BlkCnt = app_config.blk_cnt; // HI3516C = 10;  HI3516E = 4;

    HI_S32 s32Ret = HI_MPI_VB_SetConf(&vb_conf);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VB_SetConf failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    VB_SUPPLEMENT_CONF_S supplement_conf;
    memset(&supplement_conf, 0, sizeof(VB_SUPPLEMENT_CONF_S));
    supplement_conf.u32SupplementConf = 1;
    s32Ret = HI_MPI_VB_SetSupplementConf(&supplement_conf);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VB_SetSupplementConf failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VB_Init();
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VB_Init failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    MPP_SYS_CONF_S sys_conf;
    memset(&sys_conf, 0, sizeof(MPP_SYS_CONF_S));
    sys_conf.u32AlignWidth = u32AlignWidth;
    s32Ret = HI_MPI_SYS_SetConf(&sys_conf);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_SetConf failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    s32Ret = HI_MPI_SYS_Init();
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_Init failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    /* mipi reset unrest */
    HI_S32 fd = open("/dev/hi_mipi", O_RDWR);
    if (fd < 0) { printf("warning: open hi_mipi dev failed\n"); return EXIT_FAILURE; }
    combo_dev_attr_t mipi_attr = { .input_mode = sensor_config.input_mode, { } };
    if (ioctl(fd, _IOW('m', 0x01, combo_dev_attr_t), &mipi_attr)) {
        printf("set mipi attr failed\n");
        close(fd);
        return EXIT_FAILURE;
    }
    close(fd);

    sensor_register_callback();

    state->isp_dev = 0;
    ALG_LIB_S lib;
    memset(&lib, 0, sizeof(ALG_LIB_S));
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_ae_lib\0        ");
    s32Ret = HI_MPI_AE_Register(state->isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AE_Register failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_awb_lib\0       ");
    s32Ret = HI_MPI_AWB_Register(state->isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AWB_Register failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_af_lib\0        ");
    s32Ret = HI_MPI_AF_Register(state->isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AF_Register failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    s32Ret = HI_MPI_ISP_MemInit(state->isp_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_MemInit failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    HI_U32 chipId;
    s32Ret = HI_MPI_SYS_GetChipId(&chipId);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_GetChipId failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    printf("HI_MPI_SYS_GetChipId: %#X\n", chipId);

    if(app_config.motion_detect_enable) {
        s32Ret = motion_detect_init();
        if (HI_SUCCESS != s32Ret) { printf("Can't init motion detect system. Failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    }

    ISP_WDR_MODE_S wdr_mode;
    memset(&wdr_mode, 0, sizeof(ISP_WDR_MODE_S));
    wdr_mode.enWDRMode = sensor_config.mode;
    s32Ret = HI_MPI_ISP_SetWDRMode(state->isp_dev, &wdr_mode);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_SetWDRMode failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    ISP_PUB_ATTR_S pub_attr;
    memset(&pub_attr, 0, sizeof(ISP_PUB_ATTR_S));
    pub_attr.stWndRect.s32X = sensor_config.isp.isp_x;
    pub_attr.stWndRect.s32Y = sensor_config.isp.isp_y;
    pub_attr.stWndRect.u32Width = sensor_config.isp.isp_w;
    pub_attr.stWndRect.u32Height = sensor_config.isp.isp_h;
    pub_attr.f32FrameRate = sensor_config.isp.isp_frame_rate;
    pub_attr.enBayer = sensor_config.isp.isp_bayer;
    s32Ret = HI_MPI_ISP_SetPubAttr(state->isp_dev, &pub_attr);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_SetPubAttr failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    s32Ret = HI_MPI_ISP_Init(state->isp_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_Init failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    {
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        size_t stacksize;
        pthread_attr_getstacksize(&thread_attr,&stacksize);
        size_t new_stacksize = app_config.isp_thread_stack_size;
        if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) { printf("Error:  Can't set stack size %ld\n", new_stacksize); }
        if (0 != pthread_create(&gs_IspPid, &thread_attr, (void* (*)(void*))Test_ISP_Run, NULL)) { printf("%s: create isp running thread failed!\n", __FUNCTION__); return EXIT_FAILURE; }
        if (pthread_attr_setstacksize(&thread_attr, stacksize)) { printf("Error:  Can't set stack size %ld\n", stacksize); }
        pthread_attr_destroy(&thread_attr);
    }
    usleep(1000);

    state->vi_dev = 0;
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
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncHfb = sensor_config.videv.timing_blank_hsync_hfb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncAct = sensor_config.videv.timing_blank_hsync_act;
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncHbb = sensor_config.videv.timing_blank_hsync_hbb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVfb = sensor_config.videv.timing_blank_vsync_vfb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVact = sensor_config.videv.timing_blank_vsync_vact;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbb = sensor_config.videv.timing_blank_vsync_vbb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbfb = sensor_config.videv.timing_blank_vsync_vbfb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbact = sensor_config.videv.timing_blank_vsync_vbact;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbbb = sensor_config.videv.timing_blank_vsync_vbbb;
    vi_dev_attr.enDataPath = sensor_config.videv.data_path;
    vi_dev_attr.enInputDataType = sensor_config.videv.input_data_type;
    vi_dev_attr.bDataRev = sensor_config.videv.data_rev;
    vi_dev_attr.stDevRect.s32X = sensor_config.videv.dev_rect_x;
    vi_dev_attr.stDevRect.s32Y = sensor_config.videv.dev_rect_y;
    vi_dev_attr.stDevRect.u32Width = sensor_config.videv.dev_rect_w;
    vi_dev_attr.stDevRect.u32Height = sensor_config.videv.dev_rect_h;

    s32Ret = HI_MPI_VI_SetDevAttr(state->vi_dev, &vi_dev_attr);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_SetDevAttr failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    VI_WDR_ATTR_S wdr_addr;
    memset(&wdr_addr, 0, sizeof(VI_WDR_ATTR_S));
    wdr_addr.enWDRMode = WDR_MODE_NONE;
    wdr_addr.bCompress = HI_FALSE;
    s32Ret = HI_MPI_VI_SetWDRAttr(state->vi_dev, &wdr_addr);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_SetWDRAttr failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VI_EnableDev(state->vi_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_EnableDev failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    state->vi_chn = 0;
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
    s32Ret = HI_MPI_VI_SetChnAttr(state->vi_chn, &chn_attr);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_SetChnAttr failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VI_EnableChn(state->vi_chn);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_EnableChn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    state->vpss_grp = 0;
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
    s32Ret = HI_MPI_VPSS_CreateGrp(state->vpss_grp, &vpss_grp_attr);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_CreateGrp failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VPSS_StartGrp(state->vpss_grp);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_StartGrp failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    {
        MPP_CHN_S src_chn;
        src_chn.enModId = HI_ID_VIU;
        src_chn.s32DevId = 0;
        src_chn.s32ChnId = 0;
        MPP_CHN_S dest_chn;
        dest_chn.enModId = HI_ID_VPSS;
        dest_chn.s32DevId = 0;
        dest_chn.s32ChnId = 0;
        s32Ret = HI_MPI_SYS_Bind(&src_chn, &dest_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_Bind failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    }

//    state->vpss_chn = 0;
//    VPSS_CHN_ATTR_S vpss_chn_attr;
//    memset(&vpss_chn_attr, 0, sizeof(VPSS_CHN_ATTR_S));
//    vpss_chn_attr.bSpEn = HI_FALSE;
//    vpss_chn_attr.bBorderEn = HI_FALSE;
//    vpss_chn_attr.bMirror = HI_FALSE;
//    vpss_chn_attr.bFlip = HI_FALSE;
//    vpss_chn_attr.s32SrcFrameRate = -1;
//    vpss_chn_attr.s32DstFrameRate = -1;
//    vpss_chn_attr.stBorder.u32TopWidth = 0;
//    vpss_chn_attr.stBorder.u32BottomWidth = 0;
//    vpss_chn_attr.stBorder.u32LeftWidth = 0;
//    vpss_chn_attr.stBorder.u32RightWidth = 0;
//    vpss_chn_attr.stBorder.u32Color = 0;
//    s32Ret = HI_MPI_VPSS_SetChnAttr(state->vpss_grp, state->vpss_chn, &vpss_chn_attr);
//    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_SetChnAttr failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
//
//    VPSS_CHN_MODE_S vpss_chn_mode;
//    memset(&vpss_chn_mode, 0, sizeof(VPSS_CHN_MODE_S));
//    vpss_chn_mode.enChnMode = VPSS_CHN_MODE_USER;
//    vpss_chn_mode.u32Width = sensor_config.vichn.dest_size_width;
//    vpss_chn_mode.u32Height = sensor_config.vichn.dest_size_height;
//    vpss_chn_mode.bDouble = HI_FALSE;
//    vpss_chn_mode.enPixelFormat = sensor_config.vichn.pix_format;
//    vpss_chn_mode.enCompressMode = sensor_config.vichn.compress_mode;
//    s32Ret = HI_MPI_VPSS_SetChnMode(state->vpss_grp, state->vpss_chn, &vpss_chn_mode);
//    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_SetChnMode failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
//
//    s32Ret = HI_MPI_VPSS_EnableChn(state->vpss_grp, state->vpss_chn);
//    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_EnableChn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    // OSD
    if (app_config.osd_enable) {
        RGN_HANDLE rgn_handle = 0;

        RGN_ATTR_S rgn_attr;
        memset(&rgn_attr, 0, sizeof(RGN_ATTR_S));
        rgn_attr.enType = COVER_RGN;
        s32Ret = HI_MPI_RGN_Create(rgn_handle, &rgn_attr);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_RGN_Create failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

        MPP_CHN_S mpp_chn;
        memset(&mpp_chn, 0, sizeof(MPP_CHN_S));
        mpp_chn.enModId = HI_ID_VPSS;
        mpp_chn.s32DevId = state->vpss_grp;
        mpp_chn.s32ChnId = 0;

        RGN_CHN_ATTR_S rgn_chn_attr;
        memset(&rgn_chn_attr, 0, sizeof(RGN_CHN_ATTR_S));
        rgn_chn_attr.enType = COVER_RGN;
        rgn_chn_attr.bShow = true;
        rgn_chn_attr.unChnAttr.stCoverChn.enCoverType = AREA_QUAD_RANGLE;
        rgn_chn_attr.unChnAttr.stCoverChn.stQuadRangle.bSolid = HI_TRUE;
        rgn_chn_attr.unChnAttr.stCoverChn.stQuadRangle.u32Thick = 2;
        rgn_chn_attr.unChnAttr.stCoverChn.stQuadRangle.stPoint[0].s32X = 50;
        rgn_chn_attr.unChnAttr.stCoverChn.stQuadRangle.stPoint[0].s32Y = 0;
        rgn_chn_attr.unChnAttr.stCoverChn.stQuadRangle.stPoint[1].s32X = 50 + 50;
        rgn_chn_attr.unChnAttr.stCoverChn.stQuadRangle.stPoint[1].s32Y = 50;
        rgn_chn_attr.unChnAttr.stCoverChn.stQuadRangle.stPoint[2].s32X = 50 + 50;
        rgn_chn_attr.unChnAttr.stCoverChn.stQuadRangle.stPoint[2].s32Y = 300;
        rgn_chn_attr.unChnAttr.stCoverChn.stQuadRangle.stPoint[3].s32X = 50;
        rgn_chn_attr.unChnAttr.stCoverChn.stQuadRangle.stPoint[3].s32Y = 200;
        rgn_chn_attr.unChnAttr.stCoverChn.u32Color         = 0x000000ff;
        s32Ret = HI_MPI_RGN_AttachToChn(rgn_handle, &mpp_chn, &rgn_chn_attr);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_RGN_AttachToChn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    }

    // config venc
    if (app_config.mp4_enable) {
        VPSS_CHN vpss_chn = state->next_free_channel;
        state->next_free_channel++;
        s32Ret = create_vpss_chn(state->vpss_grp, vpss_chn, sensor_config.isp.isp_frame_rate, app_config.mp4_fps);
        if (HI_SUCCESS != s32Ret) { printf("create_vpss_chn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

        unsigned int width = app_config.mp4_width;
        unsigned int height = app_config.mp4_height;

        VENC_CHN venc_chn = vpss_chn;
        VENC_CHN_ATTR_S venc_chn_attr;
        memset(&venc_chn_attr, 0, sizeof(VENC_CHN_ATTR_S));

        VENC_ATTR_H264_S h264_attr;
        memset(&h264_attr, 0, sizeof(VENC_ATTR_H264_S));
        h264_attr.u32MaxPicWidth = width;
        h264_attr.u32MaxPicHeight = height;
        h264_attr.u32PicWidth = width;          /*the picture width*/
        h264_attr.u32PicHeight = height;        /*the picture height*/
        h264_attr.u32BufSize  = width * height; /*stream buffer size*/
        h264_attr.u32Profile  = 0;              /*0: baseline; 1:MP; 2:HP;  3:svc_t */
        h264_attr.bByFrame = HI_TRUE;           /*get stream mode is slice mode or frame mode?*/
        h264_attr.u32BFrameNum = 0;             /* 0: not support B frame; >=1: number of B frames */
        h264_attr.u32RefNum = 1;                /* 0: default; number of refrence frame*/

        VENC_ATTR_H264_CBR_S h264_cbr;
        memset(&h264_cbr, 0, sizeof(VENC_ATTR_H264_CBR_S));
        h264_cbr.u32Gop            = app_config.mp4_fps;
        h264_cbr.u32StatTime       = 1;             /* stream rate statics time(s) */
        h264_cbr.u32SrcFrmRate      = app_config.mp4_fps;   /* input (vi) frame rate */
        h264_cbr.fr32DstFrmRate = app_config.mp4_fps;       /* target frame rate */
        h264_cbr.u32BitRate = app_config.mp4_bitrate;
        h264_cbr.u32FluctuateLevel = 0;             /* average bit rate */

        venc_chn_attr.stVeAttr.enType = PT_H264;
        venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
        venc_chn_attr.stVeAttr.stAttrH264e = h264_attr;
        venc_chn_attr.stRcAttr.stAttrH264Cbr = h264_cbr;
        s32Ret = HI_MPI_VENC_CreateChn(venc_chn, &venc_chn_attr);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_CreateChn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

        MPP_CHN_S src_chn;
        memset(&src_chn, 0, sizeof(MPP_CHN_S));
        src_chn.enModId = HI_ID_VPSS;
        src_chn.s32DevId = 0;
        src_chn.s32ChnId = vpss_chn;
        MPP_CHN_S dest_chn;
        memset(&dest_chn, 0, sizeof(MPP_CHN_S));
        dest_chn.enModId = HI_ID_VENC;
        dest_chn.s32DevId = 0;
        dest_chn.s32ChnId = venc_chn;
        s32Ret = HI_MPI_SYS_Bind(&src_chn, &dest_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_Bind failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }

        s32Ret = HI_MPI_VENC_StartRecvPic(venc_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_StartRecvPic failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
        VencEnabled[venc_chn] = true;
    }

    if (app_config.jpeg_enable) {
        VPSS_CHN vpss_chn = state->next_free_channel;
        state->next_free_channel++;
        s32Ret = create_vpss_chn(state->vpss_grp, vpss_chn, sensor_config.isp.isp_frame_rate, app_config.jpeg_fps);
        if (HI_SUCCESS != s32Ret) { printf("create_vpss_chn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

        unsigned int width = app_config.jpeg_width;
        unsigned int height = app_config.jpeg_height;

        VENC_CHN venc_chn = vpss_chn;
        VENC_ATTR_JPEG_S jpeg_attr;
        memset(&jpeg_attr, 0, sizeof(VENC_ATTR_JPEG_S));
        jpeg_attr.u32MaxPicWidth  = width;
        jpeg_attr.u32MaxPicHeight = height;
        jpeg_attr.u32PicWidth  = width;
        jpeg_attr.u32PicHeight = height;
        jpeg_attr.u32BufSize = (((width+15)>>4)<<4) * (((height+15)>>4)<<4);
        jpeg_attr.bByFrame = HI_TRUE; /*get stream mode is field mode  or frame mode*/
        jpeg_attr.bSupportDCF = HI_FALSE;

        VENC_CHN_ATTR_S venc_chn_attr;
        memset(&venc_chn_attr, 0, sizeof(VENC_CHN_ATTR_S));
        venc_chn_attr.stVeAttr.enType = PT_JPEG;
        memcpy(&venc_chn_attr.stVeAttr.stAttrJpeg, &jpeg_attr, sizeof(VENC_ATTR_JPEG_S));

        s32Ret = HI_MPI_VENC_CreateChn(venc_chn, &venc_chn_attr);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_CreateChn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
        {
            MPP_CHN_S src_chn;
            memset(&src_chn, 0, sizeof(MPP_CHN_S));
            src_chn.enModId = HI_ID_VPSS;
            src_chn.s32DevId = 0;
            src_chn.s32ChnId = vpss_chn;
            MPP_CHN_S dest_chn;
            memset(&dest_chn, 0, sizeof(MPP_CHN_S));
            dest_chn.enModId = HI_ID_VENC;
            dest_chn.s32DevId = 0;
            dest_chn.s32ChnId = venc_chn;

            s32Ret = HI_MPI_SYS_Bind(&src_chn, &dest_chn);
            if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_Bind failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }
        }
        s32Ret = HI_MPI_VENC_StartRecvPic(venc_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_StartRecvPic failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
        VencEnabled[venc_chn] = true;
    }

    if (app_config.mjpeg_enable) {
        VPSS_CHN vpss_chn = state->next_free_channel;
        state->next_free_channel++;
        s32Ret = create_vpss_chn(state->vpss_grp, vpss_chn, sensor_config.isp.isp_frame_rate, app_config.mjpeg_fps);
        if (HI_SUCCESS != s32Ret) { printf("create_vpss_chn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

        VENC_CHN venc_chn = vpss_chn;
        VENC_CHN_ATTR_S venc_chn_attr;
        memset(&venc_chn_attr, 0, sizeof(VENC_CHN_ATTR_S));

        unsigned int width = app_config.mjpeg_width;
        unsigned int height = app_config.mjpeg_height;

        VENC_ATTR_MJPEG_S mjpeg_attr;
        memset(&mjpeg_attr, 0, sizeof(VENC_ATTR_MJPEG_S));
        mjpeg_attr.u32MaxPicWidth = width;
        mjpeg_attr.u32MaxPicHeight = height;
        mjpeg_attr.u32PicWidth = width;
        mjpeg_attr.u32PicHeight = height;
        mjpeg_attr.u32BufSize = (((width + 15) >> 4) << 4) * (((height + 15) >> 4) << 4);
        mjpeg_attr.bByFrame = HI_TRUE;  /*get stream mode is field mode  or frame mode*/

        venc_chn_attr.stVeAttr.enType = PT_MJPEG;
        memcpy(&venc_chn_attr.stVeAttr.stAttrMjpeg, &mjpeg_attr, sizeof(VENC_ATTR_MJPEG_S));
        venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
        venc_chn_attr.stRcAttr.stAttrMjpegeCbr.u32StatTime = 1;
        venc_chn_attr.stRcAttr.stAttrMjpegeCbr.u32SrcFrmRate = app_config.mjpeg_fps;
        venc_chn_attr.stRcAttr.stAttrMjpegeCbr.fr32DstFrmRate = app_config.mjpeg_fps;
        venc_chn_attr.stRcAttr.stAttrMjpegeCbr.u32FluctuateLevel = 0;
        venc_chn_attr.stRcAttr.stAttrMjpegeCbr.u32BitRate = app_config.mjpeg_bitrate;
        s32Ret = HI_MPI_VENC_CreateChn(venc_chn, &venc_chn_attr);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_CreateChn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

        MPP_CHN_S src_chn;
        memset(&src_chn, 0, sizeof(MPP_CHN_S));
        src_chn.enModId = HI_ID_VPSS;
        src_chn.s32DevId = 0;
        src_chn.s32ChnId = vpss_chn;
        MPP_CHN_S dest_chn;
        memset(&dest_chn, 0, sizeof(MPP_CHN_S));
        dest_chn.enModId = HI_ID_VENC;
        dest_chn.s32DevId = 0;
        dest_chn.s32ChnId = venc_chn;
        s32Ret = HI_MPI_SYS_Bind(&src_chn, &dest_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_Bind failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }

        s32Ret = HI_MPI_VENC_StartRecvPic(venc_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_StartRecvPic failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
        VencEnabled[venc_chn] = true;
    }

    {
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        size_t stacksize;
        pthread_attr_getstacksize(&thread_attr,&stacksize);
        size_t new_stacksize = app_config.venc_stream_thread_stack_size;
        if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) { printf("Error:  Can't set stack size %ld\n", new_stacksize); }
        if (0 != pthread_create(&gs_VencPid, &thread_attr, VENC_GetVencStreamProc, NULL)) { printf("%s: create VENC_GetVencStreamProc running thread failed!\n", __FUNCTION__); return EXIT_FAILURE; }
        if (pthread_attr_setstacksize(&thread_attr, stacksize)) { printf("Error:  Can't set stack size %ld\n", stacksize); }
        pthread_attr_destroy(&thread_attr);
    }
    printf("Start sdk Ok!\n");
    return EXIT_SUCCESS;
}

int disable_channel(uint32_t vpss_grp, uint32_t channel_id) {
    HI_S32 s32Ret;

    VENC_CHN venc_chn = channel_id;
    VENC_CHN vpss_chn = venc_chn;
    VencEnabled[venc_chn] = false;
    s32Ret = HI_MPI_VENC_StopRecvPic(venc_chn);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_StopRecvPic failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    {
        MPP_CHN_S src_chn;
        src_chn.enModId = HI_ID_VPSS;
        src_chn.s32DevId = 0;
        src_chn.s32ChnId = vpss_chn;
        MPP_CHN_S dest_chn;
        dest_chn.enModId = HI_ID_VENC;
        dest_chn.s32DevId = 0;
        dest_chn.s32ChnId = venc_chn;
        s32Ret = HI_MPI_SYS_UnBind(&src_chn, &dest_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_UnBind failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    }
    s32Ret = HI_MPI_VENC_DestroyChn(venc_chn);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_DestroyChn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VPSS_DisableChn(vpss_grp, vpss_chn);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_DisableChn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
};

int stop_sdk(struct SDKState *state) {
    HI_S32 s32Ret;
    pthread_join(gs_VencPid, NULL);

    while (state->next_free_channel > 0) {
        state->next_free_channel--;
        disable_channel(state->vpss_grp, state->next_free_channel);
    }

    {
        MPP_CHN_S src_chn;
        src_chn.enModId = HI_ID_VIU;
        src_chn.s32DevId = 0;
        src_chn.s32ChnId = 0;
        MPP_CHN_S dest_chn;
        dest_chn.enModId = HI_ID_VPSS;
        dest_chn.s32DevId = 0;
        dest_chn.s32ChnId = 0;
        s32Ret = HI_MPI_SYS_UnBind(&src_chn, &dest_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_UnBind failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    }

    if(app_config.motion_detect_enable) {
        s32Ret = motion_detect_deinit();
        if (HI_SUCCESS != s32Ret) { printf("motion_detect_deinit failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    }

    s32Ret = HI_MPI_VPSS_StopGrp(state->vpss_grp);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_StopGrp failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VPSS_DestroyGrp(state->vpss_grp);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_DestroyGrp failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VI_DisableChn(state->vi_chn);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_DisableChn failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VI_DisableDev(state->vi_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_DisableDev failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    s32Ret = HI_MPI_ISP_Exit(state->isp_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_Exit failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    pthread_join(gs_IspPid, NULL);

    ALG_LIB_S lib;
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_af_lib\0        ");
    s32Ret = HI_MPI_AF_UnRegister(state->isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AF_UnRegister failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_awb_lib\0       ");
    s32Ret = HI_MPI_AWB_UnRegister(state->isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AWB_UnRegister failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_ae_lib\0        ");
    s32Ret = HI_MPI_AE_UnRegister(state->isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AE_UnRegister failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    sensor_unregister_callback();

    s32Ret = HI_MPI_SYS_Exit();
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_Exit failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VB_Exit();
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VB_Exit failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }

    UnloadSensorLibrary();

    printf("Stop sdk Ok!\n");
    return EXIT_SUCCESS;
}
