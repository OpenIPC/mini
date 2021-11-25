#include "motion_detect.h"

#include <ivs_md.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <hi_comm_vb.h>
#include <mpi_ive.h>
#include <mpi_sys.h>
#include <mpi_vb.h>
#include <mpi_vi.h>
#include <mpi_vpss.h>

#include "hierrors.h"
#include "server.h"

typedef struct hiSAMPLE_IVE_RECT_S {
    POINT_S astPoint[4];
} SAMPLE_IVE_RECT_S;

typedef struct hiSAMPLE_RECT_ARRAY_S {
    HI_U16 u16Num;
    SAMPLE_IVE_RECT_S astRect[50];
} SAMPLE_RECT_ARRAY_S;

#define SAMPLE_IVE_MD_IMAGE_NUM 2

typedef struct hiSAMPLE_IVE_MD_S {
    IVE_SRC_IMAGE_S astImg[SAMPLE_IVE_MD_IMAGE_NUM];
    IVE_DST_MEM_INFO_S stBlob;
    MD_ATTR_S stMdAttr;
    SAMPLE_RECT_ARRAY_S stRegion;
    VB_POOL hVbPool;
    HI_U16 u16BaseWitdh;
    HI_U16 u16BaseHeight;
} SAMPLE_IVE_MD_S;

HI_VOID IVE_Md_Uninit(SAMPLE_IVE_MD_S *pstMd);
HI_S32 IVE_Md_Init(
    SAMPLE_IVE_MD_S *pstMd, HI_U16 u16ExtWidth, HI_U16 u16ExtHeight,
    HI_U16 u16BaseWidth, HI_U16 u16BaseHeight);
HI_S32 DmaImage(
    VIDEO_FRAME_INFO_S *pstFrameInfo, IVE_DST_IMAGE_S *pstDst, bool instant);
HI_S32 IVE_CreateImage(
    IVE_IMAGE_S *pstImg, IVE_IMAGE_TYPE_E enType, HI_U16 u16Width,
    HI_U16 u16Height);
HI_S32 IVE_CreateMemInfo(IVE_MEM_INFO_S *pstMemInfo, HI_U32 u32Size);

int motion_detect_init() {
    HI_S32 s32Ret = HI_IVS_MD_Init();
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_IVS_MD_Init failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret));
    }
    return s32Ret;
}

int motion_detect_run(HI_VOID *pArgs) {
    MD_CHN md_chn = 0;
    MD_ATTR_S md_attr;
    md_attr.enAlgMode = MD_ALG_MODE_REF;
    memset(&md_attr, 0, sizeof(VPSS_GRP_ATTR_S));
    HI_S32 s32Ret = HI_IVS_MD_CreateChn(md_chn, &md_attr);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_IVS_MD_CreateChn failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return EXIT_FAILURE;
    }

    VIDEO_FRAME_INFO_S stBaseFrmInfo;
    VIDEO_FRAME_INFO_S stExtFrmInfo;
    HI_S32 s32GetFrameMilliSec = 2000;
    VI_CHN viBaseChn = 0;
    VI_CHN viExtChn = 1;

    bool first_frame = true;
    bool instant = true;

    HI_S32 s32CurIdx = 0;

    HI_U16 u16ExtWidth = 352;
    HI_U16 u16ExtHeight = 288;
    HI_U16 u16BaseWidth = 1920;
    HI_U16 u16BaseHeight = 1080;

    SAMPLE_IVE_MD_S stMd;

    s32Ret = IVE_Md_Init(
        &stMd, u16ExtWidth, u16ExtHeight, u16BaseWidth, u16BaseHeight);

    MD_ATTR_S *pstMdAttr = &(stMd.stMdAttr);

    while (keepRunning) {
        s32Ret =
            HI_MPI_VI_GetFrame(viExtChn, &stExtFrmInfo, s32GetFrameMilliSec);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_VI_GetFrame failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
        }
        s32Ret =
            HI_MPI_VI_GetFrame(viBaseChn, &stBaseFrmInfo, s32GetFrameMilliSec);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_VI_GetFrame failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
        }

        if (!first_frame) {
            s32Ret = DmaImage(&stExtFrmInfo, &stMd.astImg[s32CurIdx], instant);
            if (HI_SUCCESS != s32Ret) {
                printf(
                    "DmaImage failed with %#x!\n%s\n", s32Ret,
                    hi_errstr(s32Ret));
                goto BASE_RELEASE;
            }
        } else {
            s32Ret =
                DmaImage(&stExtFrmInfo, &stMd.astImg[1 - s32CurIdx], instant);
            if (HI_SUCCESS != s32Ret) {
                printf(
                    "DmaImage failed with %#x!\n%s\n", s32Ret,
                    hi_errstr(s32Ret));
                goto BASE_RELEASE;
            }
            first_frame = false;
            goto CHANGE_IDX;
        }

        s32Ret = HI_IVS_MD_Process(
            md_chn, &stMd.astImg[s32CurIdx], &stMd.astImg[1 - s32CurIdx], NULL,
            &stMd.stBlob);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_IVS_MD_Process failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            goto BASE_RELEASE;
        }

    CHANGE_IDX:
        s32CurIdx = 1 - s32CurIdx;

    BASE_RELEASE:
        s32Ret = HI_MPI_VI_ReleaseFrame(viBaseChn, &stBaseFrmInfo);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_VI_ReleaseFrame failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
        }

    EXT_RELEASE:
        s32Ret = HI_MPI_VI_ReleaseFrame(viExtChn, &stExtFrmInfo);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_VI_ReleaseFrame failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
        }
    }
}

