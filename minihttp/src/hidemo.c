#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>

#include <hi_common.h>
#include <hi_comm_sys.h>
#include <hi_comm_vb.h>
#include <hi_comm_isp.h>
#include <hi_comm_vi.h>
#include <hi_comm_vo.h>
#include <hi_comm_venc.h>
#include <hi_comm_vpss.h>
#include <hi_comm_vdec.h>
#include <hi_comm_region.h>
#include <hi_comm_adec.h>
#include <hi_comm_aenc.h>
#include <hi_comm_ai.h>
#include <hi_comm_ao.h>
#include <hi_comm_aio.h>
#include <hi_defines.h>

#include <mpi_sys.h>
#include <mpi_vb.h>
#include <mpi_vi.h>
#include <mpi_vo.h>
#include <mpi_venc.h>
#include <mpi_vpss.h>
#include <mpi_vdec.h>
#include <mpi_region.h>
#include <mpi_adec.h>
#include <mpi_aenc.h>
#include <mpi_ai.h>
#include <mpi_ao.h>
#include <mpi_isp.h>
#include <mpi_ive.h>
#include <mpi_ae.h>
#include <mpi_awb.h>
#include <mpi_af.h>
#include <hi_vreg.h>
#include <hi_sns_ctrl.h>
#include <hi_mipi.h>

#include "sensors.h"
#include "server.h"

HI_VOID* Test_ISP_Run(HI_VOID *param) {
    ISP_DEV isp_dev = 0;
    HI_S32 s32Ret = HI_MPI_ISP_Run(isp_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_Run faild with %#x!\n", s32Ret); }
    printf("Shutdown isp_run thread\n");
    return HI_NULL;
}

HI_S32 VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, char *szFilePostfix) {
    HI_S32 s32Ret = HI_SUCCESS;
    if (PT_H264 == enPayload) { strcpy(szFilePostfix, ".h264"); }
    else if (PT_H265 == enPayload) { strcpy(szFilePostfix, ".h265"); }
    else if (PT_JPEG == enPayload) { strcpy(szFilePostfix, ".jpg"); }
    else if (PT_MJPEG == enPayload) { strcpy(szFilePostfix, ".mjp");  }
    else if (PT_MP4VIDEO == enPayload) { strcpy(szFilePostfix, ".mp4"); }
    else { printf("payload type err!\n"); return HI_FAILURE; }
    return HI_SUCCESS;
}
HI_S32 VENC_SaveH264(FILE* fpH264File, VENC_STREAM_S *pstStream) {
    HI_S32 i;
    for (i = 0; i < pstStream->u32PackCount; i++) {
        write(write_pump_h264_fd, pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset, pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset);
        fwrite(pstStream->pstPack[i].pu8Addr+pstStream->pstPack[i].u32Offset, pstStream->pstPack[i].u32Len-pstStream->pstPack[i].u32Offset, 1, fpH264File);
        fflush(fpH264File);
    }
    return HI_SUCCESS;
}

