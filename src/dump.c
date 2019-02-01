#include <stdio.h>
#include <stdlib.h>


#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <hi_common.h>
#include <hi_comm_video.h>
#include <hi_comm_sys.h>
#include <mpi_vb.h>
#include <mpi_vi.h>
#include <mpi_sys.h>
#include <hi_comm_isp.h>
#include <mpi_isp.h>
#include <hi_sns_ctrl.h>
#include <hi_ae_comm.h>
#include <hi_awb_comm.h>
#include <mpi_ae.h>
#include <mpi_awb.h>
#include <isp_dehaze.h>

#include "hierrors.h"

int dump();
int read_dump();

int main(int argc, char *argv[]) {
#ifdef __APPLE__
    read_dump();
#else
    dump();
#endif
}

int read_dump() {
//    HI_MPI_VB_SetSupplementConf
    VB_SUPPLEMENT_CONF_S pstSupplementConf;
    {
        FILE *file = fopen("HI_MPI_VB_GetSupplementConf.bin", "r");
        size_t len = fread(&pstSupplementConf, sizeof(VB_SUPPLEMENT_CONF_S), 1, file);
        fclose(file);
    }

//    HI_MPI_SYS_SetConf
    MPP_SYS_CONF_S pstSysConf;
    {
        FILE *file = fopen("HI_MPI_SYS_GetConf.bin", "r");
        size_t len = fread(&pstSysConf, sizeof(MPP_SYS_CONF_S), 1, file);
        fclose(file);
    }

//    HI_MPI_ISP_SetPubAttr
    ISP_PUB_ATTR_S pstPubAttr_0;
    {
        FILE *file = fopen("HI_MPI_ISP_GetPubAttr_0.bin", "r");
        size_t len = fread(&pstPubAttr_0, sizeof(ISP_PUB_ATTR_S), 1, file);
        fclose(file);
    }

//    HI_MPI_VI_SetDevAttr
    VI_DEV_ATTR_S pstDevAttr_0;
    {
        FILE *file = fopen("HI_MPI_VI_GetDevAttr_0.bin", "r");
        size_t len = fread(&pstDevAttr_0, sizeof(VI_DEV_ATTR_S), 1, file);
        fclose(file);
    }

//    HI_MPI_VI_SetChnAttr
    VI_CHN_ATTR_S pstAttr_0;
    {
        FILE *file = fopen("HI_MPI_VI_GetChnAttr_0.bin", "r");
        size_t len = fread(&pstAttr_0, sizeof(VI_CHN_ATTR_S), 1, file);
        fclose(file);
    }

    return EXIT_SUCCESS;
}

#ifndef __APPLE__
int dump() {
//    HI_MPI_VB_SetSupplementConf
    {
        VB_SUPPLEMENT_CONF_S pstSupplementConf;
        HI_S32 s32Ret = HI_MPI_VB_GetSupplementConf(&pstSupplementConf);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VB_GetSupplementConf failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
        FILE *file = fopen("HI_MPI_VB_GetSupplementConf.bin", "w+");
        size_t len = fwrite(&pstSupplementConf, sizeof(char), sizeof(VB_SUPPLEMENT_CONF_S), file);
        fclose(file);
    }

//    HI_MPI_SYS_SetConf
    {
        MPP_SYS_CONF_S pstSysConf;
        HI_S32 s32Ret = HI_MPI_SYS_GetConf(&pstSysConf);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_SYS_GetConf failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
        FILE *file = fopen("HI_MPI_SYS_GetConf.bin", "w+");
        size_t len = fwrite(&pstSysConf, sizeof(char), sizeof(MPP_SYS_CONF_S), file);
        fclose(file);
    }

//    HI_MPI_ISP_SetPubAttr
    {
        ISP_PUB_ATTR_S pstPubAttr;
        HI_S32 s32Ret = HI_MPI_ISP_GetPubAttr(0, &pstPubAttr);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_ISP_GetPubAttr 0 failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
        FILE *file = fopen("HI_MPI_ISP_GetPubAttr_0.bin", "w+");
        size_t len = fwrite(&pstPubAttr, sizeof(char), sizeof(ISP_PUB_ATTR_S), file);
        fclose(file);
    }

//    HI_MPI_VI_SetDevAttr
    {
        VI_DEV_ATTR_S pstDevAttr;
        HI_S32 s32Ret = HI_MPI_VI_GetDevAttr(0, &pstDevAttr);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_GetDevAttr 0 failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); return EXIT_FAILURE; }
        FILE *file = fopen("HI_MPI_VI_GetDevAttr_0.bin", "w+");
        size_t len = fwrite(&pstDevAttr, sizeof(char), sizeof(VI_DEV_ATTR_S), file);
        fclose(file);
    }

//    HI_MPI_VI_SetChnAttr
    {
        VI_CHN_ATTR_S pstAttr;
        HI_S32 s32Ret = HI_MPI_VI_GetChnAttr(0, &pstAttr);
        if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_GetChnAttr 0 failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); }
        else {
            FILE *file = fopen("HI_MPI_VI_GetChnAttr_0.bin", "w+");
            size_t len = fwrite(&pstAttr, sizeof(char), sizeof(VI_CHN_ATTR_S), file);
            fclose(file);
        }
    }
    // {
    //     VI_CHN_ATTR_S pstAttr;
    //     HI_S32 s32Ret = HI_MPI_VI_GetChnAttr(1, &pstAttr);
    //     if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_GetChnAttr 1 failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); }
    //     else {
    //         FILE *file = fopen("HI_MPI_VI_GetChnAttr_1.bin", "w+");
    //         size_t len = fwrite(&pstAttr, sizeof(char), sizeof(VI_CHN_ATTR_S), file);
    //         fclose(file);
    //     }
    // }
    // {
    //     VI_CHN_ATTR_S pstAttr;
    //     HI_S32 s32Ret = HI_MPI_VI_GetChnAttr(2, &pstAttr);
    //     if (HI_SUCCESS != s32Ret) { printf("HI_MPI_VI_GetChnAttr 2 failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret)); }
    //     else {
    //         FILE *file = fopen("HI_MPI_VI_GetChnAttr_2.bin", "w+");
    //         size_t len = fwrite(&pstAttr, sizeof(char), sizeof(VI_CHN_ATTR_S), file);
    //         fclose(file);
    //     }
    // }


    return EXIT_SUCCESS;
}
#endif