int motion_detect_deinit() {
    HI_S32 s32Ret = HI_IVS_MD_Exit();
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_IVS_MD_Exit failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret));
    }
    return s32Ret;
}

HI_S32 DmaImage(
    VIDEO_FRAME_INFO_S *pstFrameInfo, IVE_DST_IMAGE_S *pstDst, bool instant) {
    HI_S32 s32Ret;
    IVE_HANDLE hIveHandle;
    IVE_SRC_DATA_S stSrcData;
    IVE_DST_DATA_S stDstData;
    IVE_DMA_CTRL_S stCtrl = {IVE_DMA_MODE_DIRECT_COPY, 0};
    HI_BOOL bFinish = HI_FALSE;
    HI_BOOL bBlock = HI_TRUE;

    // fill src
    stSrcData.pu8VirAddr = (HI_U8 *)pstFrameInfo->stVFrame.pVirAddr[0];
    stSrcData.u32PhyAddr = pstFrameInfo->stVFrame.u32PhyAddr[0];
    stSrcData.u16Width = (HI_U16)pstFrameInfo->stVFrame.u32Width;
    stSrcData.u16Height = (HI_U16)pstFrameInfo->stVFrame.u32Height;
    stSrcData.u16Stride = (HI_U16)pstFrameInfo->stVFrame.u32Stride[0];

    // fill dst
    stDstData.pu8VirAddr = pstDst->pu8VirAddr[0];
    stDstData.u32PhyAddr = pstDst->u32PhyAddr[0];
    stDstData.u16Width = pstDst->u16Width;
    stDstData.u16Height = pstDst->u16Height;
    stDstData.u16Stride = pstDst->u16Stride[0];

    s32Ret =
        HI_MPI_IVE_DMA(&hIveHandle, &stSrcData, &stDstData, &stCtrl, instant);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_IVE_DMA failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret));
        return s32Ret;
    }

    if (instant) {
        s32Ret = HI_MPI_IVE_Query(hIveHandle, &bFinish, bBlock);
        while (HI_ERR_IVE_QUERY_TIMEOUT == s32Ret) {
            usleep(100);
            s32Ret = HI_MPI_IVE_Query(hIveHandle, &bFinish, bBlock);
        }
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_IVE_Query failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return s32Ret;
        }
    }
    return HI_SUCCESS;
}

HI_S32 IVE_Md_Init(
    SAMPLE_IVE_MD_S *pstMd, HI_U16 u16ExtWidth, HI_U16 u16ExtHeight,
    HI_U16 u16BaseWidth, HI_U16 u16BaseHeight) {
    memset(pstMd, 0, sizeof(SAMPLE_IVE_MD_S));
    for (uint32_t i = 0; i < SAMPLE_IVE_MD_IMAGE_NUM; i++) {
        HI_S32 s32Ret = IVE_CreateImage(
            &pstMd->astImg[i], IVE_IMAGE_TYPE_U8C1, u16ExtWidth, u16ExtHeight);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "SAMPLE_COMM_IVE_CreateImage failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            goto MD_INIT_FAIL;
        }
    }
    HI_U32 u32Size = sizeof(IVE_CCBLOB_S);
    HI_S32 s32Ret = IVE_CreateMemInfo(&pstMd->stBlob, u32Size);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "SAMPLE_COMM_IVE_CreateMemInfo failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        goto MD_INIT_FAIL;
    }

    pstMd->u16BaseWitdh = u16BaseWidth;
    pstMd->u16BaseHeight = u16BaseHeight;
    // Set attr info
    pstMd->stMdAttr.enAlgMode = MD_ALG_MODE_BG;
    pstMd->stMdAttr.enSadMode = IVE_SAD_MODE_MB_4X4;
    pstMd->stMdAttr.enSadOutCtrl = IVE_SAD_OUT_CTRL_THRESH;
    pstMd->stMdAttr.u16SadThr = 100 * (1 << 1); // 100 * (1 << 2);
    pstMd->stMdAttr.u16Width = u16ExtWidth;
    pstMd->stMdAttr.u16Height = u16ExtHeight;
    pstMd->stMdAttr.stAddCtrl.u0q16X = 32768;
    pstMd->stMdAttr.stAddCtrl.u0q16Y = 32768;
    HI_U8 u8WndSz = (1 << (2 + pstMd->stMdAttr.enSadMode));
    pstMd->stMdAttr.stCclCtrl.u16InitAreaThr = u8WndSz * u8WndSz;
    pstMd->stMdAttr.stCclCtrl.u16Step = u8WndSz;

    s32Ret = HI_IVS_MD_Init();
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_IVS_MD_Init failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret));
        goto MD_INIT_FAIL;
    }