char mjpeg_buf[5*1024*1024];
HI_S32 VENC_SaveMJpeg(FILE* fpMJpegFile, VENC_STREAM_S *pstStream) {
    VENC_PACK_S*  pstData;
    HI_U32 i;
    ssize_t buf_size = 0;
    //fwrite(g_SOI, 1, sizeof(g_SOI), fpJpegFile); //in Hi3531, user needn't write SOI!
    for (i = 0; i < pstStream->u32PackCount; i++) {
        pstData = &pstStream->pstPack[i];
        //  write(write_pump_fd, pstData->pu8Addr+pstData->u32Offset, pstData->u32Len-pstData->u32Offset);
        memcpy(mjpeg_buf + buf_size, pstData->pu8Addr+pstData->u32Offset, pstData->u32Len-pstData->u32Offset);
        buf_size += pstData->u32Len-pstData->u32Offset;
//        fwrite(pstData->pu8Addr+pstData->u32Offset, pstData->u32Len-pstData->u32Offset, 1, fpMJpegFile);
//        fflush(fpMJpegFile);
    }
    write(write_pump_mjpeg_fd, mjpeg_buf, buf_size);
    return HI_SUCCESS;
}
HI_S32 VENC_SaveStream(PAYLOAD_TYPE_E enType,FILE *pFd, VENC_STREAM_S *pstStream) {
    HI_S32 s32Ret;
    if (PT_H264 == enType) { s32Ret = VENC_SaveH264(pFd, pstStream); }
    else if (PT_MJPEG == enType) { s32Ret = VENC_SaveMJpeg(pFd, pstStream); }
    // else if (PT_H265 == enType) { s32Ret = SAMPLE_COMM_VENC_SaveH265(pFd, pstStream); }
    else { return HI_FAILURE; }
    return s32Ret;
}
HI_VOID* VENC_GetVencStreamProc(HI_VOID *p) {
    HI_S32 i;
    HI_S32 s32ChnTotal;
    VENC_CHN_ATTR_S stVencChnAttr;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64];
    FILE *pFile[VENC_MAX_CHN_NUM];
    char szFilePostfix[10];
    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_CHN VencChn;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
    s32ChnTotal = 1;

    if (s32ChnTotal >= VENC_MAX_CHN_NUM) { printf("input count invaild\n"); return NULL; }
    for (i = 0; i < s32ChnTotal; i++) {
        VencChn = i;
        s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
        if(s32Ret != HI_SUCCESS) { printf("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", VencChn, s32Ret); return NULL; }
        enPayLoadType[i] = stVencChnAttr.stVeAttr.enType;

        s32Ret = VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
        if(s32Ret != HI_SUCCESS) { printf("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", stVencChnAttr.stVeAttr.enType, s32Ret); return NULL; }
        sprintf(aszFileName[i], "/tmp/stream_chn%d%s", i, szFilePostfix);
        pFile[i] = fopen(aszFileName[i], "wb");
        if (!pFile[i]) { printf("open file[%s] failed!\n", aszFileName[i]); return NULL; }

        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] < 0) { printf("HI_MPI_VENC_GetFd failed with %#x!\n", VencFd[i]); return NULL; }
        if (maxfd <= VencFd[i]) {  maxfd = VencFd[i]; }
    }

    while (keepRunning) {
        FD_ZERO(&read_fds);
        for (i = 0; i < s32ChnTotal; i++) { FD_SET(VencFd[i], &read_fds); }

        TimeoutVal.tv_sec  = 5;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0) { printf("select failed!\n"); break; }
        else if (s32Ret == 0) { printf("get sample_venc stream time out, exit thread\n"); continue; }
        else {
            for (i = 0; i < s32ChnTotal; i++) {
                if (FD_ISSET(VencFd[i], &read_fds)) {
                    memset(&stStream, 0, sizeof(stStream));
                    s32Ret = HI_MPI_VENC_Query(i, &stStat);
                    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_Query chn[%d] failed with %#x!\n", i, s32Ret); break; }

                    if(0 == stStat.u32CurPacks) { printf("NOTE: Current  frame is NULL!\n"); continue; }

                    stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack) { printf("malloc stream pack failed!\n"); break; }
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = HI_MPI_VENC_GetStream(i, &stStream, HI_TRUE);
                    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_GetStream faild with %#x!\n", s32Ret); free(stStream.pstPack); stStream.pstPack = NULL; break; }
                    s32Ret = VENC_SaveStream(enPayLoadType[i], pFile[i], &stStream);
                    if (HI_SUCCESS != s32Ret) { printf("VENC_SaveStream faild with %#x!\n", s32Ret); free(stStream.pstPack); stStream.pstPack = NULL; break; }
                    s32Ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
                    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_ReleaseStream faild with %#x!\n", s32Ret); free(stStream.pstPack); stStream.pstPack = NULL;  break; }
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                }
            }
        }
    }
    for (i = 0; i < s32ChnTotal; i++) fclose(pFile[i]);
    printf("Shutdown hisdk venc thread\n");
    return NULL;
}

static pthread_t gs_VencPid;

