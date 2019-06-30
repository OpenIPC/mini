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

#include "config/app_config.h"

#include "hierrors.h"

#define tag "[hi_jpeg]: "

HI_S32 request_pic(int vpss_chn, uint32_t width, uint32_t height, struct JpegData *jpeg_buf) {

    HI_S32 s32Ret = create_vpss_chn(state.vpss_grp, vpss_chn, -1, -1);
    if (HI_SUCCESS != s32Ret) { printf("create_vpss_chn %d failed with %#x!\n%s\n", vpss_chn, s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }

    VENC_CHN venc_chn = vpss_chn;
    app_config.http_post_chn = venc_chn;
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
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_CreateChn(%d) failed with %#x!\n%s\n", venc_chn, s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }
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

    VENC_RECV_PIC_PARAM_S pstRecvParam;
    pstRecvParam.s32RecvPicNum = 1;
    s32Ret = HI_MPI_VENC_StartRecvPicEx(venc_chn, &pstRecvParam);
    if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_StartRecvPicEx(%d) failed with %#x!\n%s\n", venc_chn, s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }

    HI_S32 fd = HI_MPI_VENC_GetFd(venc_chn);

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    struct timeval TimeoutVal;
    TimeoutVal.tv_sec  = 2;
    TimeoutVal.tv_usec = 0;
    int sel_res = select(fd + 1, &read_fds, NULL, NULL, &TimeoutVal);
    if (sel_res < 0) { printf("select failed!\n"); return HI_FAILURE; }
    else if (sel_res == 0) { printf("get jpeg stream (chn %d) time out\n", venc_chn); return HI_FAILURE; }

    if (FD_ISSET(fd, &read_fds)) {
        VENC_CHN_STAT_S stStat;
        s32Ret = HI_MPI_VENC_Query(venc_chn, &stStat);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_Query(%d, ...) failed with %#x!\n%s\n", venc_chn, s32Ret, hi_errstr(s32Ret)); return HI_FAILURE; }

        if(0 == stStat.u32CurPacks) { printf("NOTE: Current frame is NULL!\n"); return HI_FAILURE; }

        VENC_STREAM_S stStream;
        memset(&stStream, 0, sizeof(stStream));
        stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
        if (NULL == stStream.pstPack) { printf("malloc stream chn[%d] pack failed!\n", venc_chn); return HI_FAILURE; }
        stStream.u32PackCount = stStat.u32CurPacks;
        s32Ret = HI_MPI_VENC_GetStream(venc_chn, &stStream, HI_TRUE);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_GetStream(%d, ...) failed with %#x!\n%s\n", venc_chn, s32Ret, hi_errstr(s32Ret)); free(stStream.pstPack); stStream.pstPack = NULL; return HI_FAILURE; }
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
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VENC_ReleaseStream(%d, ...) failed with %#x!\n%s\n", venc_chn, s32Ret, hi_errstr(s32Ret)); free(stStream.pstPack); stStream.pstPack = NULL; return HI_FAILURE; }
        free(stStream.pstPack);
        stStream.pstPack = NULL;
    }

    return HI_SUCCESS;
}

int get_jpeg(uint32_t width, uint32_t height, struct JpegData *jpeg_buf) {
    VENC_CHN venc_chn = take_next_free_channel(false);
    // printf("get_next_free_channel %d\n", venc_chn);
    HI_S32 s32Ret = request_pic(venc_chn, width, height, jpeg_buf);
    // printf("disable_channel %d\n", venc_chn);
    disable_channel(state.vpss_grp, venc_chn);
    return s32Ret;
}
