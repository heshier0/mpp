#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#include "sample_comm.h"

#define YUV_FIFO        "/tmp/grp0chn1.yuv"
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define MAX_FRM_WIDTH   8192
#define VALUE_BETWEEN(x,min,max) (((x)>=(min)) && ((x) <= (max)))

typedef struct hiDUMP_MEMBUF_S
{
    VB_BLK  hBlock;
    VB_POOL hPool;
    HI_U32  u32PoolId;

    HI_U64  u64PhyAddr;
    HI_U8*  pVirAddr;
    HI_S32  s32Mdev;
} DUMP_MEMBUF_S;

static HI_U32 u32VpssDepthFlag = 0;

static VPSS_GRP VpssGrp = 0;
static VPSS_CHN VpssChn = 0;
static HI_U32 u32OrigDepth = 0;
static VIDEO_FRAME_INFO_S stFrame;

static VB_POOL hPool  = VB_INVALID_POOLID;
static DUMP_MEMBUF_S stMem = {0};
static VGS_HANDLE hHandle = -1;
static HI_U32  u32BlkSize = 0;

static HI_CHAR* pUserPageAddr[2] = {HI_NULL, HI_NULL};
static HI_U32 u32Size = 0;

//static FILE* pfd = HI_NULL;
int pfd = -1;


static HI_S32 VPSS_Restore(VPSS_GRP VpssGrp, VPSS_CHN VpssChn)
{
    HI_S32 s32Ret = HI_FAILURE;
    VPSS_CHN_ATTR_S stChnAttr;
    VPSS_EXT_CHN_ATTR_S stExtChnAttr;


    if (VB_INVALID_POOLID != stFrame.u32PoolId)
    {
        s32Ret = HI_MPI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrame);

        if (HI_SUCCESS != s32Ret)
        {
            printf("Release Chn Frame error!!!\n");
        }

        stFrame.u32PoolId = VB_INVALID_POOLID;
    }

    if (-1 != hHandle)
    {
        HI_MPI_VGS_CancelJob(hHandle);
        hHandle = -1;
    }

    if (HI_NULL != stMem.pVirAddr)
    {
        HI_MPI_SYS_Munmap((HI_VOID*)stMem.pVirAddr, u32BlkSize );
        stMem.u64PhyAddr = HI_NULL;
    }

    if (VB_INVALID_POOLID != stMem.hPool)
    {
        HI_MPI_VB_ReleaseBlock(stMem.hBlock);
        stMem.hPool = VB_INVALID_POOLID;
    }

    if (VB_INVALID_POOLID != hPool)
    {
        HI_MPI_VB_DestroyPool( hPool );
        hPool = VB_INVALID_POOLID;
    }

    if (HI_NULL != pUserPageAddr[0])
    {
        HI_MPI_SYS_Munmap(pUserPageAddr[0], u32Size);
        pUserPageAddr[0] = HI_NULL;
    }

    if (-1 == pfd)
    {
        // fclose(pfd);
        // pfd = HI_NULL;
        unlink(YUV_FIFO);
        pfd = -1;
    }

    if (u32VpssDepthFlag)
    {

        if (VpssChn >= VPSS_MAX_PHY_CHN_NUM)
        {
            s32Ret = HI_MPI_VPSS_GetExtChnAttr(VpssGrp, VpssChn, &stExtChnAttr);
        }
        else
        {
            s32Ret = HI_MPI_VPSS_GetChnAttr(VpssGrp, VpssChn, &stChnAttr);
        }

        if (s32Ret != HI_SUCCESS)
        {
            printf("get chn attr error!!!\n");
        }


        if (VpssChn >= VPSS_MAX_PHY_CHN_NUM)
        {
            stExtChnAttr.u32Depth = u32OrigDepth;
            s32Ret = HI_MPI_VPSS_SetExtChnAttr(VpssGrp, VpssChn, &stExtChnAttr);
        }
        else
        {
            stChnAttr.u32Depth = u32OrigDepth;
            s32Ret = HI_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stChnAttr) ;
        }

        if (s32Ret != HI_SUCCESS)
        {
            printf("set depth error!!!\n");
        }

        u32VpssDepthFlag = 0;
    }


    return HI_SUCCESS;
}

