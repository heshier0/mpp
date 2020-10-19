#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <hi_common.h>
#include <hi_comm_sys.h>
#include <hi_comm_vb.h>
#include <mpi_vb.h>
#include <hi_comm_vpss.h>
#include <mpi_vpss.h>

#include <QRcode_Detection.h>

#include "utils.h"
#include "hxt_client.h"
#include "board_func.h"


static void sample_yuv_8bit_dump(VIDEO_FRAME_S* pVBuf, void** pOutBuf)
{
    unsigned int w, h;
    char* pVBufVirt_Y;
    char* pVBufVirt_C;
    char* pMemContent;
    unsigned char TmpBuff[20480]; //If this value is too small and the image is big, this memory may not be enough
    HI_U64 phy_addr;
    PIXEL_FORMAT_E  enPixelFormat = pVBuf->enPixelFormat;
    HI_U32 u32UvHeight = 0;/*When the storage format is a planar format, this variable is used to keep the height of the UV component */
    HI_BOOL bUvInvert;
    HI_CHAR* pUserPageAddr[2] = {HI_NULL, HI_NULL};
    
    HI_U32 u32Size = (pVBuf->u32Stride[0]) * (pVBuf->u32Height) * 3 / 2;
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

    for (h = 0; h < pVBuf->u32Height; h++)
    {
        pMemContent = pVBufVirt_Y + h * pVBuf->u32Stride[0];
        memcpy(*pOutBuf + h * pVBuf->u32Width, pMemContent, pVBuf->u32Width);
    }

    if (PIXEL_FORMAT_YUV_400 != enPixelFormat)
    {
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

        for (h = 0; h < u32UvHeight; h++)
        {
            pMemContent = pVBufVirt_C + h * pVBuf->u32Stride[1];
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
    // utils_print("done %d!\n", pVBuf->u32TimeRef);   

    HI_MPI_SYS_Munmap(pUserPageAddr[0], u32Size);
    pUserPageAddr[0] = HI_NULL;

    return u32Size;
}

BOOL board_get_qrcode_yuv_buffer()
{
    char *qrcode_info = NULL;
    char *yuv_data = NULL;
    int width = hxt_get_video_height();
    int height = hxt_get_video_width();
    
    board_get_yuv_from_vpss_chn(&yuv_data);
    if(NULL == yuv_data)
    {
        return FALSE;
    }
    
 #ifndef DEBUG   
    utils_save_yuv_test(yuv_data, width, height);
 #endif 

    int ret = QRcode_Detection(yuv_data, width, height, &qrcode_info);
    if (ret == -1)
    {
        utils_print("QRCode not recognize...\n");
        if(yuv_data != NULL)
        {
            utils_free(yuv_data);
            yuv_data = NULL;
        }
        return FALSE;
    }
    utils_print("QRCode:[%s]\n", qrcode_info);

    if(yuv_data != NULL)
    {
        utils_free(yuv_data);
        yuv_data = NULL;
    }

    if (!hxt_query_wifi_info((void*)qrcode_info))
    {
        //board_set_led_status(NORMAL);
        return FALSE;
    }
    
    return TRUE;
}

void board_get_yuv_from_vpss_chn(char** yuv_buf)
{
    HI_S32 ret_val;
    VPSS_GRP vpss_grp = 0;
    HI_S32 time_out = 3000;
    VPSS_CHN vpss_chn = 2;
    VIDEO_FRAME_INFO_S  video_frame;

    memset(&video_frame, 0, sizeof(video_frame));
    video_frame.u32PoolId = VB_INVALID_POOLID;

    ret_val = HI_MPI_VPSS_GetChnFrame(vpss_grp, vpss_chn, &video_frame, time_out);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("vpss_grp[%d] get vpss_chn[%d] frame failed, ret:0x%08x\n",vpss_grp, vpss_chn, ret_val);
        return;
    }

    /* to change to yuv picture */
    sample_yuv_8bit_dump(&video_frame.stVFrame, (void**)yuv_buf);
 
    ret_val = HI_MPI_VPSS_ReleaseChnFrame(vpss_grp, vpss_chn, &video_frame);
    if ( HI_SUCCESS != ret_val )
    {
        utils_print("vpss release frame failed, ret:0x%08x\n", ret_val);
        return;
    }  
    video_frame.u32PoolId = VB_INVALID_POOLID;

    return;
}