MD_INIT_FAIL:
    if (HI_SUCCESS != s32Ret) {
        IVE_Md_Uninit(pstMd);
    }
    return s32Ret;
}

HI_U16 IVE_CalcStride(HI_U16 u16Width, HI_U8 u8Align) {
    return (u16Width + (u8Align - u16Width % u8Align) % u8Align);
}

#define IVE_ALIGN 16
HI_S32 IVE_CreateImage(
    IVE_IMAGE_S *pstImg, IVE_IMAGE_TYPE_E enType, HI_U16 u16Width,
    HI_U16 u16Height) {
    HI_U32 u32Size = 0;
    HI_S32 s32Ret;
    if (NULL == pstImg) {
        printf("pstImg is null\n");
        return HI_FAILURE;
    }

    pstImg->enType = enType;
    pstImg->u16Width = u16Width;
    pstImg->u16Height = u16Height;
    pstImg->u16Stride[0] = IVE_CalcStride(pstImg->u16Width, IVE_ALIGN);

    switch (enType) {
    case IVE_IMAGE_TYPE_U8C1:
    case IVE_IMAGE_TYPE_S8C1: {
        u32Size = pstImg->u16Stride[0] * pstImg->u16Height;
        s32Ret = HI_MPI_SYS_MmzAlloc(
            &pstImg->u32PhyAddr[0], (void **)&pstImg->pu8VirAddr[0], NULL,
            HI_NULL, u32Size);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_SYS_MmzAlloc failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return s32Ret;
        }
    } break;
    case IVE_IMAGE_TYPE_YUV420SP: {
        u32Size = pstImg->u16Stride[0] * pstImg->u16Height * 3 / 2;
        s32Ret = HI_MPI_SYS_MmzAlloc(
            &pstImg->u32PhyAddr[0], (void **)&pstImg->pu8VirAddr[0], NULL,
            HI_NULL, u32Size);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_SYS_MmzAlloc failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return s32Ret;
        }
        pstImg->u16Stride[1] = pstImg->u16Stride[0];
        pstImg->u32PhyAddr[1] =
            pstImg->u32PhyAddr[0] + pstImg->u16Stride[0] * pstImg->u16Height;
        pstImg->pu8VirAddr[1] =
            pstImg->pu8VirAddr[0] + pstImg->u16Stride[0] * pstImg->u16Height;

    } break;
    case IVE_IMAGE_TYPE_YUV422SP: {
        u32Size = pstImg->u16Stride[0] * pstImg->u16Height * 2;
        s32Ret = HI_MPI_SYS_MmzAlloc(
            &pstImg->u32PhyAddr[0], (void **)&pstImg->pu8VirAddr[0], NULL,
            HI_NULL, u32Size);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_SYS_MmzAlloc failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return s32Ret;
        }
        pstImg->u16Stride[1] = pstImg->u16Stride[0];
        pstImg->u32PhyAddr[1] =
            pstImg->u32PhyAddr[0] + pstImg->u16Stride[0] * pstImg->u16Height;
        pstImg->pu8VirAddr[1] =
            pstImg->pu8VirAddr[0] + pstImg->u16Stride[0] * pstImg->u16Height;

    } break;
    case IVE_IMAGE_TYPE_YUV420P:
        break;
    case IVE_IMAGE_TYPE_YUV422P:
        break;
    case IVE_IMAGE_TYPE_S8C2_PACKAGE:
        break;
    case IVE_IMAGE_TYPE_S8C2_PLANAR:
        break;
    case IVE_IMAGE_TYPE_S16C1:
    case IVE_IMAGE_TYPE_U16C1: {
        u32Size = pstImg->u16Stride[0] * pstImg->u16Height * sizeof(HI_U16);
        s32Ret = HI_MPI_SYS_MmzAlloc(
            &pstImg->u32PhyAddr[0], (void **)&pstImg->pu8VirAddr[0], NULL,
            HI_NULL, u32Size);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_SYS_MmzAlloc failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return s32Ret;
        }
    } break;
    case IVE_IMAGE_TYPE_U8C3_PACKAGE: {
        u32Size = pstImg->u16Stride[0] * pstImg->u16Height * 3;
        s32Ret = HI_MPI_SYS_MmzAlloc(
            &pstImg->u32PhyAddr[0], (void **)&pstImg->pu8VirAddr[0], NULL,
            HI_NULL, u32Size);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_SYS_MmzAlloc failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return s32Ret;
        }
        pstImg->pu8VirAddr[1] = pstImg->pu8VirAddr[0] + 1;
        pstImg->pu8VirAddr[2] = pstImg->pu8VirAddr[1] + 1;
        pstImg->u32PhyAddr[1] = pstImg->u32PhyAddr[0] + 1;
        pstImg->u32PhyAddr[2] = pstImg->u32PhyAddr[1] + 1;
        pstImg->u16Stride[1] = pstImg->u16Stride[0];
        pstImg->u16Stride[2] = pstImg->u16Stride[0];
    } break;
    case IVE_IMAGE_TYPE_U8C3_PLANAR:
        break;
    case IVE_IMAGE_TYPE_S32C1:
    case IVE_IMAGE_TYPE_U32C1: {
        u32Size = pstImg->u16Stride[0] * pstImg->u16Height * sizeof(HI_U32);
        s32Ret = HI_MPI_SYS_MmzAlloc(
            &pstImg->u32PhyAddr[0], (void **)&pstImg->pu8VirAddr[0], NULL,
            HI_NULL, u32Size);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_SYS_MmzAlloc failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return s32Ret;
        }
    } break;
    case IVE_IMAGE_TYPE_S64C1:
    case IVE_IMAGE_TYPE_U64C1: {
        u32Size = pstImg->u16Stride[0] * pstImg->u16Height * sizeof(HI_U64);
        s32Ret = HI_MPI_SYS_MmzAlloc(
            &pstImg->u32PhyAddr[0], (void **)&pstImg->pu8VirAddr[0], NULL,
            HI_NULL, u32Size);
        if (HI_SUCCESS != s32Ret) {
            printf(
                "HI_MPI_SYS_MmzAlloc failed with %#x!\n%s\n", s32Ret,
                hi_errstr(s32Ret));
            return s32Ret;
        }
    } break;
    default:
        break;
    }
    return HI_SUCCESS;
}