void SAMPLE_YUV_HandleSig(HI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_VENC_StopGetStream();
        VPSS_Restore(VpssGrp, VpssChn);
        SAMPLE_COMM_All_ISP_Stop();
        SAMPLE_COMM_SYS_Exit();

        SAMPLE_PRT("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}

//static void sample_yuv_8bit_dump(VIDEO_FRAME_S* pVBuf, FILE* pfd)
static void sample_yuv_8bit_dump(VIDEO_FRAME_S* pVBuf, int pfd)
{
    unsigned int w, h;
    char* pVBufVirt_Y;
    char* pVBufVirt_C;
    char* pMemContent;
    unsigned char TmpBuff[MAX_FRM_WIDTH]; //If this value is too small and the image is big, this memory may not be enough
    HI_U64 phy_addr;
    PIXEL_FORMAT_E  enPixelFormat = pVBuf->enPixelFormat;
    HI_U32 u32UvHeight = 0;/*When the storage format is a planar format, this variable is used to keep the height of the UV component */
    HI_BOOL bUvInvert;

    bUvInvert = (PIXEL_FORMAT_YUV_SEMIPLANAR_420 == enPixelFormat
        || PIXEL_FORMAT_YUV_SEMIPLANAR_422 == enPixelFormat) ? HI_TRUE : HI_FALSE;

    if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == enPixelFormat || PIXEL_FORMAT_YUV_SEMIPLANAR_420 == enPixelFormat)
    {
        u32Size = (pVBuf->u32Stride[0]) * (pVBuf->u32Height) * 3 / 2;
        u32UvHeight = pVBuf->u32Height / 2;
    }
    else if (PIXEL_FORMAT_YVU_SEMIPLANAR_422 == enPixelFormat || PIXEL_FORMAT_YUV_SEMIPLANAR_422 == enPixelFormat)
    {
        u32Size = (pVBuf->u32Stride[0]) * (pVBuf->u32Height) * 2;
        u32UvHeight = pVBuf->u32Height;
    }
    else if (PIXEL_FORMAT_YUV_400 == enPixelFormat)
    {
        u32Size = (pVBuf->u32Stride[0]) * (pVBuf->u32Height);
        u32UvHeight = pVBuf->u32Height;
    }

    phy_addr = pVBuf->u64PhyAddr[0];
    pUserPageAddr[0] = (HI_CHAR*) HI_MPI_SYS_Mmap(phy_addr, u32Size);
    if (HI_NULL == pUserPageAddr[0])
    {
        return;
    }

    pVBufVirt_Y = pUserPageAddr[0];
    pVBufVirt_C = pVBufVirt_Y + (pVBuf->u32Stride[0]) * (pVBuf->u32Height);

    char* pYUV = calloc(u32Size, 1);
    if(NULL == pYUV)
    {
        printf("calloc memory failed\n");
        return;
    }

    /* save Y ----------------------------------------------------------------*/
    fprintf(stderr, "saving......Y......");
    fflush(stderr);

    for (h = 0; h < pVBuf->u32Height; h++)
    {
        pMemContent = pVBufVirt_Y + h * pVBuf->u32Stride[0];
        //fwrite(pMemContent, pVBuf->u32Width, 1, pfd);
        memcpy(pYUV + h * pVBuf->u32Width, pMemContent, pVBuf->u32Width);
    }

    if (PIXEL_FORMAT_YUV_400 != enPixelFormat)
    {
        // fflush(pfd);
        /* save U ----------------------------------------------------------------*/
        fprintf(stderr, "U......");
        fflush(stderr);

        for (h = 0; h < u32UvHeight; h++)
        {
            pMemContent = pVBufVirt_C + h * pVBuf->u32Stride[1];

            if(!bUvInvert) pMemContent += 1;

            for (w = 0; w < pVBuf->u32Width / 2; w++)
            {
                TmpBuff[w] = *pMemContent;
                pMemContent += 2;
            }

            //fwrite(TmpBuff, pVBuf->u32Width / 2, 1, pfd);
            memcpy(pYUV + pVBuf->u32Width * pVBuf->u32Height + pVBuf->u32Width / 2 * h, 
                        TmpBuff, 
                        pVBuf->u32Width / 2);
        }

        // fflush(pfd);

        /* save V ----------------------------------------------------------------*/
        fprintf(stderr, "V......");
        fflush(stderr);

        for (h = 0; h < u32UvHeight; h++)
        {
            pMemContent = pVBufVirt_C + h * pVBuf->u32Stride[1];

            if(bUvInvert) pMemContent += 1;

            for (w = 0; w < pVBuf->u32Width / 2; w++)
            {
                TmpBuff[w] = *pMemContent;
                pMemContent += 2;
            }

            // fwrite(TmpBuff, pVBuf->u32Width / 2, 1, pfd);
            memcpy(pYUV + pVBuf->u32Width*pVBuf->u32Height + pVBuf->u32Width/2 * u32UvHeight + pVBuf->u32Width/2 * h, 
                        TmpBuff, 
                        pVBuf->u32Width / 2);
        }
    }

    // fflush(pfd);
    printf("Write to fifo....\n");
    int n = write(pfd, pYUV, u32Size);
    if (n != u32Size)
    {
        printf("写入FIFO失败\n");
        return; 
    }
    free(pYUV);
    pYUV = NULL;

    fprintf(stderr, "done %d!\n", pVBuf->u32TimeRef);
    fflush(stderr);

    HI_MPI_SYS_Munmap(pUserPageAddr[0], u32Size);
    pUserPageAddr[0] = HI_NULL;
}

HI_VOID SAMPLE_MISC_VpssDump(VPSS_GRP Grp, VPSS_CHN Chn, HI_U32 u32FrameCnt)
{
    HI_CHAR szYuvName[128];
    HI_CHAR szPixFrm[10];
    HI_U32 u32Cnt = u32FrameCnt;
    HI_U32 u32Depth = 2;
    HI_S32 s32MilliSec = -1;
    HI_S32 s32Times = 10;
    HI_BOOL bSendToVgs = HI_FALSE;
    VIDEO_FRAME_INFO_S stFrmInfo;
    VGS_TASK_ATTR_S stTask;
    HI_U32 u32LumaSize = 0;
    HI_U32 u32PicLStride = 0;
    HI_U32 u32PicCStride = 0;
    HI_U32 u32Width = 0;
    HI_U32 u32Height = 0;
    HI_S32 i = 0;
    HI_S32 s32Ret;
    VPSS_CHN_ATTR_S stChnAttr;
    VPSS_EXT_CHN_ATTR_S stExtChnAttr;
    HI_U32 u32BitWidth;
    VB_POOL_CONFIG_S stVbPoolCfg;

    if (VpssChn >= VPSS_MAX_PHY_CHN_NUM)
    {
        s32Ret = HI_MPI_VPSS_GetExtChnAttr(Grp, Chn, &stExtChnAttr);
        u32OrigDepth = stExtChnAttr.u32Depth;
    }
    else
    {
        s32Ret = HI_MPI_VPSS_GetChnAttr(Grp, Chn, &stChnAttr);
        u32OrigDepth = stChnAttr.u32Depth;
    }

    if (s32Ret != HI_SUCCESS)
    {
        printf("get chn attr error!!!\n");
        return;
    }

    if (VpssChn >= VPSS_MAX_PHY_CHN_NUM)
    {
        stExtChnAttr.u32Depth = u32Depth;
        s32Ret = HI_MPI_VPSS_SetExtChnAttr(Grp, Chn, &stExtChnAttr);
    }
    else
    {
        stChnAttr.u32Depth = u32Depth;
        s32Ret = HI_MPI_VPSS_SetChnAttr(Grp, Chn, &stChnAttr) ;
    }

    if (s32Ret != HI_SUCCESS)
    {
        printf("set depth error!!!\n");
        VPSS_Restore(Grp, Chn);
        return;
    }

    u32VpssDepthFlag = 1;

    memset(&stFrame, 0, sizeof(stFrame));
    stFrame.u32PoolId = VB_INVALID_POOLID;

    while (HI_MPI_VPSS_GetChnFrame(Grp, Chn, &stFrame, s32MilliSec) != HI_SUCCESS)
    {
        s32Times--;

        if (0 >= s32Times)
        {
            printf("get frame error for 10 times,now exit !!!\n");
            VPSS_Restore(Grp, Chn);
            return;
        }

        usleep(40000);
    }

    if (VIDEO_FORMAT_LINEAR != stFrame.stVFrame.enVideoFormat)
    {
        printf("only support linear frame dump!\n");
        HI_MPI_VPSS_ReleaseChnFrame(Grp, Chn, &stFrame);
        stFrame.u32PoolId = VB_INVALID_POOLID;
        return;
    }

    switch (stFrame.stVFrame.enPixelFormat)
    {
        case PIXEL_FORMAT_YVU_SEMIPLANAR_420:
        case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
            snprintf(szPixFrm, 10, "P420");
            break;

        case PIXEL_FORMAT_YVU_SEMIPLANAR_422:
        case PIXEL_FORMAT_YUV_SEMIPLANAR_422:
            snprintf(szPixFrm, 10, "P422");
            break;

        default:
            snprintf(szPixFrm, 10, "P400");
            break;
    }


    /* make file name */
    // snprintf(szYuvName, 128, "./vpss_grp%d_chn%d_%dx%d_%s_%d.yuv", Grp, Chn,
    //          stFrame.stVFrame.u32Width, stFrame.stVFrame.u32Height, szPixFrm, u32Cnt);
    // printf("Dump YUV frame of vpss chn %d  to file: \"%s\"\n", Chn, szYuvName);
    // fflush(stdout);

    s32Ret = HI_MPI_VPSS_ReleaseChnFrame(Grp, Chn, &stFrame);

    if (HI_SUCCESS != s32Ret)
    {
        printf("Release frame error ,now exit !!!\n");
        VPSS_Restore(Grp, Chn);
        return;
    }

    stFrame.u32PoolId = VB_INVALID_POOLID;
    /* open file */
    // pfd = fopen(szYuvName, "wb");
    pfd = open(YUV_FIFO, O_WRONLY, 0);
    // if (HI_NULL == pfd)
    if (-1 == pfd)
    {
        printf("open fifo failed:%s!\n", strerror(errno));
        unlink(YUV_FIFO);
        VPSS_Restore(Grp, Chn);
        return;
    }

    /* get frame  */
    //while (u32Cnt--)
    while(1)
    {
        if (HI_MPI_VPSS_GetChnFrame(Grp, Chn, &stFrame, s32MilliSec) != HI_SUCCESS)
        {
            printf("Get frame fail \n");
            usleep(1000);
            continue;
        }

        sample_yuv_8bit_dump(&stFrame.stVFrame, pfd);
        
        /* release frame after using */
        s32Ret = HI_MPI_VPSS_ReleaseChnFrame(Grp, Chn, &stFrame);
        if (HI_SUCCESS != s32Ret)
        {
            printf("Release frame error ,now exit !!!\n");
            VPSS_Restore(Grp, Chn);
            return;
        }

        stFrame.u32PoolId = VB_INVALID_POOLID;
    }

    VPSS_Restore(Grp, Chn);
    return;
}

HI_S32 SAMPLE_VIO_VPSS_VO_MIPI_TX_DOUBLE_VI(HI_VOID)
{
    HI_S32             s32Ret = HI_SUCCESS;

    HI_S32             s32ViCnt       = 2;
    VI_DEV             ViDev[2]       = {0, 1};
    VI_PIPE            ViPipe[4]      = {0, 1, 2, 3};
    VI_CHN             ViChn          = 0;
    HI_S32             s32WorkSnsId   = 0;
    SAMPLE_VI_CONFIG_S stViConfig;

    SIZE_S             stSize;
    VB_CONFIG_S        stVbConf;
    PIC_SIZE_E         enPicSize;
    HI_U32             u32BlkSize;

    VO_CHN             VoChn          = 0;
    SAMPLE_VO_CONFIG_S stVoConfig;

    WDR_MODE_E         enWDRMode      = WDR_MODE_NONE;
    DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E     enVideoFormat  = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E    enCompressMode = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E     enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;//VI_ONLINE_VPSS_OFFLINE;

    VPSS_GRP           VpssGrp[2]        = {0, 1};
    VPSS_GRP_ATTR_S    stVpssGrpAttr;
    VPSS_CHN           VpssChn        = VPSS_CHN0;
    HI_BOOL            abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {1, 1};
    VPSS_CHN_ATTR_S    astVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];

    /*config vi*/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);

    stViConfig.s32WorkingViNum                                   = s32ViCnt;
    stViConfig.as32WorkingViId[0]                                = 0;
    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.MipiDev         = ViDev[0];
    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.s32BusId        = 0;
    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.ViDev           = ViDev[0];
    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.enWDRMode       = enWDRMode;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.enMastPipeMode = enMastPipeMode;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[0]       = ViPipe[0];
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[1]       = -1;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[2]       = -1;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[3]       = -1;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.ViChn           = ViChn;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enPixFormat     = enPixFormat;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enDynamicRange  = enDynamicRange;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enVideoFormat   = enVideoFormat;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enCompressMode  = enCompressMode;

    stViConfig.as32WorkingViId[1]                                = 1;
    stViConfig.astViInfo[1].stSnsInfo.MipiDev         = ViDev[1];
    stViConfig.astViInfo[1].stSnsInfo.s32BusId        = 1;
    stViConfig.astViInfo[1].stDevInfo.ViDev           = ViDev[1];
    stViConfig.astViInfo[1].stDevInfo.enWDRMode       = enWDRMode;
    stViConfig.astViInfo[1].stPipeInfo.enMastPipeMode = enMastPipeMode;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[0]       = ViPipe[2];
    stViConfig.astViInfo[1].stPipeInfo.aPipe[1]       = -1;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[2]       = -1;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[3]       = -1;
    stViConfig.astViInfo[1].stChnInfo.ViChn           = ViChn;
    stViConfig.astViInfo[1].stChnInfo.enPixFormat     = enPixFormat;
    stViConfig.astViInfo[1].stChnInfo.enDynamicRange  = enDynamicRange;
    stViConfig.astViInfo[1].stChnInfo.enVideoFormat   = enVideoFormat;
    stViConfig.astViInfo[1].stChnInfo.enCompressMode  = enCompressMode;

    /*get picture size*/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get picture size by sensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get picture size failed!\n");
        return s32Ret;
    }

    /*config vb*/
    hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 20;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    SAMPLE_COMM_SYS_Exit();
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        return s32Ret;
    }

    /*start vi*/
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT;
    }
	
    /*config vpss*/
    hi_memset(&stVpssGrpAttr, sizeof(VPSS_GRP_ATTR_S), 0, sizeof(VPSS_GRP_ATTR_S));
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate    = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate    = -1;
    stVpssGrpAttr.enDynamicRange                 = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.enPixelFormat                  = enPixFormat;
    stVpssGrpAttr.u32MaxW                        = stSize.u32Width;
    stVpssGrpAttr.u32MaxH                        = stSize.u32Height;
    stVpssGrpAttr.bNrEn                          = HI_TRUE;
    stVpssGrpAttr.stNrAttr.enCompressMode        = COMPRESS_MODE_FRAME;
    stVpssGrpAttr.stNrAttr.enNrMotionMode        = NR_MOTION_MODE_NORMAL;
    //通道0不能缩小
    astVpssChnAttr[VpssChn].u32Width                    = stSize.u32Width;
    astVpssChnAttr[VpssChn].u32Height                   = stSize.u32Height;
    astVpssChnAttr[VpssChn].enChnMode                   = VPSS_CHN_MODE_USER;
    astVpssChnAttr[VpssChn].enCompressMode              = enCompressMode;
    astVpssChnAttr[VpssChn].enDynamicRange              = enDynamicRange;
    astVpssChnAttr[VpssChn].enVideoFormat               = enVideoFormat;
    astVpssChnAttr[VpssChn].enPixelFormat               = enPixFormat;
    astVpssChnAttr[VpssChn].stFrameRate.s32SrcFrameRate = 30;
    astVpssChnAttr[VpssChn].stFrameRate.s32DstFrameRate = 30;
    astVpssChnAttr[VpssChn].u32Depth                    = 0;
    astVpssChnAttr[VpssChn].bMirror                     = HI_FALSE;
    astVpssChnAttr[VpssChn].bFlip                       = HI_FALSE;
    astVpssChnAttr[VpssChn].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
	//通道1只能缩小
    astVpssChnAttr[1].u32Width                    = 640;//stSize.u32Width;
    astVpssChnAttr[1].u32Height                   = 360;//stSize.u32Height;
    astVpssChnAttr[1].enChnMode                   = VPSS_CHN_MODE_USER;
    astVpssChnAttr[1].enCompressMode              = enCompressMode;
    astVpssChnAttr[1].enDynamicRange              = enDynamicRange;
    astVpssChnAttr[1].enVideoFormat               = enVideoFormat;
    astVpssChnAttr[1].enPixelFormat               = enPixFormat;
    astVpssChnAttr[1].stFrameRate.s32SrcFrameRate = 30;
    astVpssChnAttr[1].stFrameRate.s32DstFrameRate = 30;
    astVpssChnAttr[1].u32Depth                    = 0;
    astVpssChnAttr[1].bMirror                     = HI_FALSE;
    astVpssChnAttr[1].bFlip                       = HI_FALSE;
    astVpssChnAttr[1].stAspectRatio.enMode        = ASPECT_RATIO_NONE;


    /*start vpss*/
    abChnEnable[0] = HI_TRUE;
    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp[0], abChnEnable, &stVpssGrpAttr, astVpssChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT1;
    }