int main(int argc, char *argv[]) {

    start_server();

    LoadSensorLibrary("libsns_imx222.so");

    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();

    VB_CONF_S vb_conf;
    memset(&vb_conf,0,sizeof(VB_CONF_S));
    vb_conf.u32MaxPoolCnt = 128;
    vb_conf.astCommPool[0].u32BlkSize = 3159360;
    vb_conf.astCommPool[0].u32BlkCnt = 10;

    HI_S32 s32Ret = HI_MPI_VB_SetConf(&vb_conf);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VB_SetConf faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    VB_SUPPLEMENT_CONF_S supplement_conf;
    memset(&supplement_conf, 0, sizeof(VB_SUPPLEMENT_CONF_S));
    supplement_conf.u32SupplementConf = 1;
    s32Ret = HI_MPI_VB_SetSupplementConf(&supplement_conf);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VB_SetSupplementConf faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VB_Init();
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VB_Init faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    MPP_SYS_CONF_S sys_conf;
    memset(&sys_conf,0,sizeof(MPP_SYS_CONF_S));
    sys_conf.u32AlignWidth = 64;
    s32Ret = HI_MPI_SYS_SetConf(&sys_conf);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_SetConf faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    s32Ret = HI_MPI_SYS_Init();
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_Init faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    /* mipi reset unrest */
    HI_S32 fd = open("/dev/hi_mipi", O_RDWR);
    if (fd < 0) { printf("warning: open hi_mipi dev failed\n"); return EXIT_FAILURE; }
    combo_dev_attr_t MIPI_CMOS3V3_ATTR = { .input_mode = INPUT_MODE_CMOS_33V, { } };
    if (ioctl(fd, _IOW('m', 0x01, combo_dev_attr_t), &MIPI_CMOS3V3_ATTR)) {
        printf("set mipi attr failed\n");
        close(fd);
        return EXIT_FAILURE;
    }
    close(fd);

    sensor_register_callback();


    ISP_DEV isp_dev = 0;
    ALG_LIB_S lib;
    memset(&lib,0,sizeof(ALG_LIB_S));
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_ae_lib\0        ");
    s32Ret = HI_MPI_AE_Register(isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AE_Register faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_awb_lib\0       ");
    s32Ret = HI_MPI_AWB_Register(isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AWB_Register faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_af_lib\0        ");
    s32Ret = HI_MPI_AF_Register(isp_dev, &lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AF_Register faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    s32Ret = HI_MPI_ISP_MemInit(isp_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_MemInit faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    ISP_WDR_MODE_S wdr_mode;
    memset(&wdr_mode,0,sizeof(ISP_WDR_MODE_S));
    wdr_mode.enWDRMode = WDR_MODE_NONE;
    s32Ret = HI_MPI_ISP_SetWDRMode(isp_dev, &wdr_mode);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_SetWDRMode faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    ISP_PUB_ATTR_S pub_attr;
    memset(&pub_attr,0,sizeof(ISP_PUB_ATTR_S));
    pub_attr.enBayer = BAYER_RGGB;
    pub_attr.f32FrameRate = 30.0;
    pub_attr.stWndRect.s32X = 200;
    pub_attr.stWndRect.s32Y = 20;
    pub_attr.stWndRect.u32Width = 1920;
    pub_attr.stWndRect.u32Height = 1080;
    s32Ret = HI_MPI_ISP_SetPubAttr(isp_dev, &pub_attr);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_SetPubAttr faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    s32Ret = HI_MPI_ISP_Init(isp_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_Init faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    pthread_t gs_IspPid = 0;
    if (0 != pthread_create(&gs_IspPid, 0, (void* (*)(void*))Test_ISP_Run, NULL)) { printf("%s: create isp running thread failed!\n", __FUNCTION__); return EXIT_FAILURE; }
    usleep(1000);



    VI_DEV vi_dev = 0;
    VI_DEV_ATTR_S vi_dev_attr;
    memset(&vi_dev_attr,0,sizeof(VI_DEV_ATTR_S));
    vi_dev_attr.enIntfMode = VI_MODE_DIGITAL_CAMERA;
    vi_dev_attr.enWorkMode = VI_WORK_MODE_1Multiplex;
    vi_dev_attr.au32CompMask[0] = 0x3FF0000;
    vi_dev_attr.au32CompMask[1] = 0x0;
    vi_dev_attr.enScanMode = VI_SCAN_PROGRESSIVE;
    vi_dev_attr.s32AdChnId[0] = -1;
    vi_dev_attr.s32AdChnId[1] = -1;
    vi_dev_attr.s32AdChnId[2] = -1;
    vi_dev_attr.s32AdChnId[3] = -1;
    vi_dev_attr.stDevRect.s32X = 0;
    vi_dev_attr.stDevRect.s32Y = 0;
    vi_dev_attr.stDevRect.u32Width = 1920;
    vi_dev_attr.stDevRect.u32Height = 1080;
    vi_dev_attr.enDataSeq = VI_INPUT_DATA_YUYV;
    vi_dev_attr.stSynCfg.enVsync = VI_VSYNC_PULSE;
    vi_dev_attr.stSynCfg.enVsyncNeg = VI_VSYNC_NEG_HIGH;
    vi_dev_attr.stSynCfg.enHsync = VI_HSYNC_VALID_SINGNAL;
    vi_dev_attr.stSynCfg.enHsyncNeg = VI_HSYNC_NEG_HIGH;
    vi_dev_attr.stSynCfg.enVsyncValid = VI_VSYNC_VALID_SINGAL;
    vi_dev_attr.stSynCfg.enVsyncValidNeg = VI_VSYNC_VALID_NEG_HIGH;
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncHfb = 0;
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncAct = 1920;
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncHbb = 0;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVfb = 0;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVact = 1080;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbb = 0;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbfb = 0;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbact = 0;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbbb = 0;
    vi_dev_attr.enDataPath = VI_PATH_ISP;
    vi_dev_attr.enInputDataType = VI_DATA_TYPE_RGB;
    vi_dev_attr.bDataRev = HI_FALSE;
    vi_dev_attr.stDevRect.s32X = 200;
    vi_dev_attr.stDevRect.s32Y = 20;
    vi_dev_attr.stDevRect.u32Width = 1920;
    vi_dev_attr.stDevRect.u32Height = 1080;

    s32Ret = HI_MPI_VI_SetDevAttr(vi_dev, &vi_dev_attr);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_SetDevAttr faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

//    ISP_WDR_MODE_S wdr_mode;
//    memset(&wdr_mode,0,sizeof(ISP_WDR_MODE_S));
//    wdr_mode.enWDRMode = WDR_MODE_NONE;
    s32Ret = HI_MPI_ISP_GetWDRMode(isp_dev, &wdr_mode);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_GetWDRMode faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    // TODO

    VI_WDR_ATTR_S wdr_addr;
    memset(&wdr_addr,0,sizeof(VI_WDR_ATTR_S));
    wdr_addr.enWDRMode = WDR_MODE_NONE;
    wdr_addr.bCompress = HI_FALSE;
    s32Ret = HI_MPI_VI_SetWDRAttr(vi_dev, &wdr_addr);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_SetWDRAttr faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VI_EnableDev(vi_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_EnableDev faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    VI_CHN vi_chn = 0;
    VI_CHN_ATTR_S chn_attr;
    memset(&chn_attr,0,sizeof(VI_CHN_ATTR_S));
    chn_attr.stCapRect.s32X = 0;
    chn_attr.stCapRect.s32Y = 0;
    chn_attr.stCapRect.u32Width = 1920;
    chn_attr.stCapRect.u32Height = 1080;
    chn_attr.stDestSize.u32Width = 1920;
    chn_attr.stDestSize.u32Height = 1080;
    chn_attr.enCapSel = VI_CAPSEL_BOTH;
    chn_attr.enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    chn_attr.bMirror = HI_FALSE;
    chn_attr.bFlip = HI_FALSE;
    chn_attr.s32SrcFrameRate = -1;
    chn_attr.s32DstFrameRate = -1;
    chn_attr.enCompressMode = COMPRESS_MODE_NONE;
    s32Ret = HI_MPI_VI_SetChnAttr(vi_chn, &chn_attr);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_SetChnAttr faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VI_EnableChn(vi_chn);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_EnableChn faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    VPSS_GRP vpss_grp = 0;
    VPSS_GRP_ATTR_S vpss_grp_attr;
    memset(&vpss_grp_attr,0,sizeof(VPSS_GRP_ATTR_S));
    vpss_grp_attr.u32MaxW = 1920;
    vpss_grp_attr.u32MaxH = 1080;
    vpss_grp_attr.enPixFmt = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    vpss_grp_attr.bIeEn = HI_FALSE;
    vpss_grp_attr.bDciEn = HI_FALSE;
    vpss_grp_attr.bNrEn = HI_TRUE;
    vpss_grp_attr.bHistEn = HI_FALSE;
    vpss_grp_attr.enDieMode = VPSS_DIE_MODE_NODIE;
    s32Ret = HI_MPI_VPSS_CreateGrp(vpss_grp, &vpss_grp_attr);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_CreateGrp faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    VPSS_NR_PARAM_U nr_param;
    memset(&nr_param,0,sizeof(VPSS_NR_PARAM_U));
    s32Ret = HI_MPI_VPSS_GetNRParam(vpss_grp, &nr_param);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_GetNRParam faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    nr_param.stNRParam_V1.s32YPKStr = 0;
    nr_param.stNRParam_V1.s32YSFStr = 100;
    nr_param.stNRParam_V1.s32YTFStr = 64;
    nr_param.stNRParam_V1.s32TFStrMax = 12;
    nr_param.stNRParam_V1.s32TFStrMov = 0;
    nr_param.stNRParam_V1.s32YSmthStr = 0;
    nr_param.stNRParam_V1.s32YSmthRat = 16;
    nr_param.stNRParam_V1.s32YSFStrDlt = 0;
    nr_param.stNRParam_V1.s32YSFStrDl = 0;
    nr_param.stNRParam_V1.s32YTFStrDlt = 0;
    nr_param.stNRParam_V1.s32YTFStrDl = 0;
    nr_param.stNRParam_V1.s32YSFBriRat = 24;
    nr_param.stNRParam_V1.s32CSFStr = 32;
    nr_param.stNRParam_V1.s32CTFstr = 0;
    nr_param.stNRParam_V1.s32YTFMdWin = 1;
    s32Ret = HI_MPI_VPSS_SetNRParam(vpss_grp, &nr_param);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_SetNRParam faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VPSS_StartGrp(vpss_grp);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_StartGrp faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    MPP_CHN_S src_chn;
    src_chn.enModId = HI_ID_VIU;
    src_chn.s32DevId = 0;
    src_chn.s32ChnId = 0;
    MPP_CHN_S dest_chn;
    dest_chn.enModId = HI_ID_VPSS;
    dest_chn.s32DevId = 0;
    dest_chn.s32ChnId = 0;
    s32Ret = HI_MPI_SYS_Bind(&src_chn, &dest_chn);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_Bind faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    VPSS_CHN vpss_chn = 0;
    VPSS_CHN_ATTR_S vpss_chn_attr;
    memset(&vpss_chn_attr,0,sizeof(VPSS_CHN_ATTR_S));
    vpss_chn_attr.bSpEn = HI_FALSE;
    vpss_chn_attr.bBorderEn = HI_FALSE;
    vpss_chn_attr.bMirror = HI_FALSE;
    vpss_chn_attr.bFlip = HI_FALSE;
    vpss_chn_attr.s32SrcFrameRate = -1;
    vpss_chn_attr.s32DstFrameRate = -1;
    vpss_chn_attr.stBorder.u32TopWidth = 0;
    vpss_chn_attr.stBorder.u32BottomWidth = 0;
    vpss_chn_attr.stBorder.u32LeftWidth = 0;
    vpss_chn_attr.stBorder.u32RightWidth = 0;
    vpss_chn_attr.stBorder.u32Color = 0;
    s32Ret = HI_MPI_VPSS_SetChnAttr(vpss_grp, vpss_chn, &vpss_chn_attr);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_SetChnAttr faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    VPSS_CHN_MODE_S vpss_chn_mode;
    memset(&vpss_chn_mode,0,sizeof(VPSS_CHN_MODE_S));
    vpss_chn_mode.enChnMode = VPSS_CHN_MODE_USER;
    vpss_chn_mode.u32Width = 1920;
    vpss_chn_mode.u32Height = 1080;
    vpss_chn_mode.bDouble = HI_FALSE;
    vpss_chn_mode.enPixelFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    vpss_chn_mode.enCompressMode = COMPRESS_MODE_NONE;
    s32Ret = HI_MPI_VPSS_SetChnMode(vpss_grp, vpss_chn, &vpss_chn_mode);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_SetChnMode faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VPSS_EnableChn(vpss_grp, vpss_chn);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_EnableChn faild with %#x!\n", s32Ret); return EXIT_FAILURE; }



    // config venc

    int picWidth = 1920;
    int picHeight = 1080;
    int frm_rate = 30;


    {
        VENC_CHN venc_chn = 0;
        VENC_CHN_ATTR_S venc_chn_attr;
        memset(&venc_chn_attr, 0, sizeof(VENC_CHN_ATTR_S));

        VENC_ATTR_MJPEG_S mjpeg_attr;
        memset(&mjpeg_attr, 0, sizeof(VENC_ATTR_MJPEG_S));
        mjpeg_attr.u32MaxPicWidth = picWidth;
        mjpeg_attr.u32MaxPicHeight = picHeight;
        mjpeg_attr.u32PicWidth = picWidth;
        mjpeg_attr.u32PicHeight = picHeight;
        mjpeg_attr.u32BufSize = (((picWidth + 15) >> 4) << 4) * (((picHeight + 15) >> 4) << 4);
        mjpeg_attr.bByFrame = HI_TRUE;  /*get stream mode is field mode  or frame mode*/

        venc_chn_attr.stVeAttr.enType = PT_MJPEG;
        memcpy(&venc_chn_attr.stVeAttr.stAttrMjpeg, &mjpeg_attr, sizeof(VENC_ATTR_MJPEG_S));
        venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
        venc_chn_attr.stRcAttr.stAttrMjpegeCbr.u32StatTime = 1;
        venc_chn_attr.stRcAttr.stAttrMjpegeCbr.u32SrcFrmRate = frm_rate;
        venc_chn_attr.stRcAttr.stAttrMjpegeCbr.fr32DstFrmRate = frm_rate;
        venc_chn_attr.stRcAttr.stAttrMjpegeCbr.u32FluctuateLevel = 0;
        venc_chn_attr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024 * 10 * 3;
        s32Ret = HI_MPI_VENC_CreateChn(venc_chn, &venc_chn_attr);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_CreateChn faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    }

    {
        VENC_CHN venc_chn = 1;
        VENC_CHN_ATTR_S venc_chn_attr;
        memset(&venc_chn_attr, 0, sizeof(VENC_CHN_ATTR_S));

        VENC_ATTR_H264_S h264_attr;
        memset(&h264_attr, 0, sizeof(VENC_ATTR_H264_S));
        h264_attr.u32MaxPicWidth = picWidth;
        h264_attr.u32MaxPicHeight = picHeight;
        h264_attr.u32PicWidth = picWidth;/*the picture width*/
        h264_attr.u32PicHeight = picHeight;/*the picture height*/
        h264_attr.u32BufSize  = picWidth * picHeight;/*stream buffer size*/
        h264_attr.u32Profile  = 0;/*0: baseline; 1:MP; 2:HP;  3:svc_t */
        h264_attr.bByFrame = HI_TRUE;/*get stream mode is slice mode or frame mode?*/
        h264_attr.u32BFrameNum = 0;/* 0: not support B frame; >=1: number of B frames */
        h264_attr.u32RefNum = 1;/* 0: default; number of refrence frame*/

        VENC_ATTR_H264_CBR_S h264_cbr;
        memset(&h264_cbr, 0, sizeof(VENC_ATTR_H264_CBR_S));
        h264_cbr.u32Gop            = frm_rate;
        h264_cbr.u32StatTime       = 1; /* stream rate statics time(s) */
        h264_cbr.u32SrcFrmRate      = frm_rate;/* input (vi) frame rate */
        h264_cbr.fr32DstFrmRate = frm_rate;/* target frame rate */
        h264_cbr.u32BitRate = 1024*4;
        h264_cbr.u32FluctuateLevel = 0; /* average bit rate */

        venc_chn_attr.stVeAttr.enType = PT_H264;
        venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
        venc_chn_attr.stVeAttr.stAttrH264e = h264_attr;
        venc_chn_attr.stRcAttr.stAttrH264Cbr = h264_cbr;
        s32Ret = HI_MPI_VENC_CreateChn(venc_chn, &venc_chn_attr);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_CreateChn faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    }

    {
        MPP_CHN_S src_chn;
        memset(&src_chn, 0, sizeof(MPP_CHN_S));
        src_chn.enModId = HI_ID_VPSS;
        src_chn.s32DevId = 0;
        src_chn.s32ChnId = 0;
        MPP_CHN_S dest_chn;
        memset(&dest_chn, 0, sizeof(MPP_CHN_S));
        dest_chn.enModId = HI_ID_VENC;
        dest_chn.s32DevId = 0;
        dest_chn.s32ChnId = 0;

        s32Ret = HI_MPI_SYS_Bind(&src_chn, &dest_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_Bind faild with %#x!\n", s32Ret); return HI_FAILURE; }
    }


    {
        MPP_CHN_S src_chn;
        memset(&src_chn, 0, sizeof(MPP_CHN_S));
        src_chn.enModId = HI_ID_VPSS;
        src_chn.s32DevId = 0;
        src_chn.s32ChnId = 0;
        MPP_CHN_S dest_chn;
        memset(&dest_chn, 0, sizeof(MPP_CHN_S));
        dest_chn.enModId = HI_ID_VENC;
        dest_chn.s32DevId = 0;
        dest_chn.s32ChnId = 1;

        s32Ret = HI_MPI_SYS_Bind(&src_chn, &dest_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_Bind faild with %#x!\n", s32Ret); return HI_FAILURE; }
    }

    s32Ret = HI_MPI_VENC_StartRecvPic(0);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_StartRecvPic faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    s32Ret = HI_MPI_VENC_StartRecvPic(1);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_StartRecvPic faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    pthread_create(&gs_VencPid, 0, VENC_GetVencStreamProc, NULL);

    while(keepRunning) sleep(1);

    {
        MPP_CHN_S src_chn;
        src_chn.enModId = HI_ID_VPSS;
        src_chn.s32DevId = 0;
        src_chn.s32ChnId = 0;
        MPP_CHN_S dest_chn;
        dest_chn.enModId = HI_ID_VENC;
        dest_chn.s32DevId = 0;
        dest_chn.s32ChnId = 0;
        s32Ret = HI_MPI_SYS_UnBind(&src_chn, &dest_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_UnBind faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    }

    {
        MPP_CHN_S src_chn;
        src_chn.enModId = HI_ID_VPSS;
        src_chn.s32DevId = 0;
        src_chn.s32ChnId = 0;
        MPP_CHN_S dest_chn;
        dest_chn.enModId = HI_ID_VENC;
        dest_chn.s32DevId = 0;
        dest_chn.s32ChnId = 1;
        s32Ret = HI_MPI_SYS_UnBind(&src_chn, &dest_chn);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_UnBind faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    }

    s32Ret = HI_MPI_VENC_StopRecvPic(0);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_StopRecvPic faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VENC_DestroyChn(0);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_DestroyChn faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VENC_StopRecvPic(1);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_StopRecvPic faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VENC_DestroyChn(1);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_DestroyChn faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VPSS_DisableChn(vpss_grp, vpss_chn);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_DisableChn faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

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
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_UnBind faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    }

    s32Ret = HI_MPI_VPSS_StopGrp(vpss_grp);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_StopGrp faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VPSS_DestroyGrp(vpss_grp);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_DestroyGrp faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VI_DisableChn(vi_chn);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_DisableChn faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VI_DisableDev(vi_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_DisableDev faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    s32Ret = HI_MPI_ISP_Exit(isp_dev);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_Exit faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    pthread_join(gs_IspPid, NULL);

    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_af_lib\0        ");
    s32Ret = HI_MPI_AF_UnRegister(isp_dev, & lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AF_UnRegister faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_awb_lib\0       ");
    s32Ret = HI_MPI_AWB_UnRegister(isp_dev, & lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AWB_UnRegister faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    lib.s32Id = 0;
    strcpy(lib.acLibName, "hisi_ae_lib\0        ");
    s32Ret = HI_MPI_AE_UnRegister(isp_dev, & lib);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_AE_UnRegister faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    sensor_unregister_callback();

    s32Ret = HI_MPI_SYS_Exit();
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_Exit faild with %#x!\n", s32Ret); return EXIT_FAILURE; }
    s32Ret = HI_MPI_VB_Exit();
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VB_Exit faild with %#x!\n", s32Ret); return EXIT_FAILURE; }

    UnloadSensorLibrary();

    printf("Run stop_server..\n");
    stop_server();

    printf("Shutdown main thread\n");
}
