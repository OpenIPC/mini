#include "hi_jpeg.h"
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

#include "hidemo.h"

#include "night.h"

#include "config/app_config.h"

#include "hierrors.h"

#define tag "[hi_jpeg]: "

#define MAX_WIDTH 1920
#define MAX_HEIGHT 1080

VENC_CHN jpeg_venc_chn;
bool jpeg_enable = false;

pthread_mutex_t jpeg_mutex;

int32_t InitJPEG() {
    pthread_mutex_lock(&jpeg_mutex);

    HI_S32 s32Ret;
    jpeg_venc_chn = take_next_free_channel(false);
    s32Ret = create_venc_chn(jpeg_venc_chn, -1, -1);
    if (HI_SUCCESS != s32Ret) {
        printf(tag "create_venc_chn(%d, ...) failed with %#x!\n%s\n", jpeg_venc_chn, s32Ret, hi_errstr(s32Ret));
        pthread_mutex_unlock(&jpeg_mutex);
        return HI_FAILURE;
    }

    VENC_ATTR_JPEG_S jpeg_attr;
    memset(&jpeg_attr, 0, sizeof(VENC_ATTR_JPEG_S));
    jpeg_attr.u32MaxPicWidth  = MAX_WIDTH;
    jpeg_attr.u32MaxPicHeight = MAX_HEIGHT;
    jpeg_attr.u32PicWidth  = app_config.jpeg_width;
    jpeg_attr.u32PicHeight = app_config.jpeg_height;
    jpeg_attr.u32BufSize = (((MAX_WIDTH+15)>>4)<<4) * (((MAX_HEIGHT+15)>>4)<<4);
    jpeg_attr.bByFrame = HI_TRUE; /*get stream mode is field mode  or frame mode*/
    jpeg_attr.bSupportDCF = HI_FALSE;

    VENC_CHN_ATTR_S venc_chn_attr;
    memset(&venc_chn_attr, 0, sizeof(VENC_CHN_ATTR_S));
    venc_chn_attr.stVeAttr.enType = PT_JPEG;
    memcpy(&venc_chn_attr.stVeAttr.stAttrJpeg, &jpeg_attr, sizeof(VENC_ATTR_JPEG_S));

    s32Ret = HI_MPI_VENC_CreateChn(jpeg_venc_chn, &venc_chn_attr);
    if (HI_SUCCESS != s32Ret) {
        printf(tag "HI_MPI_VENC_CreateChn(%d) failed with %#x!\n%s\n", jpeg_venc_chn, s32Ret, hi_errstr(s32Ret));
        pthread_mutex_unlock(&jpeg_mutex);
        return HI_FAILURE;
    }

    jpeg_enable = true;
    pthread_mutex_unlock(&jpeg_mutex);

    return HI_SUCCESS;
}


int32_t DestroyJPEG() {
    pthread_mutex_lock(&jpeg_mutex);
    disable_venc_chn(jpeg_venc_chn);
    jpeg_enable = false;
    pthread_mutex_unlock(&jpeg_mutex);
//    HI_S32 s32Ret;
//
//    s32Ret = HI_MPI_VENC_StopRecvPic(jpeg_venc_chn);
//    // if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_StopRecvPic(%d) failed with %#x!\n%s\n", venc_chn, s32Ret, hi_errstr(s32Ret)); /* return EXIT_FAILURE; */ }
//    {
//        MPP_CHN_S src_chn;
//        src_chn.enModId = HI_ID_VPSS;
//        src_chn.s32DevId = 0;
//        src_chn.s32ChnId = jpeg_vpss_chn;
//        MPP_CHN_S dest_chn;
//        dest_chn.enModId = HI_ID_VENC;
//        dest_chn.s32DevId = 0;
//        dest_chn.s32ChnId = jpeg_venc_chn;
//        s32Ret = HI_MPI_SYS_UnBind(&src_chn, &dest_chn);
//        // if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_UnBind failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); /* return EXIT_FAILURE; */ }
//    }
//    s32Ret = HI_MPI_VENC_DestroyChn(jpeg_venc_chn);
//    // if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_DestroyChn(%d) failed with %#x!\n%s\n", venc_chn,  s32Ret, hi_errstr(s32Ret)); /* return EXIT_FAILURE; */ }
//
//    s32Ret = HI_MPI_VPSS_DisableChn(0, jpeg_vpss_chn);
//    // if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VPSS_DisableChn(%d, %d) failed with %#x!\n%s\n", vpss_grp, vpss_chn, s32Ret, hi_errstr(s32Ret)); /* return EXIT_FAILURE; */ }
}