// start vpssgrp 1
    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp[1], abChnEnable, &stVpssGrpAttr, astVpssChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /*vpss bind vo*/
    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe[0], ViChn, VpssGrp[0]);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vpss bind vi failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT2;
    }

    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe[2], ViChn, VpssGrp[1]);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vpss bind vi failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT3;
    }

    /*config vo*/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    stVoConfig.enDstDynamicRange = enDynamicRange;

    stVoConfig.enVoIntfType = VO_INTF_MIPI;
 //   stVoConfig.enIntfSync = VO_OUTPUT_1080x1920_60;
//   stVoConfig.enIntfSync = VO_OUTPUT_800x1280_60;
    stVoConfig.enIntfSync = VO_OUTPUT_USER;//VO_OUTPUT_1080x1920_60;

    stVoConfig.enPicSize = enPicSize;
//	stVoConfig.enVoPartMode      = 1;
	stVoConfig.enVoMode			 =1;

    /*start vo*/
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT4;
    }

#if 0   /* added by rpdzkj ivy to rotate sensor display degrees */  
        /* Rotate 90 degrees to the channel */    
	if(sns0_rotat == 1 || sns0_rotat == 2 || sns0_rotat == 3 )
	{
        s32Ret = HI_MPI_VO_SetChnRotation(stVoConfig.VoDev, 0, sns0_rotat);
        if (s32Ret != HI_SUCCESS)                                             
        {                                                            
            printf("Set channel rotation failed with errno %#x!\n", s32Ret);  
            return HI_FAILURE;                                                
        }          
	}
	
	if(sns1_rotat == 1 || sns1_rotat == 2 || sns1_rotat == 3 )
	{
        s32Ret = HI_MPI_VO_SetChnRotation(stVoConfig.VoDev, 1, sns1_rotat);
        if (s32Ret != HI_SUCCESS)                                             
        {                                                            
            printf("Set channel rotation failed with errno %#x!\n", s32Ret);  
            return HI_FAILURE;                                                
        }          
	}
