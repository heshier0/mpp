#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "hi_common.h"
#include "hi_comm_sys.h"
#include "hi_comm_vb.h"
#include "mpi_vb.h"
#include "hi_comm_vpss.h"
#include "mpi_vpss.h"

#include "common.h"
#include "utils.h"

#define MAX_FRM_WIDTH   20480

static HI_U32 u32VpssDepthFlag = 0;
static VPSS_GRP VpssGrp = 0;
static VPSS_CHN VpssChn = 1;
static HI_U32 u32OrigDepth = 0;
static VIDEO_FRAME_INFO_S stFrame;
static VB_POOL hPool  = VB_INVALID_POOLID;
static HI_CHAR* pUserPageAddr[2] = {HI_NULL, HI_NULL};
static HI_U32 u32Size = 0;

/*When saving a file,sp420 will be denoted by p420 and sp422 will be denoted by p422 in the name of the file */
static void sample_yuv_8bit_dump(VIDEO_FRAME_S* pVBuf, void **pOutBuf)
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

    u32Size = (pVBuf->u32Stride[0]) * (pVBuf->u32Height) * 3 / 2;
    u32UvHeight = pVBuf->u32Height / 2;
    phy_addr = pVBuf->u64PhyAddr[0];
    pUserPageAddr[0] = (HI_CHAR*) HI_MPI_SYS_Mmap(phy_addr, u32Size);
    if (HI_NULL == pUserPageAddr[0])
    {
        return;
    }

    *pOutBuf = utils_calloc(u32Size);
    if(NULL == *pOutBuf)
    {
        return;
    }

    pVBufVirt_Y = pUserPageAddr[0];
    pVBufVirt_C = pVBufVirt_Y + (pVBuf->u32Stride[0]) * (pVBuf->u32Height);

    utils_print("saving......Y......\n");
    for (h = 0; h < pVBuf->u32Height; h++)
    {
        pMemContent = pVBufVirt_Y + h * pVBuf->u32Stride[0];
        memcpy(*pOutBuf + h * pVBuf->u32Width, pMemContent, pVBuf->u32Width);
    }

    if (PIXEL_FORMAT_YUV_400 != enPixelFormat)
    {
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
            memcpy(*pOutBuf + pVBuf->u32Width * pVBuf->u32Height + pVBuf->u32Width / 2 * h, 
                        TmpBuff, 
                        pVBuf->u32Width / 2);
        }

        utils_print("saving......V......\n");
        for (h = 0; h < u32UvHeight; h++)
        {
            pMemContent = pVBufVirt_C + h * pVBuf->u32Stride[1];
            //pMemContent += 1;
            for (w = 0; w < pVBuf->u32Width / 2; w++)
            {
                TmpBuff[w] = *pMemContent;
                pMemContent += 2;
            }
            memcpy(*pOutBuf + pVBuf->u32Width*pVBuf->u32Height + pVBuf->u32Width/2 * u32UvHeight + pVBuf->u32Width/2 * h, 
                        TmpBuff, 
                        pVBuf->u32Width / 2);
        }
    }
    utils_print("done %d!\n", pVBuf->u32TimeRef);   

    HI_MPI_SYS_Munmap(pUserPageAddr[0], u32Size);
    pUserPageAddr[0] = HI_NULL;
}

static HI_S32 VPSS_Restore()
{
    HI_S32 s32Ret = HI_FAILURE;
    VPSS_CHN_ATTR_S stChnAttr;

    if (VB_INVALID_POOLID != stFrame.u32PoolId)
    {
        s32Ret = HI_MPI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrame);

        if (HI_SUCCESS != s32Ret)
        {
            printf("Release Chn Frame error!!!\n");
        }

        stFrame.u32PoolId = VB_INVALID_POOLID;
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

    if (u32VpssDepthFlag)
    {

        s32Ret = HI_MPI_VPSS_GetChnAttr(VpssGrp, VpssChn, &stChnAttr);
        if (s32Ret != HI_SUCCESS)
        {
            printf("get chn attr error!!!\n");
        }

        stChnAttr.u32Depth = u32OrigDepth;
        s32Ret = HI_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stChnAttr);
        if (s32Ret != HI_SUCCESS)
        {
            printf("set depth error!!!\n");
        }
        u32VpssDepthFlag = 0;
    }

    return HI_SUCCESS;
}

