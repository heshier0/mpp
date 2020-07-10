#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <hi_math.h>
#include <hi_common.h>
#include <hi_comm_video.h>
#include <hi_comm_sys.h>
#include <mpi_sys.h>
#include <hi_buffer.h>
#include <hi_comm_vb.h>
#include <mpi_vb.h>
#include <hi_comm_vi.h>
#include <mpi_vi.h>
#include <mpi_vgs.h>

#include <QRcode_Detection.h>

#include "utils.h"

#define MAX_FRM_WIDTH   8192


static HI_U32 u32ViDepthFlag = 0;
static HI_U32 u32OrigDepth = 0;
static VIDEO_FRAME_INFO_S stFrame;
static HI_CHAR* pUserPageAddr[2] = {HI_NULL,HI_NULL};
static HI_U32 u32Size = 0;

/*When saving a file,sp420 will be denoted by p420 and sp422 will be denoted by p422 in the name of the file */
static void sample_yuv_8bit_dump(VIDEO_FRAME_S* pVBuf, void **out_buffer)
{
    unsigned int w, h;
    char* pVBufVirt_Y;
    char* pVBufVirt_C;
    char* pMemContent;
    unsigned char TmpBuff[MAX_FRM_WIDTH];

    HI_U64 phy_addr;
    PIXEL_FORMAT_E  enPixelFormat = pVBuf->enPixelFormat;
    VIDEO_FORMAT_E  enVideoFormat = stFrame.stVFrame.enVideoFormat;
    HI_U32 u32UvHeight = 0;

    if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == enPixelFormat)
    {
        if (VIDEO_FORMAT_TILE_16x8 != enVideoFormat)
        {
            u32Size = (pVBuf->u32Stride[0]) * (pVBuf->u32Height) * 3 / 2;
            u32UvHeight = pVBuf->u32Height / 2;
        }
        else
        {
            u32Size = (pVBuf->u32Stride[0]) * (pVBuf->u32Height);
            u32UvHeight = 0;
        }
    }
    phy_addr = pVBuf->u64PhyAddr[0];

    //printf("phy_addr:%x, size:%d\n", phy_addr, size);
    pUserPageAddr[0] = (HI_CHAR*) HI_MPI_SYS_Mmap(phy_addr, u32Size);
    if (HI_NULL == pUserPageAddr[0])
    {
        return;
    }
    //
    *out_buffer = utils_calloc(u32Size);
    if(NULL == out_buffer)
    {
        return;
    }
    //printf("stride: %d,%d\n",pVBuf->u32Stride[0],pVBuf->u32Stride[1] );

    pVBufVirt_Y = pUserPageAddr[0];
    pVBufVirt_C = pVBufVirt_Y + (pVBuf->u32Stride[0]) * (pVBuf->u32Height);

    utils_print("saving......Y......\n");
    for (h = 0; h < pVBuf->u32Height; h++)
    {
        pMemContent = pVBufVirt_Y + h * pVBuf->u32Stride[0];
        memcpy(*out_buffer + h * pVBuf->u32Width, pMemContent, pVBuf->u32Width);
    }
    
    if(PIXEL_FORMAT_YUV_400 != enPixelFormat && VIDEO_FORMAT_TILE_16x8 != enVideoFormat)
    {
        /* save U ----------------------------------------------------------------*/
        utils_print("saving......U......\n");
        for (h = 0; h < u32UvHeight; h++)
        {
            pMemContent = pVBufVirt_C + h * pVBuf->u32Stride[1];

            pMemContent += 1;

            for (w = 0; w < pVBuf->u32Width / 2; w++)
            {
                TmpBuff[w] = *pMemContent;
                pMemContent += 2;
            }
            memcpy(*out_buffer + pVBuf->u32Width * pVBuf->u32Height + pVBuf->u32Width / 2 * h, 
                        TmpBuff, 
                        pVBuf->u32Width / 2);
        }

        /* save V ----------------------------------------------------------------*/
        fprintf(stderr, "V......");
        fflush(stderr);
        for (h = 0; h < u32UvHeight; h++)
        {
            pMemContent = pVBufVirt_C + h * pVBuf->u32Stride[1];

            for (w = 0; w < pVBuf->u32Width / 2; w++)
            {
                TmpBuff[w] = *pMemContent;
                pMemContent += 2;
            }
            memcpy(*out_buffer + pVBuf->u32Width*pVBuf->u32Height + pVBuf->u32Width/2 * u32UvHeight + pVBuf->u32Width/2 * h, 
                        TmpBuff, 
                        pVBuf->u32Width / 2);
        }
    }

    utils_print("done %d!\n", pVBuf->u32TimeRef);

    HI_MPI_SYS_Munmap(pUserPageAddr[0], u32Size);
    pUserPageAddr[0] = HI_NULL;
}