#endif 

    /*vpss bind vo*/
    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp[0], VpssChn, stVoConfig.VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vo bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT5;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp[1], 1, stVoConfig.VoDev, 1);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vo bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT6;
    }

    SAMPLE_MISC_VpssDump(0, 1, 30);
    
    PAUSE();

    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp[0], VpssChn, stVoConfig.VoDev, VoChn);
    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp[1], 1, stVoConfig.VoDev, 1);

EXIT6:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT5:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe[2], ViChn, VpssGrp[1]);
EXIT4:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe[0], ViChn, VpssGrp[0]);
EXIT3:
    SAMPLE_COMM_VPSS_Stop(VpssGrp[1], abChnEnable);
EXIT2:
    SAMPLE_COMM_VPSS_Stop(VpssGrp[0], abChnEnable);
EXIT1:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    SAMPLE_COMM_SYS_Exit();
    return s32Ret;
}

int main(int argc, char *argv[])
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_U32 u32FrmCnt = 1;
    HI_U32 u32ByteAlign = 1;

    u32VpssDepthFlag = 0;
    pUserPageAddr[0] = HI_NULL;
    stFrame.u32PoolId = VB_INVALID_POOLID;
    u32OrigDepth = 0;
    hPool  = VB_INVALID_POOLID;
    hHandle = -1;
    u32BlkSize = 0;
    u32Size = 0;
    // pfd = HI_NULL;
    pfd = -1;

    signal(SIGINT, SAMPLE_YUV_HandleSig);
    signal(SIGTERM, SAMPLE_YUV_HandleSig);

    if(mkfifo(YUV_FIFO, FILE_MODE) < 0)
    {
        if(EEXIST == errno)
		{
			printf("FIFO：%s已经存在，不能重新创建\r\n", YUV_FIFO);
		}
		else
		{
			perror("创建FIFO错误");
			exit(-1);
		}
    }

    s32Ret = SAMPLE_VIO_VPSS_VO_MIPI_TX_DOUBLE_VI();

    if (HI_SUCCESS == s32Ret)
    {
        SAMPLE_PRT("sample_yuv exit success!\n");
    }
    else
    {
        SAMPLE_PRT("sample_yuv exit abnormally!\n");
    }


    return s32Ret;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