#if 0
BOOL get_qrcode_yuv_buffer(void **qrcode_info)
{
    HI_U32 u32Cnt = 1;
    HI_U32 u32Depth = 2;
    HI_S32 s32MilliSec = -1;
    HI_S32 s32Times = 10;
    HI_U32 u32Width = 0;
    HI_U32 u32Height = 0;
    HI_S32 i = 0;
    HI_S32 s32Ret;
    VPSS_CHN_ATTR_S stChnAttr;

    char* qrcode_buffer = NULL;

    s32Ret = HI_MPI_VPSS_GetChnAttr(VpssGrp, VpssChn, &stChnAttr);
    u32OrigDepth = stChnAttr.u32Depth;
    if (s32Ret != HI_SUCCESS)
    {
        printf("get chn attr error!!!\n");
        return FALSE;
    }

    stChnAttr.u32Depth = u32Depth;
    s32Ret = HI_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("set depth error!!!\n");
        VPSS_Restore(VpssGrp, VpssChn);
        return FALSE;
    }

    u32VpssDepthFlag = 1;

    memset(&stFrame, 0, sizeof(stFrame));
    stFrame.u32PoolId = VB_INVALID_POOLID;

    while (HI_MPI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stFrame, s32MilliSec) != HI_SUCCESS)
    {
        s32Times--;

        if (0 >= s32Times)
        {
            printf("get frame error for 10 times,now exit !!!\n");
            VPSS_Restore(VpssGrp, VpssChn);
            return FALSE;
        }
        usleep(40000);
    }

    if (VIDEO_FORMAT_LINEAR != stFrame.stVFrame.enVideoFormat)
    {
        printf("only support linear frame dump!\n");
        HI_MPI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrame);
        stFrame.u32PoolId = VB_INVALID_POOLID;
        return FALSE;
    }

    s32Ret = HI_MPI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrame);
    if (HI_SUCCESS != s32Ret)
    {
        printf("Release frame error ,now exit !!!\n");
        VPSS_Restore(VpssGrp, VpssChn);
        return FALSE;
    }

    stFrame.u32PoolId = VB_INVALID_POOLID;

    /* get frame  */
    while (u32Cnt--)
    {
        if (HI_MPI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stFrame, s32MilliSec) != HI_SUCCESS)
        {
            printf("Get frame fail \n");
            usleep(1000);
            continue;
        }

        sample_yuv_8bit_dump(&stFrame.stVFrame, (void *)&qrcode_buffer);

        printf("Get frame %d!!\n", u32Cnt);
        /* release frame after using */
        s32Ret = HI_MPI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrame);
        if (HI_SUCCESS != s32Ret)
        {
            printf("Release frame error ,now exit !!!\n");
            VPSS_Restore(VpssGrp, VpssChn);
            return FALSE;
        }

        stFrame.u32PoolId = VB_INVALID_POOLID;
    }

    if (NULL == qrcode_buffer)
    {
        VPSS_Restore(VpssGrp, VpssChn);
        return FALSE;
    }
  
    //for test, write buffer to file
 #ifdef DEBUG   
    FILE* pfd = fopen("qrcode_vpss.yuv", "wb");
    fwrite(qrcode_buffer, stFrame.stVFrame.u32Stride[0] * stFrame.stVFrame.u32Height * 3 / 2, 1, pfd);
    fflush(pfd);
    fclose(pfd);
 #endif 
  
    int ret = QRcode_Detection(qrcode_buffer, stFrame.stVFrame.u32Stride[0], stFrame.stVFrame.u32Height, qrcode_info);
    if (ret == -1)
    {
        utils_print("QRCode not recognize...\n");
        VPSS_Restore(VpssGrp, VpssChn);
        return FALSE;
    }

    if(qrcode_buffer != NULL)
    {
        utils_free(qrcode_buffer);
        qrcode_buffer = NULL;
    }

   
    VPSS_Restore(VpssGrp, VpssChn);
    return TRUE;
}
#endif

BOOL get_qrcode_yuv_buffer(void **qrcode_info)
{
    int width = hxt_get_video_width_cfg();
    int height = hxt_get_video_height_cfg();
    int size = width * height * 3 / 2 ;

    char *yuv_buff = (char*) utils_malloc(size);
    if(NULL == yuv_buff)
    {
        return FALSE;
    }

    int fd = utils_open_fifo(VIDEO_FIFO, O_RDONLY);
    if (-1 == fd)
    {
        return FALSE;
    }
    utils_print("To read yuv data from fifo....\n");
    int read_count = read(fd, yuv_buff, size);
    utils_print("read %d yuv bytes from fifo\n", read_count);

 #ifdef DEBUG   
    FILE* pfd = fopen("qrcode_vpss.yuv", "wb");
    fwrite(yuv_buff, size, 1, pfd);
    fflush(pfd);
    fclose(pfd);
 #endif 

    int ret = QRcode_Detection(yuv_buff, width, height, qrcode_info);
    if (ret == -1)
    {
        utils_print("QRCode not recognize...\n");
        return FALSE;
    }

    if(yuv_buff != NULL)
    {
        utils_free(yuv_buff);
        yuv_buff = NULL;
    }

    return TRUE;
}