static HI_S32 VI_Restore(VI_PIPE Pipe, VI_CHN Chn)
{
    HI_S32 s32Ret= HI_FAILURE;
    VI_CHN_ATTR_S stChnAttr;

    if(VB_INVALID_POOLID != stFrame.u32PoolId)
    {
        s32Ret = HI_MPI_VI_ReleaseChnFrame(Pipe, Chn, &stFrame);
        if(HI_SUCCESS != s32Ret)
        {
             printf("Release Chn Frame error!!!\n");
        }
        stFrame.u32PoolId = VB_INVALID_POOLID;
    }

    if(HI_NULL != pUserPageAddr[0])
    {
        HI_MPI_SYS_Munmap(pUserPageAddr[0], u32Size);
        pUserPageAddr[0] = HI_NULL;
    }

    if(u32ViDepthFlag)
    {
        s32Ret = HI_MPI_VI_GetChnAttr(Pipe, Chn, &stChnAttr);
        if (HI_SUCCESS != s32Ret)
        {
            printf("get chn attr error!!!\n");
            return HI_FAILURE;
        }

        stChnAttr.u32Depth = u32OrigDepth;
        s32Ret = HI_MPI_VI_SetChnAttr(Pipe, Chn, &stChnAttr);
        if (HI_SUCCESS != s32Ret)
        {
            printf("set chn attr error!!!\n");
            return HI_FAILURE;
        }
        
        u32ViDepthFlag = 0;
    }

    return HI_SUCCESS;
}

//取固定通道 0
BOOL get_qrcode_yuv_buffer()
{
    HI_CHAR szPixFrm[10];
    HI_CHAR szDynamicRange[10];
    HI_CHAR szVideoFrm[10];
    HI_U32 u32Cnt = 1;
    HI_U32 u32Depth = 2;
    HI_S32 s32MilliSec = -1;
    HI_S32 s32Times = 10;
    HI_BOOL bSendToVgs = HI_FALSE;
    VIDEO_FRAME_INFO_S stFrmInfo;
    VGS_TASK_ATTR_S stTask;
    HI_U32 u32Width = 0;
    HI_U32 u32Height = 0;
    HI_S32 i = 0;
    HI_S32 s32Ret;
    VI_CHN_ATTR_S stChnAttr;
    VB_POOL_CONFIG_S stVbPoolCfg;

    HI_U32                 u32Align;
    PIXEL_FORMAT_E         enPixelFormat;
    DATA_BITWIDTH_E        enBitWidth;
    COMPRESS_MODE_E        enCmpMode;
    VB_CAL_CONFIG_S        stCalConfig;

    VI_PIPE Pipe = 2;
    VI_CHN Chn = 0;

    char* qrcode_buffer = NULL;

    pUserPageAddr[0] = HI_NULL;
    stFrame.u32PoolId = VB_INVALID_POOLID;
    u32OrigDepth = 0;
    u32Size = 0;

    //固定通道0
    //设置通道属性
    s32Ret = HI_MPI_VI_GetChnAttr(Pipe, Chn, &stChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("get chn attr error!!!\n");
        return FALSE;
    }

    u32OrigDepth = stChnAttr.u32Depth;
    stChnAttr.u32Depth = u32Depth; //2
    s32Ret = HI_MPI_VI_SetChnAttr(Pipe, Chn, &stChnAttr);

    if (HI_SUCCESS != s32Ret)
    {
        printf("set chn attr error!!!\n");
        return FALSE;
    }
    
    u32ViDepthFlag = 1;
    memset(&stFrame, 0, sizeof(stFrame));
    stFrame.u32PoolId = VB_INVALID_POOLID; //视频缓存池ID
    //不影响输出到VO
    while (u32Cnt--)
    {
        if (HI_MPI_VI_GetChnFrame(Pipe, Chn, &stFrame, s32MilliSec) != HI_SUCCESS)
        {
            printf("Get frame fail \n");
            usleep(1000);
            continue;
        }

        if (DYNAMIC_RANGE_SDR8 == stFrame.stVFrame.enDynamicRange)
        {
            sample_yuv_8bit_dump(&stFrame.stVFrame, (void*)&qrcode_buffer);
        }
    
        printf("Get ViPipe %d frame %d!!\n", Pipe, u32Cnt);
        /* release frame after using */
        s32Ret = HI_MPI_VI_ReleaseChnFrame(Pipe, Chn, &stFrame);

        if (HI_SUCCESS != s32Ret)
        {
            printf("Release frame error ,now exit !!!\n");
            VI_Restore(Pipe, Chn);
            return FALSE;
        }

        stFrame.u32PoolId = VB_INVALID_POOLID;
    }

    VI_Restore(Pipe, Chn);

    //recognize QRcode
    if(NULL == qrcode_buffer)
    {
        return FALSE;
    }
    QRcode_Detection(qrcode_buffer, &stFrame.stVFrame.u32Stride[0], &stFrame.stVFrame.u32Height, output);

    return TRUE;
}