HI_S32 get_stream(int fd, int venc_chn, struct JpegData *jpeg_buf) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    struct timeval TimeoutVal;
    TimeoutVal.tv_sec  = 2;
    TimeoutVal.tv_usec = 0;
    int sel_res = select(fd + 1, &read_fds, NULL, NULL, &TimeoutVal);
    if (sel_res < 0) { printf(tag "(venc_chn = %d) select failed!\n", venc_chn); return HI_FAILURE; }
    else if (sel_res == 0) { printf(tag "get jpeg stream (chn %d) time out\n", venc_chn); return HI_FAILURE; }

    if (FD_ISSET(fd, &read_fds)) {
        VENC_CHN_STAT_S stStat;
        HI_S32 s32Ret = HI_MPI_VENC_Query(venc_chn, &stStat);
        if (HI_SUCCESS != s32Ret) { printf(tag "HI_MPI_VENC_Query(%d, ...) failed with %#x!\n%s\n", venc_chn, s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }

        if(0 == stStat.u32CurPacks) { printf(tag "NOTE: Current frame is NULL!\n"); return HI_FAILURE; }

        VENC_STREAM_S stStream;
        memset(&stStream, 0, sizeof(stStream));
        stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
        if (NULL == stStream.pstPack) { printf(tag "malloc stream chn[%d] pack failed!\n", venc_chn); return HI_FAILURE; }
        stStream.u32PackCount = stStat.u32CurPacks;
        s32Ret = HI_MPI_VENC_GetStream(venc_chn, &stStream, HI_TRUE);
        if (HI_SUCCESS != s32Ret) { printf(tag "HI_MPI_VENC_GetStream(%d, ...) failed with %#x!\n%s\n", venc_chn, s32Ret, hi_errstr(s32Ret)); free(stStream.pstPack); stStream.pstPack = NULL; return HI_FAILURE; }
        // printf(tag "HI_MPI_VENC_GetStream ok\n");
        {
            jpeg_buf->jpeg_size = 0;
            for (HI_U32 i = 0; i < stStream.u32PackCount; i++) {
                VENC_PACK_S* pack = &stStream.pstPack[i];
                uint32_t pack_len = pack->u32Len - pack->u32Offset;
                uint8_t *pack_data = pack->pu8Addr + pack->u32Offset;

                ssize_t need_size = jpeg_buf->jpeg_size + pack_len;
                // printf(tag "need_size %d bytes\n", need_size);
                if (need_size > jpeg_buf->buf_size) {
                    jpeg_buf->buf = realloc(jpeg_buf->buf, need_size);
                    jpeg_buf->buf_size = need_size;
                }
                memcpy(jpeg_buf->buf + jpeg_buf->jpeg_size, pack_data, pack_len);
                jpeg_buf->jpeg_size += pack_len;
            }
        }
        // printf(tag "copy ok %d bytes\n", jpeg_buf->jpeg_size);

        s32Ret = HI_MPI_VENC_ReleaseStream(venc_chn, &stStream);
        if (HI_SUCCESS != s32Ret) { printf(tag "HI_MPI_VENC_ReleaseStream(%d, ...) failed with %#x!\n%s\n", venc_chn, s32Ret, hi_errstr(s32Ret)); free(stStream.pstPack); stStream.pstPack = NULL; return HI_FAILURE; }
        free(stStream.pstPack);
        stStream.pstPack = NULL;
    }

    return HI_SUCCESS;
}