HI_S32 IVE_CreateMemInfo(IVE_MEM_INFO_S *pstMemInfo, HI_U32 u32Size) {
    if (NULL == pstMemInfo) {
        printf("pstMemInfo is null\n");
        return HI_FAILURE;
    }
    pstMemInfo->u32Size = u32Size;
    HI_S32 s32Ret = HI_MPI_SYS_MmzAlloc(
        &pstMemInfo->u32PhyAddr, (void **)&pstMemInfo->pu8VirAddr, NULL,
        HI_NULL, u32Size);
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_MPI_SYS_MmzAlloc failed with %#x!\n%s\n", s32Ret,
            hi_errstr(s32Ret));
        return s32Ret;
    }
    return HI_SUCCESS;
}

// free mmz
#define IVE_MMZ_FREE(phy, vir)                                                 \
    do {                                                                       \
        if ((0 != (phy)) && (NULL != (vir))) {                                 \
            HI_MPI_SYS_MmzFree((phy), (vir));                                  \
            (phy) = 0;                                                         \
            (vir) = NULL;                                                      \
        }                                                                      \
    } while (0)

HI_VOID IVE_Md_Uninit(SAMPLE_IVE_MD_S *pstMd) {
    for (HI_S32 i = 0; i < SAMPLE_IVE_MD_IMAGE_NUM; i++) {
        IVE_MMZ_FREE(
            pstMd->astImg[i].u32PhyAddr[0], pstMd->astImg[i].pu8VirAddr[0]);
    }
    IVE_MMZ_FREE(pstMd->stBlob.u32PhyAddr, pstMd->stBlob.pu8VirAddr);
    HI_S32 s32Ret = HI_IVS_MD_Exit();
    if (HI_SUCCESS != s32Ret) {
        printf(
            "HI_IVS_MD_Exit failed with %#x!\n%s\n", s32Ret, hi_errstr(s32Ret));
    }
}