int32_t request_pic(uint32_t width, uint32_t height, uint32_t qfactor, struct JpegData *jpeg_buf) {
    HI_S32 s32Ret;

    VENC_ATTR_JPEG_S jpeg_attr;
    memset(&jpeg_attr, 0, sizeof(VENC_ATTR_JPEG_S));
    jpeg_attr.u32MaxPicWidth  = MAX_WIDTH;
    jpeg_attr.u32MaxPicHeight = MAX_HEIGHT;
    jpeg_attr.u32PicWidth  = width;
    jpeg_attr.u32PicHeight = height;
    jpeg_attr.u32BufSize = (((MAX_WIDTH+15)>>4)<<4) * (((MAX_HEIGHT+15)>>4)<<4);
    jpeg_attr.bByFrame = HI_TRUE; /*get stream mode is field mode  or frame mode*/
    jpeg_attr.bSupportDCF = HI_FALSE;

    VENC_CHN_ATTR_S venc_chn_attr;
    memset(&venc_chn_attr, 0, sizeof(VENC_CHN_ATTR_S));
    venc_chn_attr.stVeAttr.enType = PT_JPEG;
    memcpy(&venc_chn_attr.stVeAttr.stAttrJpeg, &jpeg_attr, sizeof(VENC_ATTR_JPEG_S));

    s32Ret = HI_MPI_VENC_SetChnAttr(jpeg_venc_chn, &venc_chn_attr);
    if (HI_SUCCESS != s32Ret) { printf(tag "HI_MPI_VENC_SetChnAttr(%d, ...) failed with %#x!\n%s\n", jpeg_venc_chn, s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }

    VENC_PARAM_JPEG_S venc_jpeg_param;
    memset(&venc_jpeg_param, 0, sizeof(VENC_PARAM_JPEG_S));
    s32Ret = HI_MPI_VENC_GetJpegParam(jpeg_venc_chn, &venc_jpeg_param);
    if (HI_SUCCESS != s32Ret) { printf(tag "HI_MPI_VENC_SetJpegParam(%d, ...) failed with %#x!\n%s\n", jpeg_venc_chn, s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }
    venc_jpeg_param.u32Qfactor = qfactor;
    s32Ret = HI_MPI_VENC_SetJpegParam(jpeg_venc_chn, &venc_jpeg_param);
    if (HI_SUCCESS != s32Ret) { printf(tag "HI_MPI_VENC_SetJpegParam(%d, ...) failed with %#x!\n%s\n", jpeg_venc_chn, s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }

//    VENC_COLOR2GREY_S pstChnColor2Grey;
//    pstChnColor2Grey.bColor2Grey = night_mode_enable();
//    s32Ret = HI_MPI_VENC_SetColor2Grey(jpeg_venc_chn, &pstChnColor2Grey);
//    if (HI_SUCCESS != s32Ret) {
//        printf(tag "HI_MPI_VENC_CreateChn(%d) failed with %#x!\n%s\n", jpeg_venc_chn, s32Ret, hi_errstr(s32Ret));
//    }

    VENC_RECV_PIC_PARAM_S pstRecvParam;
    pstRecvParam.s32RecvPicNum = 1;
    s32Ret = HI_MPI_VENC_StartRecvPicEx(jpeg_venc_chn, &pstRecvParam);
    if (HI_SUCCESS != s32Ret) { printf(tag "HI_MPI_VENC_StartRecvPicEx(%d, ...) failed with %#x!\n%s\n", jpeg_venc_chn, s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }

    HI_S32 fd = HI_MPI_VENC_GetFd(jpeg_venc_chn);
    HI_S32 stream_err = get_stream(fd, jpeg_venc_chn, jpeg_buf);
    if (HI_MPI_VENC_CloseFd(jpeg_venc_chn) != HI_SUCCESS) { printf(tag "HI_MPI_VENC_CloseFd(%d) fail\n", jpeg_venc_chn); };

    s32Ret = HI_MPI_VENC_StopRecvPic(jpeg_venc_chn);
    // if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_StopRecvPic(%d) failed with %#x!\n%s\n", venc_chn, s32Ret, hi_errstr(s32Ret)); /* return EXIT_FAILURE; */ }

    return stream_err;
}

int32_t get_jpeg(uint32_t width, uint32_t height, uint32_t qfactor, struct JpegData *jpeg_buf) {
    pthread_mutex_lock(&jpeg_mutex);
    if (!jpeg_enable) { pthread_mutex_unlock(&jpeg_mutex); return HI_FAILURE; }
    // printf(tag "get_next_free_channel %d\n", venc_chn);
    HI_S32 s32Ret = request_pic(width, height, qfactor, jpeg_buf);
    if (s32Ret != HI_SUCCESS) { printf(tag "Can't request_pic!\n"); }
    // printf(tag "disable_channel %d\n", venc_chn);
    pthread_mutex_unlock(&jpeg_mutex);
    return s32Ret;
}
