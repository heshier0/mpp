#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/prctl.h>
#include <semaphore.h>


#include "sample_comm.h"
#include "acodec.h"
#include "audio_mp3_adp.h"

#include "fifobuffer.h"
#include "databuffer.h"
#include "common.h"
#include "utils.h"

#define BYTE_ALIGN(a,b)    ((( a ) / b) * b)

DATABUFFER g_video_buffer;
FIFO_BUFFER g_frame_fifo;

WDR_MODE_E      enWDRMode       = WDR_MODE_NONE;
DYNAMIC_RANGE_E enDynamicRange  = DYNAMIC_RANGE_SDR8;
PIXEL_FORMAT_E  enPixFormat     = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
VIDEO_FORMAT_E  enVideoFormat   = VIDEO_FORMAT_LINEAR;
COMPRESS_MODE_E enCompressMode  = COMPRESS_MODE_NONE;
VI_VPSS_MODE_E  enMastPipeMode  = VI_OFFLINE_VPSS_OFFLINE;

static SAMPLE_VENC_GETSTREAM_PARA_S gs_stPara;
static sem_t sem_snap[2];

static SAMPLE_VI_CONFIG_S g_vi_configs;

static BOOL g_start_record = FALSE;
static BOOL g_start_snap = FALSE;

static void stop_vi()
{
    SAMPLE_COMM_VI_StopVi(&g_vi_configs);
}

static void stop_all_vpss()
{
    HI_BOOL chn_enable[VPSS_MAX_PHY_CHN_NUM] = { HI_TRUE, HI_TRUE, HI_FAILURE, HI_FAILURE };
    VPSS_GRP vpss_grp[2] = {0, 1};
    for(int i = 0; i < 2; i++)
    {
        SAMPLE_COMM_VPSS_Stop(vpss_grp[i], chn_enable);
    }
}

static void stop_all_venc()
{
    VENC_CHN venc_chn[4] = {0, 1, 2, 3};
    for (int i = 0; i < 4; i ++)
    {
        SAMPLE_COMM_VENC_Stop(venc_chn[i]);
    }
}

static void unbind_vi_vpss()
{
    VI_PIPE vi_pipe[3] = {0, 1, 2};
    VI_CHN vi_chn = 0;
    VPSS_GRP vpss_grp[2] = {0, 1};

    SAMPLE_COMM_VI_UnBind_VPSS(vi_pipe[2], vi_chn, vpss_grp[1]);
    SAMPLE_COMM_VI_UnBind_VPSS(vi_pipe[0], vi_chn, vpss_grp[0]);
}

static void unbind_vpss_venc()
{
    VPSS_GRP vpss_grp[2] = {0, 1};
    VPSS_CHN vpss_chn0 = VPSS_CHN1;
    VPSS_CHN vpss_chn1 = VPSS_CHN2;
    VENC_CHN venc_chn[4] = {0, 1, 2, 3};

    SAMPLE_COMM_VPSS_UnBind_VENC(vpss_grp[0], vpss_chn1, venc_chn[0]);
    SAMPLE_COMM_VPSS_UnBind_VENC(vpss_grp[1], vpss_chn1, venc_chn[1]);
    SAMPLE_COMM_VPSS_UnBind_VENC(vpss_grp[0], vpss_chn1, venc_chn[2]);
    SAMPLE_COMM_VPSS_UnBind_VENC(vpss_grp[1], vpss_chn1, venc_chn[3]);

    return;
}

/* two vi */
/* sensor0: sony imx307, use library with imx327*/
/* sensor1: jx f23*/
static BOOL start_vi()
{
    BOOL ret = TRUE;
    HI_S32 ret_val = HI_FAILURE;
    HI_S32 vi_count = 2;
    VI_DEV vi_dev[2] = {0, 1};
    VI_PIPE vi_pipe[4] = {0, 1, 2, 3};
    VI_CHN vi_chn = 0;

    PIC_SIZE_E enPicSize;
    SIZE_S stSize;
   
    memset(&g_vi_configs, 0, sizeof(SAMPLE_VI_CONFIG_S));
    SAMPLE_COMM_VI_GetSensorInfo(&g_vi_configs);

    g_vi_configs.s32WorkingViNum                        = vi_count;
    g_vi_configs.as32WorkingViId[0]                     = 0;
    g_vi_configs.as32WorkingViId[1]                     = 1;

    g_vi_configs.astViInfo[0].stSnsInfo.MipiDev         = vi_dev[0];
    g_vi_configs.astViInfo[0].stSnsInfo.s32BusId        = 0;
    g_vi_configs.astViInfo[0].stSnsInfo.s32SnsId        = 0;
    g_vi_configs.astViInfo[0].stSnsInfo.enSnsType       = SENSOR0_TYPE;
    g_vi_configs.astViInfo[0].stDevInfo.ViDev           = vi_dev[0];
    g_vi_configs.astViInfo[0].stDevInfo.enWDRMode       = WDR_MODE_NONE;
    g_vi_configs.astViInfo[0].stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    g_vi_configs.astViInfo[0].stPipeInfo.bIspBypass     = HI_FALSE;
    g_vi_configs.astViInfo[0].stPipeInfo.aPipe[0]       = vi_pipe[0];
    g_vi_configs.astViInfo[0].stPipeInfo.aPipe[1]       = -1;
    g_vi_configs.astViInfo[0].stPipeInfo.aPipe[2]       = -1;
    g_vi_configs.astViInfo[0].stPipeInfo.aPipe[3]       = -1;
    g_vi_configs.astViInfo[0].stChnInfo.ViChn           = vi_chn;
    g_vi_configs.astViInfo[0].stChnInfo.enPixFormat     = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    g_vi_configs.astViInfo[0].stChnInfo.enDynamicRange  = DYNAMIC_RANGE_SDR8;
    g_vi_configs.astViInfo[0].stChnInfo.enVideoFormat   = VIDEO_FORMAT_LINEAR;
    g_vi_configs.astViInfo[0].stChnInfo.enCompressMode  = COMPRESS_MODE_NONE;

    g_vi_configs.astViInfo[1].stSnsInfo.MipiDev         = vi_dev[1];
    g_vi_configs.astViInfo[1].stSnsInfo.s32BusId        = 1;
    g_vi_configs.astViInfo[1].stSnsInfo.s32SnsId        = 1;
    g_vi_configs.astViInfo[1].stSnsInfo.enSnsType       = SENSOR1_TYPE;
    g_vi_configs.astViInfo[1].stDevInfo.ViDev           = vi_dev[1];
    g_vi_configs.astViInfo[1].stDevInfo.enWDRMode       = WDR_MODE_NONE;
    g_vi_configs.astViInfo[1].stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    g_vi_configs.astViInfo[1].stPipeInfo.bIspBypass     = HI_FALSE;
    g_vi_configs.astViInfo[1].stPipeInfo.aPipe[0]       = vi_pipe[2];
    g_vi_configs.astViInfo[1].stPipeInfo.aPipe[1]       = -1;
    g_vi_configs.astViInfo[1].stPipeInfo.aPipe[2]       = -1;
    g_vi_configs.astViInfo[1].stPipeInfo.aPipe[3]       = -1;
    g_vi_configs.astViInfo[1].stChnInfo.ViChn           = vi_chn;
    g_vi_configs.astViInfo[1].stChnInfo.enPixFormat     = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    g_vi_configs.astViInfo[1].stChnInfo.enDynamicRange  = DYNAMIC_RANGE_SDR8;
    g_vi_configs.astViInfo[1].stChnInfo.enVideoFormat   = VIDEO_FORMAT_LINEAR;
    g_vi_configs.astViInfo[1].stChnInfo.enCompressMode  = COMPRESS_MODE_NONE;

    /*get picture size*/
    ret_val = SAMPLE_COMM_VI_GetSizeBySensor(g_vi_configs.astViInfo[0].stSnsInfo.enSnsType, &enPicSize);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("get picture size by sensor failed!\n");
        return FALSE;
    }

    ret_val = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("get picture size failed!\n");
        return FALSE;
    }

    /*start vi*/
    ret_val = SAMPLE_COMM_VI_StartVi(&g_vi_configs);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("start vi failed.s32Ret:0x%x !\n", ret_val);
        return FALSE;
    }

    return ret;
}

static BOOL rotate_picture()
{
    HI_S32 ret_val = HI_FAILURE;
    ROTATION_E rotate =  ROTATION_90;  //ROTATION_270;
    // VI_PIPE vi_pipe[4] = {0, 1, 2, 3};
    // VI_CHN vi_chn = 0;

    // ret_val = HI_MPI_VI_SetChnRotation(vi_pipe[0], vi_chn, ROTATION_270);
    // if (HI_SUCCESS != ret_val)
    // {
    //     utils_print("ViPipe 0 rotate failed, s32Ret: 0x%x !\n", ret_val);
    //     stop_vi();
    //     return FALSE;
    // }

    // ret_val = HI_MPI_VI_SetChnRotation(vi_pipe[2], vi_chn, ROTATION_270);
    // if (HI_SUCCESS != ret_val)
    // {
    //     utils_print("ViPipe 2 rotate failed, s32Ret: 0x%x !\n", ret_val);
    //     stop_vi();
    //     return FALSE;
    // }

    VPSS_GRP           vpss_grp[2]        = {0, 1};
    VPSS_CHN           vpss_chn0  = VPSS_CHN1, vpss_chn1 = VPSS_CHN2;

    ret_val = HI_MPI_VPSS_SetChnRotation(vpss_grp[0], vpss_chn0, rotate);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("vpss grp0 chn1 rotate failed, s32Ret: 0x%x !\n", ret_val);
        return FALSE;
    }

    ret_val = HI_MPI_VPSS_SetChnRotation(vpss_grp[0], vpss_chn1, rotate);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("vpss grp0 chn2 rotate failed, s32Ret: 0x%x !\n", ret_val);
        return FALSE;
    }
    
    ret_val = HI_MPI_VPSS_SetChnRotation(vpss_grp[1], vpss_chn0, rotate);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("vpss grp1 chn1 rotate failed, s32Ret: 0x%x !\n", ret_val);
        return FALSE;
    }
    
    ret_val = HI_MPI_VPSS_SetChnRotation(vpss_grp[1], vpss_chn1, rotate);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("vpss grp1 chn2 rotate failed, s32Ret: 0x%x !\n", ret_val);
        return FALSE;
    }

    return TRUE;
}

static BOOL start_vpss()
{
    BOOL ret = TRUE;
    
    HI_S32             ret_val = HI_FAILURE;
    VPSS_GRP           vpss_grp[2]        = {0, 1};
    VPSS_GRP_ATTR_S    vpss_grp_attrs     = {0};
    VPSS_CHN           vpss_chn0  = VPSS_CHN1, vpss_chn1 = VPSS_CHN2;
    HI_BOOL            chn_enable[VPSS_MAX_PHY_CHN_NUM] = {HI_FALSE};
    VPSS_CHN_ATTR_S    vpss_chn_attrs[VPSS_MAX_PHY_CHN_NUM] = {0};
    PIC_SIZE_E enPicSize;
    SIZE_S stSize;

    /*get picture size*/
    ret_val = SAMPLE_COMM_VI_GetSizeBySensor(g_vi_configs.astViInfo[0].stSnsInfo.enSnsType, &enPicSize);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("get picture size by sensor failed!\n");
        return FALSE;
    }

    ret_val = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("get picture size failed!\n");
        return FALSE;
    }

    /* init vpss groups */
    hi_memset(&vpss_grp_attrs, sizeof(VPSS_GRP_ATTR_S), 0, sizeof(VPSS_GRP_ATTR_S));
    vpss_grp_attrs.u32MaxW                        = 1920;
    vpss_grp_attrs.u32MaxH                        = 1920;
    vpss_grp_attrs.stFrameRate.s32SrcFrameRate    = -1;
    vpss_grp_attrs.stFrameRate.s32DstFrameRate    = -1;
    vpss_grp_attrs.enDynamicRange                 = DYNAMIC_RANGE_SDR8;
    vpss_grp_attrs.enPixelFormat                  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vpss_grp_attrs.bNrEn                          = HI_TRUE;

    /* init vpps channels */
    chn_enable[vpss_chn0] = HI_TRUE;
    vpss_chn_attrs[vpss_chn0].u32Width                    = hxt_get_video_width_cfg();
    vpss_chn_attrs[vpss_chn0].u32Height                   = hxt_get_video_height_cfg();
    vpss_chn_attrs[vpss_chn0].enChnMode                   = VPSS_CHN_MODE_USER;
    vpss_chn_attrs[vpss_chn0].enCompressMode              = COMPRESS_MODE_NONE;
    vpss_chn_attrs[vpss_chn0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    vpss_chn_attrs[vpss_chn0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    vpss_chn_attrs[vpss_chn0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vpss_chn_attrs[vpss_chn0].stFrameRate.s32SrcFrameRate = -1;
    vpss_chn_attrs[vpss_chn0].stFrameRate.s32DstFrameRate = -1;
    vpss_chn_attrs[vpss_chn0].u32Depth                    = 1;
    vpss_chn_attrs[vpss_chn0].bMirror                     = HI_FALSE;
    vpss_chn_attrs[vpss_chn0].bFlip                       = HI_FALSE;
    vpss_chn_attrs[vpss_chn0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;

    chn_enable[vpss_chn1] = HI_TRUE;
    vpss_chn_attrs[vpss_chn1].u32Width                    = hxt_get_video_width_cfg();
    vpss_chn_attrs[vpss_chn1].u32Height                   = hxt_get_video_height_cfg();
    vpss_chn_attrs[vpss_chn1].enChnMode                   = VPSS_CHN_MODE_USER;
    vpss_chn_attrs[vpss_chn1].enCompressMode              = COMPRESS_MODE_NONE;
    vpss_chn_attrs[vpss_chn1].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    vpss_chn_attrs[vpss_chn1].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    vpss_chn_attrs[vpss_chn1].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vpss_chn_attrs[vpss_chn1].stFrameRate.s32SrcFrameRate = -1;
    vpss_chn_attrs[vpss_chn1].stFrameRate.s32DstFrameRate = -1;
    vpss_chn_attrs[vpss_chn1].u32Depth                    = 1;
    vpss_chn_attrs[vpss_chn1].bMirror                     = HI_FALSE;
    vpss_chn_attrs[vpss_chn1].bFlip                       = HI_FALSE;
    vpss_chn_attrs[vpss_chn1].stAspectRatio.enMode        = ASPECT_RATIO_NONE;

    /* start vpss group 0 */
    ret_val = SAMPLE_COMM_VPSS_Start(vpss_grp[0], chn_enable, &vpss_grp_attrs, vpss_chn_attrs);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("start vpss group failed. s32Ret: 0x%x !\n", ret_val);
        return FALSE;
    }
    /* start vpss group 1 */
    ret_val = SAMPLE_COMM_VPSS_Start(vpss_grp[1], chn_enable, &vpss_grp_attrs, vpss_chn_attrs);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("start vpss group failed. s32Ret: 0x%x !\n", ret_val);
        SAMPLE_COMM_VPSS_Stop(vpss_grp[0], chn_enable);
        return FALSE;
    }

    return ret;
}

static BOOL start_venc()
{
    BOOL ret = TRUE;
    HI_S32 ret_val = HI_FAILURE;
    HI_S32 gop_val = 25;
    VENC_GOP_MODE_E gop_mode  = VENC_GOPMODE_NORMALP;
    VENC_GOP_ATTR_S gop_attrs = {0};
    VENC_CHN venc_chn[4] = {0, 1, 2, 3};
    SAMPLE_RC_E rc_mode = SAMPLE_RC_CBR;
    HI_BOOL rcn_ref_shared_buf = HI_TRUE;
    int gop_value = 0;
    
    SIZE_S picture_size;
    picture_size.u32Width = hxt_get_video_height_cfg();
    picture_size.u32Height = hxt_get_video_width_cfg();

    ret_val = SAMPLE_COMM_VENC_GetGopAttr(gop_mode, &gop_attrs);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("venc get GopAttr failed, s32Ret: 0x%x !\n", ret_val);
        return FALSE;
    }

    ret_val = SAMPLE_COMM_VENC_Start(venc_chn[0], PT_H264, picture_size, rc_mode, gop_value, rcn_ref_shared_buf, &gop_attrs);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("start venc chn %d failed, s32Ret: 0x%x !\n", venc_chn[0], ret_val);
        return FALSE;
    }

    ret_val = SAMPLE_COMM_VENC_Start(venc_chn[1], PT_H264, picture_size, rc_mode, gop_value, rcn_ref_shared_buf, &gop_attrs);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("start venc chn %d failed, s32Ret: 0x%x !\n", venc_chn[1], ret_val);
        SAMPLE_COMM_VENC_Stop(venc_chn[0]);
        return FALSE;
    }
    
    ret_val = SAMPLE_COMM_VENC_SnapStart(venc_chn[2], &picture_size, HI_FALSE);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("start sanp chn %d failed, s32Ret: 0x%x !\n", venc_chn[2], ret_val);
        SAMPLE_COMM_VENC_Stop(venc_chn[0]);
        SAMPLE_COMM_VENC_Stop(venc_chn[1]);
        return FALSE;
    }
    
    ret_val = SAMPLE_COMM_VENC_SnapStart(venc_chn[3], &picture_size, HI_FALSE);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("start sanp chn %d failed, s32Ret: 0x%x !\n", venc_chn[3], ret_val);
        SAMPLE_COMM_VENC_Stop(venc_chn[0]);
        SAMPLE_COMM_VENC_Stop(venc_chn[1]);
        SAMPLE_COMM_VENC_SnapStop(venc_chn[2]);
        return FALSE;
    }

    return ret;
}

static BOOL bind_vi_vpss()
{
    BOOL ret = TRUE;
    HI_S32 ret_val = HI_FAILURE;

    VI_PIPE vi_pipe[4] = {0, 1, 2, 3};
    VI_CHN vi_chn = 0;
    VPSS_GRP vpss_grp[2] = {0, 1};

    ret_val = SAMPLE_COMM_VI_Bind_VPSS(vi_pipe[0], vi_chn, vpss_grp[0]);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("vpss bind vi failed. s32Ret: 0x%x !\n", ret_val);
        return FALSE;
    }

    ret_val = SAMPLE_COMM_VI_Bind_VPSS(vi_pipe[2], vi_chn, vpss_grp[1]);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("vpss bind vi failed. s32Ret: 0x%x !\n", ret_val);
        SAMPLE_COMM_VI_UnBind_VPSS(vi_pipe[0], vi_chn, vpss_grp[0]);
        return FALSE;
    }

    return ret;
}

static BOOL bind_vpss_venc()
{
    BOOL ret = TRUE;
    HI_S32 ret_val= HI_FAILURE;
    VPSS_GRP vpss_grp[2] = {0, 1};
    VPSS_CHN vpss_chn0 = VPSS_CHN1;
    VPSS_CHN vpss_chn1 = VPSS_CHN2;
    VENC_CHN venc_chn[4] = {0, 1, 2, 3};

    ret_val = SAMPLE_COMM_VPSS_Bind_VENC(vpss_grp[0], vpss_chn1, venc_chn[0]);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("venc bind vpss failed, s32Ret: 0x%x !\n", ret_val);
        return FALSE;
    }

    ret_val = SAMPLE_COMM_VPSS_Bind_VENC(vpss_grp[1], vpss_chn1, venc_chn[1]);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("venc bind vpss failed, s32Ret: 0x%x !\n", ret_val);
        SAMPLE_COMM_VPSS_UnBind_VENC(vpss_grp[0], vpss_chn1, venc_chn[0]);
        return FALSE;
    }

    ret_val = SAMPLE_COMM_VPSS_Bind_VENC(vpss_grp[0], vpss_chn1, venc_chn[2]);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("venc bind vpss failed, s32Ret: 0x%x !\n", ret_val);
        SAMPLE_COMM_VPSS_UnBind_VENC(vpss_grp[0], vpss_chn1, venc_chn[0]);
        SAMPLE_COMM_VPSS_UnBind_VENC(vpss_grp[1], vpss_chn1, venc_chn[1]);
        return FALSE;
    }

    ret_val = SAMPLE_COMM_VPSS_Bind_VENC(vpss_grp[1], vpss_chn1, venc_chn[3]);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("venc bind vpss failed, s32Ret: 0x%x !\n", ret_val);
        SAMPLE_COMM_VPSS_UnBind_VENC(vpss_grp[0], vpss_chn1, venc_chn[0]);
        SAMPLE_COMM_VPSS_UnBind_VENC(vpss_grp[1], vpss_chn1, venc_chn[1]);
        SAMPLE_COMM_VPSS_UnBind_VENC(vpss_grp[0], vpss_chn1, venc_chn[2]);
        return FALSE;
    }

    return ret;
}

static void sample_yuv_8bit_dump(VIDEO_FRAME_S* pVBuf, void **pOutBuf)
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

    // utils_print("saving......Y......\n");
    for (h = 0; h < pVBuf->u32Height; h++)
    {
        pMemContent = pVBufVirt_Y + h * pVBuf->u32Stride[0];
        memcpy(*pOutBuf + h * pVBuf->u32Width, pMemContent, pVBuf->u32Width);
    }

    if (PIXEL_FORMAT_YUV_400 != enPixelFormat)
    {
        // utils_print("saving......U......\n");
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

        // utils_print("saving......V......\n");
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
    // utils_print("done %d!\n", pVBuf->u32TimeRef);   

    HI_MPI_SYS_Munmap(pUserPageAddr[0], u32Size);
    pUserPageAddr[0] = HI_NULL;
}

static HI_S32 SnapSave(HI_S32 ch, VENC_STREAM_S * pstStream)
{
	HI_S32 s32Ret;
    char acFile[FILE_NAME_LEN]    = {0};
    FILE* pFile;
    
	struct tm * tm;
	time_t now = time(0);
	
	tm = localtime(&now);
	
	snprintf(acFile, 128, "SNAP_CH%02d-%04d%02d%02d-%02d%02d%02d.jpg", ch - 2,
	         tm->tm_year + 1900,
	         tm->tm_mon + 1,
	         tm->tm_mday,
	         tm->tm_hour,
	         tm->tm_min,
	         tm->tm_sec);
	         
    pFile = fopen(acFile, "wb");
    if (pFile == NULL)
    {
        SAMPLE_PRT("open file err\n");

        return HI_FAILURE;
    }
    
    s32Ret = SAMPLE_COMM_VENC_SaveStream(pFile, pstStream);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("save snap picture failed!\n");
    }	
    
    fclose(pFile);
    
    return s32Ret;
}

static void* sample_pcm_cb(void *data)
{
    HI_S32 i, j, s32Ret;
    AI_CHN      AiChn = 0;
    ADEC_CHN    AdChn = 0;
    HI_S32      s32AiChnCnt;
    HI_S32      s32AencChnCnt;
    AENC_CHN    AeChn = 0;
    AIO_ATTR_S stAioAttr;
    AI_TALKVQE_CONFIG_S stAiVqeTalkAttr;
    HI_VOID     *pAiVqeAttr = NULL;


    HI_MPI_AENC_AacInit();

    AUDIO_DEV   AiDev = SAMPLE_AUDIO_INNER_AI_DEV;
    stAioAttr.enSamplerate   = AUDIO_SAMPLE_RATE_16000;
    stAioAttr.enBitwidth     = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode     = AIO_MODE_I2S_MASTER;
    stAioAttr.enSoundmode    = AUDIO_SOUND_MODE_MONO;
    stAioAttr.u32EXFlag      = 0;
    stAioAttr.u32FrmNum      = 30;
    stAioAttr.u32PtNumPerFrm = MP3_SAMPLES_PER_FRAME;//1024;
    stAioAttr.u32ChnCnt      = 2;
    stAioAttr.u32ClkSel      = 0;
    stAioAttr.enI2sType      = AIO_I2STYPE_INNERCODEC;
    
    //回声消除功能
    // HI_S16 s16ERLBand[6] = {4, 6, 36, 49, 50, 51};
    // HI_S16 s16ERL[7] = {7, 10, 16, 10, 18, 18, 18};
    // memset(&stAiVqeTalkAttr, 0, sizeof(AI_TALKVQE_CONFIG_S));
    // stAiVqeTalkAttr.enWorkstate = VQE_WORKSTATE_COMMON;
    // stAiVqeTalkAttr.s32FrameSample = 1024;
    // stAiVqeTalkAttr.s32WorkSampleRate = AUDIO_SAMPLE_RATE_16000;
    // stAiVqeTalkAttr.stAecCfg.bUsrMode = HI_TRUE;
    // stAiVqeTalkAttr.stAecCfg.s16EchoBandLow = 10;
    // stAiVqeTalkAttr.stAecCfg.s16EchoBandLow2 = 25;
    // stAiVqeTalkAttr.stAecCfg.s16EchoBandHigh = 28;
    // stAiVqeTalkAttr.stAecCfg.s16EchoBandHigh2 = 35;
    // memcpy(stAiVqeTalkAttr.stAecCfg.s16ERLBand, &s16ERLBand, sizeof(s16ERLBand));
    // memcpy(stAiVqeTalkAttr.stAecCfg.s16ERL, &s16ERL, sizeof(s16ERL));
    // stAiVqeTalkAttr.stAecCfg.s16VioceProtectFreqL = 3;
    // stAiVqeTalkAttr.stAecCfg.s16VioceProtectFreqL1 = 6;
    // stAiVqeTalkAttr.stAgcCfg.bUsrMode = HI_FALSE;
    // stAiVqeTalkAttr.stAnrCfg.bUsrMode = HI_TRUE;
    // stAiVqeTalkAttr.stAnrCfg.s16NrIntensity = 25;
    // stAiVqeTalkAttr.stAnrCfg.s16NoiseDbThr = 60;
    // stAiVqeTalkAttr.stAnrCfg.s8SpProSwitch = 0;
    // stAiVqeTalkAttr.stHpfCfg.bUsrMode = HI_TRUE;
    // stAiVqeTalkAttr.stHpfCfg.enHpfFreq = AUDIO_HPF_FREQ_150;
    // stAiVqeTalkAttr.u32OpenMask = AI_TALKVQE_MASK_AEC | AI_TALKVQE_MASK_ANR | AI_TALKVQE_MASK_HPF;

    //开启Audio In
    s32AiChnCnt = stAioAttr.u32ChnCnt;
    // s32Ret = SAMPLE_COMM_AUDIO_StartAi(AiDev, s32AiChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE, pAiVqeAttr, 2);
    s32Ret = SAMPLE_COMM_AUDIO_StartAi(AiDev, s32AiChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE, NULL, 0);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        goto AIAENC_ERR6;
    }
 
    //音频codec属性设置
    s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        goto AIAENC_ERR5;
    }

    //音频编码器属性设置
    s32AencChnCnt = stAioAttr.u32ChnCnt >> 1; //stAioAttr.enSoundmode;
    s32Ret = SAMPLE_COMM_AUDIO_StartAenc(s32AencChnCnt, &stAioAttr, PT_LPCM);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        goto AIAENC_ERR5;
    }

    //音频输入与音频编码器绑定
    for (i = 0; i < s32AencChnCnt; i++)
    {
        AeChn = i;
        AiChn = i;
        s32Ret = SAMPLE_COMM_AUDIO_AencBindAi(AiDev, AiChn, AeChn);
        if (s32Ret != HI_SUCCESS)
        {
            utils_print("ret=%d\n",s32Ret);
            for (j=0; j<i; j++)
            {
                SAMPLE_COMM_AUDIO_AencUnbindAi(AiDev, j, j);
            }
            goto AIAENC_ERR4;
        }

        printf("Ai(%d,%d) bind to AencChn:%d ok!\n", AiDev , AiChn, AeChn);
    }

    //编码
    HI_S32 AencFd;
    AUDIO_STREAM_S stStream;
    fd_set read_fds;
    struct timeval TimeoutVal;
    int fd = utils_open_fifo(PCM_FIFO, O_WRONLY);
    if (-1 == fd)
    {
        goto AIAENC_ERR1;
    }
    FD_ZERO(&read_fds);
    AencFd = HI_MPI_AENC_GetFd(AeChn);
    FD_SET(AencFd, &read_fds);
    
    while(g_voice_status)
    {
        TimeoutVal.tv_sec = 1;
        TimeoutVal.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_SET(AencFd, &read_fds);

        s32Ret = select(AencFd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if(s32Ret < 0)
        {
            break;
        }
        else if (s32Ret == 0)
        {
            utils_print("get aenc stream select time out\n");
            break;
        }

        if(FD_ISSET(AencFd, &read_fds))
        {
            //get stream from aenc chn
            s32Ret = HI_MPI_AENC_GetStream(AeChn, &stStream, HI_FALSE);
            if (HI_SUCCESS != s32Ret )
            {
                utils_print("HI_MPI_ADEC_SendStream(%d), failed with %#x!\n", AeChn, s32Ret);
                break;
            }

            int write_count = write(fd, stStream.pStream, stStream.u32Len);
            //utils_print("write %d bytes into fifo\n", write_count);

            s32Ret = HI_MPI_AENC_ReleaseStream(AeChn, &stStream);
            if (HI_SUCCESS != s32Ret)
            {
                utils_print("HI_MPI_AENC_ReleaseStream(%d), failed with %#x!\n", AeChn, s32Ret);
                break;
            }
        }
    }
    
    close(fd);

AIAENC_ERR1:
    for (i = 0; i < s32AencChnCnt; i++)
    {
        AeChn = i;
        AiChn = i;
    
        s32Ret |= SAMPLE_COMM_AUDIO_AencUnbindAi(AiDev, AiChn, AeChn);
        if (s32Ret != HI_SUCCESS)
        {
            utils_print("ret=%d\n",s32Ret);
        }
    }

AIAENC_ERR4:
    s32Ret |= SAMPLE_COMM_AUDIO_StopAenc(s32AencChnCnt);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
    }

AIAENC_ERR5:
    s32Ret |= SAMPLE_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
       utils_print("ret=%d\n",s32Ret);
    }

AIAENC_ERR6:
    HI_MPI_AENC_AacDeInit();

    utils_print("voice sample thread exit...\n");
    return NULL;
}

static void* play_mp3_cb(void* data)
{
    HI_S32      s32Ret;
    AO_CHN      AoChn = 0;
    ADEC_CHN    AdChn = 0;
    HI_S32      s32AoChnCnt;
    AIO_ATTR_S stAioAttr;

    HI_MPI_ADEC_AacInit();
    HI_MPI_ADEC_Mp3Init();

    AUDIO_DEV   AoDev        = SAMPLE_AUDIO_INNER_AO_DEV;
    stAioAttr.enSamplerate   = AUDIO_SAMPLE_RATE_16000;
    stAioAttr.enBitwidth     = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode     = AIO_MODE_I2S_MASTER;
    stAioAttr.enSoundmode    = AUDIO_SOUND_MODE_STEREO;
    stAioAttr.u32EXFlag      = 0;
    stAioAttr.u32FrmNum      = 30;
    stAioAttr.u32PtNumPerFrm = MP3_SAMPLES_PER_FRAME;
    stAioAttr.u32ChnCnt      = 2;
    stAioAttr.u32ClkSel      = 0;
    stAioAttr.enI2sType      = AIO_I2STYPE_INNERCODEC;

    //通道绑定解码器
    s32Ret = SAMPLE_COMM_AUDIO_StartAdec(AdChn, PT_MP3);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        goto ADECAO_ERR3;
    }

    //音频输出设置
    s32AoChnCnt = stAioAttr.u32ChnCnt;
    s32Ret = SAMPLE_COMM_AUDIO_StartAo(AoDev, s32AoChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        goto ADECAO_ERR2;
    }

    //设置codec
    s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        goto ADECAO_ERR1;
    }

    //Adec绑定Ao
    s32Ret = SAMPLE_COMM_AUDIO_AoBindAdec(AoDev, AoChn, AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        goto ADECAO_ERR1;
    }

    //设置音量
    //s32Ret = HI_MPI_AO_SetVolume(AoDev, -51);
    s32Ret = HI_MPI_AO_SetVolume(AoDev, -10);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        goto ADECAO_ERR1;
    }
 

    //解码
    int fd =  utils_open_fifo(MP3_FIFO, O_RDONLY);
    AUDIO_STREAM_S stAudioStream;
    HI_U32 u32Len = 640;
    HI_S32 s32AdecChn = 0;
    HI_U8* pu8AudioStream = NULL;

    pu8AudioStream = (HI_U8*)malloc(sizeof(HI_U8) * MAX_AUDIO_STREAM_LEN);
    if (NULL == pu8AudioStream)
    {
        utils_print("malloc failed!\n");
        return NULL;
    }
    while (g_play_status)
    {
        stAudioStream.pStream = pu8AudioStream;
        //读取mp3数据
        ssize_t read_count = read(fd, stAudioStream.pStream, u32Len);
        if (read_count > 0)
        {
            // utils_print("read %d bytes from my_mp3_fifo\n", (HI_U32)read_count);
            stAudioStream.u32Len = (HI_U32)read_count;
            s32Ret = HI_MPI_ADEC_SendStream(s32AdecChn, &stAudioStream, HI_TRUE);
            if (HI_SUCCESS != s32Ret)
            {
                utils_print("HI_MPI_ADEC_SendStream(%d) failed with %#x!\n", s32AdecChn, s32Ret);
                break;
            }
        }
        else
        {
            s32Ret = HI_MPI_ADEC_SendEndOfStream(s32AdecChn, HI_FALSE);
            if (HI_SUCCESS != s32Ret)
            {
                printf("HI_MPI_ADEC_SendEndOfStream failed!\n");
            }   
        }    
    }

    free(pu8AudioStream);
    pu8AudioStream = NULL;
    close(fd);

ADECAO_ERR0:
    s32Ret = SAMPLE_COMM_AUDIO_AoUnbindAdec(AoDev, AoChn, AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
    }

ADECAO_ERR1:
    s32Ret |= SAMPLE_COMM_AUDIO_StopAo(AoDev, s32AoChnCnt, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
    }
ADECAO_ERR2:
    s32Ret |= SAMPLE_COMM_AUDIO_StopAdec(AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
    }

ADECAO_ERR3:
    utils_print("mp3 play thread exit...\n");
    return NULL;

}

static void* sample_video_cb(void* data)
{
    BOOL init_result = FALSE;

    /*config vi*/
    init_result = start_vi();
    if(!init_result)
    {
        utils_print("start vi failed\n");
        return NULL;
    }

    init_result = start_vpss();
    if(!init_result)
    {
        utils_print("start vpss failed\n");
        stop_vi();
        return NULL;
    }

    init_result = start_venc();
    if(!init_result)
    {
        utils_print("start venc failed\n");
        stop_all_vpss();
        stop_vi();
        return NULL;
    }
    
    init_result = bind_vi_vpss();
    if(!init_result)
    {
        utils_print("bind vi and vpss failed\n");
        stop_all_venc();
        stop_all_vpss();
        stop_vi();
        return NULL;
    }

    init_result = bind_vpss_venc();
    if(!init_result)
    {
        utils_print("bind vi and vpss failed\n");
        stop_all_venc();
        unbind_vi_vpss();
        stop_all_vpss();
        stop_vi();
        return NULL;
    }

    init_result = rotate_picture();
    if(!init_result)
    {
        utils_print("rotate vi failed\n");
        goto EXIT;
    }

    board_get_stream_from_venc_chn();
    // board_get_venc_stream();
    // while (g_video_status)
    // {
    //     sleep(5);
    // }

EXIT:
    unbind_vpss_venc();
    stop_all_venc();
    unbind_vi_vpss();
    stop_all_vpss();
    stop_vi();
    utils_print("video sample thread exit...\n");
    return NULL;
}


BOOL board_mpp_init()
{
    BOOL init_result = FALSE;
    BOOL ret = TRUE;
    HI_S32 ret_val = HI_SUCCESS;
    int blk_size = 0;
    PIC_SIZE_E pic_size_type;
    SIZE_S pic_size;

    /* create data buffer */
    create_buffer(&g_video_buffer, 10*1024*1024);

    /* get picture size */
    ret_val = SAMPLE_COMM_VI_GetSizeBySensor(SENSOR0_TYPE, &pic_size_type);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("get picture size by sensor failed!\n");
        return FALSE;
    }

    ret_val = SAMPLE_COMM_SYS_GetPicSize(pic_size_type, &pic_size);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("get picture size failed!\n");
        return FALSE;
    }

    /* init vb */
    VB_CONFIG_S vb_conf;
    hi_memset(&vb_conf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    vb_conf.u32MaxPoolCnt = 2;

    blk_size = COMMON_GetPicBufferSize(pic_size.u32Width, pic_size.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    vb_conf.astCommPool[0].u64BlkSize  = blk_size;
    vb_conf.astCommPool[0].u32BlkCnt   = 8;
    
    blk_size = VI_GetRawBufferSize(pic_size.u32Width, pic_size.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    vb_conf.astCommPool[1].u64BlkSize  = blk_size;
    vb_conf.astCommPool[1].u32BlkCnt   = 8;    
    /* init system */
    ret_val = SAMPLE_COMM_SYS_Init(&vb_conf);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("system init failed with %d!\n", ret_val);
        return FALSE;
    }

    // /*config vi*/
    // init_result = start_vi();
    // if(!init_result)
    // {
    //     utils_print("start vi failed\n");
    //     return FALSE;
    // }

    // init_result = start_vpss();
    // if(!init_result)
    // {
    //     utils_print("start vpss failed\n");
    //     stop_vi();
    //     return FALSE;
    // }

    // init_result = start_venc();
    // if(!init_result)
    // {
    //     utils_print("start venc failed\n");
    //     stop_all_vpss();
    //     stop_vi();
    //     return FALSE;
    // }
    
    // init_result = bind_vi_vpss();
    // if(!init_result)
    // {
    //     utils_print("bind vi and vpss failed\n");
    //     stop_all_venc();
    //     stop_all_vpss();
    //     stop_vi();
    //     return FALSE;
    // }

    // init_result = bind_vpss_venc();
    // if(!init_result)
    // {
    //     utils_print("bind vi and vpss failed\n");
    //     stop_all_venc();
    //     unbind_vi_vpss();
    //     stop_all_vpss();
    //     stop_vi();
    //     return FALSE;
    // }

    // init_result = rotate_picture();
    // if(!init_result)
    // {
    //     utils_print("rotate vi failed\n");
    //     unbind_vpss_venc();
    //     stop_all_venc();
    //     unbind_vi_vpss();
    //     stop_all_vpss();
    //     stop_vi();
    //     return FALSE;
    // }

    // board_get_stream_from_venc_chn();

    return ret;       
}

void board_mpp_deinit()
{
    unbind_vpss_venc();
    stop_all_venc();
    unbind_vi_vpss();
    stop_all_vpss();
    stop_vi();

    /* exit system */
    SAMPLE_COMM_SYS_Exit();

    /* destroy data buffer */
    destroy_buffer(&g_video_buffer);
}

void board_get_yuv_from_vpss_chn(char **yuv_buf)
{
    HI_S32 ret_val;
    VPSS_GRP vpss_grp[2] = {0, 1};
    HI_S32 time_out = 2000;
    VPSS_CHN vpss_chn = 2;
    VIDEO_FRAME_INFO_S  video_frame;

    ret_val = HI_MPI_VPSS_GetChnFrame(vpss_grp[1], vpss_chn, &video_frame, time_out);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("vpss get frame failed, ret:0x%08x\n", ret_val);
        return;
    }

    /* to change to yuv picture */
    sample_yuv_8bit_dump(&video_frame.stVFrame, (void *)yuv_buf);

    ret_val = HI_MPI_VPSS_ReleaseChnFrame(vpss_grp[1], vpss_chn, &video_frame);
    if ( HI_SUCCESS != ret_val )
    {
        SAMPLE_PRT("vpss release frame failed, ret:0x%08x\n", ret_val);
        return;
    }  
}


#if 1
void board_get_venc_stream()
{
    VENC_CHN venc_chn = -1;
    VENC_CHN_ATTR_S venc_chn_attrs;
    VENC_STREAM_BUF_INFO_S steam_buff_infos[VENC_MAX_CHN_NUM];
    VENC_CHN_STATUS_S chn_stat;
    VENC_STREAM_S venc_stream;
    HI_S32 venc_fd[VENC_MAX_CHN_NUM] = {0};
    HI_S32 max_fd = 0;
    HI_S32 ret_val = HI_FAILURE;
    int total_chn = 4;  //venc channel count
    fd_set read_fds;
    struct timeval timout_val;
    HI_U32 picture_cnt[] = {0};

    for(int i = 0; i < total_chn; i++)
    {
        venc_chn = i;
        ret_val = HI_MPI_VENC_GetChnAttr(venc_chn, &venc_chn_attrs);
        if (ret_val != HI_SUCCESS)
        {
            utils_print("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", venc_chn, ret_val);
            return;
        }

        venc_fd[i] = HI_MPI_VENC_GetFd(i);
        if (venc_fd[i] < 0)
        {
            utils_print("HI_MPI_VENC_GetFd failed with %#x!\n", venc_fd[i]);
            return;
        }
        if (max_fd <= venc_fd[i])
        {
            max_fd = venc_fd[i];
        }

        ret_val = HI_MPI_VENC_GetStreamBufInfo (i, &steam_buff_infos[i]);
        if (HI_SUCCESS != ret_val)
        {
            utils_print("HI_MPI_VENC_GetStreamBufInfo failed with %#x!\n", ret_val);
            return;
        }
    }

    HI_PDT_Init();


    while (g_video_status)
    {
        FD_ZERO(&read_fds);
        int i = 0;
        for (i = 0; i < total_chn; i++)
        {
            FD_SET(venc_fd[i], &read_fds);
        }

        timout_val.tv_sec  = 2;
        timout_val.tv_usec = 0;
        ret_val = select(max_fd + 1, &read_fds, NULL, NULL, &timout_val);
        if (ret_val < 0)
        {
            utils_print("select failed!\n");
            break;
        }
        else if (ret_val == 0)
        {
            utils_print("get venc stream time out, exit thread\n");
            continue;
        }
        else
        {
            for (i = 0; i < total_chn; i++)
            {
                if (FD_ISSET(venc_fd[i], &read_fds))
                {
                    /*******************************************************
                     step 2.1 : query how many packs in one-frame stream.
                    *******************************************************/
                    memset(&venc_stream, 0, sizeof(venc_stream));
                    ret_val = HI_MPI_VENC_QueryStatus(i, &chn_stat);
                    if (HI_SUCCESS != ret_val)
                    {
                        utils_print("HI_MPI_VENC_QueryStatus chn[%d] failed with %#x!\n", i, ret_val);
                        break;
                    }

                    /*******************************************************
                    step 2.2 :suggest to check both u32CurPacks and u32LeftStreamFrames at the same time,for example:
                     if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
                     {
                        SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                        continue;
                     }
                    *******************************************************/
                    if(0 == chn_stat.u32CurPacks)
                    {
                          utils_print("NOTE: Current  frame is NULL!\n");
                          continue;
                    }
                    /*******************************************************
                     step 2.3 : malloc corresponding number of pack nodes.
                    *******************************************************/
                    venc_stream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * chn_stat.u32CurPacks);
                    if (NULL == venc_stream.pstPack)
                    {
                        utils_print("malloc stream pack failed!\n");
                        break;
                    }

                    /*******************************************************
                     step 2.4 : call mpi to get one-frame stream
                    *******************************************************/
                    venc_stream.u32PackCount = chn_stat.u32CurPacks;
                    ret_val = HI_MPI_VENC_GetStream(i, &venc_stream, HI_TRUE);
                    if (HI_SUCCESS != ret_val)
                    {
                        free(venc_stream.pstPack);
                        venc_stream.pstPack = NULL;
                        utils_print("HI_MPI_VENC_GetStream failed with %#x!\n", ret_val);
                        break;
                    }
                    
                    if (i == 0 || i == 1)
                    {	
                    	//264保存成mp4
                        //HI_PDT_WriteVideo(i,  &venc_stream);
                        /* write into a circle buffer, to save 10 seconds video */
                        
                    }     
                    else if (i == 2 || i == 3)
                    {
                    	//jpeg存文件
                    	SnapSave(i, &venc_stream);
                    	sem_post(&sem_snap[i - 2]);
                    }	 

                    if (HI_SUCCESS != ret_val)
                    {
                        free(venc_stream.pstPack);
                        venc_stream.pstPack = NULL;
                        utils_print("save stream failed!\n");
                        break;
                    }
                    /*******************************************************
                     step 2.6 : release stream
                     *******************************************************/
                    ret_val = HI_MPI_VENC_ReleaseStream(i, &venc_stream);
                    if (HI_SUCCESS != ret_val)
                    {
                        utils_print("HI_MPI_VENC_ReleaseStream failed!\n");
                        free(venc_stream.pstPack);
                        venc_stream.pstPack = NULL;
                        break;
                    }

                    /*******************************************************
                     step 2.7 : free pack nodes
                    *******************************************************/
                    free(venc_stream.pstPack);
                    venc_stream.pstPack = NULL;
                    picture_cnt[i]++;
                }
            }
        }
    }
    
    HI_PDT_Exit();
    
    return;
}
#endif

/* default venc chn 0 */
void board_get_stream_from_venc_chn()
{
    VENC_CHN venc_chn = 0;

    VENC_CHN_ATTR_S venc_chn_attrs;
    VENC_STREAM_BUF_INFO_S steam_buff_infos;
    VENC_CHN_STATUS_S chn_stat;
    VENC_STREAM_S venc_stream;
    HI_S32 venc_fd = -1;
    HI_S32 max_fd = 0;
    HI_S32 ret_val = HI_FAILURE;
    fd_set read_fds;
    struct timeval timout_val;

    ret_val = HI_MPI_VENC_GetChnAttr(venc_chn, &venc_chn_attrs);
    if (ret_val != HI_SUCCESS)
    {
        utils_print("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", venc_chn, ret_val);
        return;
    }

    venc_fd = HI_MPI_VENC_GetFd(venc_chn);
    if (venc_fd < 0)
    {
        utils_print("HI_MPI_VENC_GetFd failed with %#x!\n", venc_fd);
        return;
    }

    ret_val = HI_MPI_VENC_GetStreamBufInfo (venc_chn, &steam_buff_infos);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("HI_MPI_VENC_GetStreamBufInfo failed with %#x!\n", ret_val);
        return;
    }

    while (g_video_status)
    {        
        FD_ZERO(&read_fds);
        FD_SET(venc_fd, &read_fds);

        timout_val.tv_sec  = 2;
        timout_val.tv_usec = 0;
        ret_val = select(venc_fd + 1, &read_fds, NULL, NULL, &timout_val);
        if (ret_val < 0)
        {
            utils_print("select failed!\n");
            continue;
        }
        else if (ret_val == 0)
        {
            utils_print("get venc stream time out, exit thread\n");
            continue;
        }
        else
        {
            if (FD_ISSET(venc_fd, &read_fds))
            {
                /*******************************************************
                 step 2.1 : query how many packs in one-frame stream.
                *******************************************************/
                memset(&venc_stream, 0, sizeof(venc_stream));
                ret_val = HI_MPI_VENC_QueryStatus(venc_chn, &chn_stat);
                if (HI_SUCCESS != ret_val)
                {
                    utils_print("HI_MPI_VENC_QueryStatus chn[%d] failed with %#x!\n", venc_chn, ret_val);
                    break;
                }

                /*******************************************************
                step 2.2 :suggest to check both u32CurPacks and u32LeftStreamFrames at the same time,for example:
                    if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
                    {
                    SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                    continue;
                    }
                *******************************************************/
                if(0 == chn_stat.u32CurPacks)
                {
                    utils_print("NOTE: Current  frame is NULL!\n");
                    continue;
                }
                /*******************************************************
                 step 2.3 : malloc corresponding number of pack nodes.
                *******************************************************/
                venc_stream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * chn_stat.u32CurPacks);
                if (NULL == venc_stream.pstPack)
                {
                    utils_print("malloc stream pack failed!\n");
                    break;
                }

                /*******************************************************
                 step 2.4 : call mpi to get one-frame stream
                *******************************************************/
                venc_stream.u32PackCount = chn_stat.u32CurPacks;
                ret_val = HI_MPI_VENC_GetStream(venc_chn, &venc_stream, HI_TRUE);
                if (HI_SUCCESS != ret_val)
                {
                    free(venc_stream.pstPack);
                    venc_stream.pstPack = NULL;
                    utils_print("HI_MPI_VENC_GetStream failed with %#x!\n", ret_val);
                    break;
                }
                
                /* to save mp4 */
                if(g_start_record)
                {   
                    HI_PDT_WriteVideo(venc_chn,  &venc_stream);  
                }

                if (HI_SUCCESS != ret_val)
                {
                    free(venc_stream.pstPack);
                    venc_stream.pstPack = NULL;
                    utils_print("save stream failed!\n");
                    break;
                }
                /*******************************************************
                 step 2.6 : release stream
                    *******************************************************/
                ret_val = HI_MPI_VENC_ReleaseStream(venc_chn, &venc_stream);
                if (HI_SUCCESS != ret_val)
                {
                    utils_print("HI_MPI_VENC_ReleaseStream failed!\n");
                    free(venc_stream.pstPack);
                    venc_stream.pstPack = NULL;
                    break;
                }

                /*******************************************************
                 step 2.7 : free pack nodes
                *******************************************************/
                free(venc_stream.pstPack);
                venc_stream.pstPack = NULL;
            }    
        }
    }

    return;
}

/* default venc chn 2 */
void board_get_snap_from_venc_chn()
{
    VENC_CHN venc_chn = 3;

    struct timeval timeout;
    fd_set read_fds;
    HI_S32 venc_fd;
    VENC_CHN_STATUS_S venc_chn_stat;
    VENC_STREAM_S stream;
    HI_S32 ret;
    HI_U32 idx;
    VENC_RECV_PIC_PARAM_S  snap_recv_param;

    snap_recv_param.s32RecvPicNum = 1;
    ret = HI_MPI_VENC_StartRecvFrame(venc_chn, &snap_recv_param);
    if (HI_SUCCESS != ret)
    {
        utils_print("HI_MPI_VENC_StartRecvPic faild with%#x!\n", ret);
        return HI_FAILURE;
    }

    venc_fd = HI_MPI_VENC_GetFd(venc_chn);
    if (venc_fd < 0)
    {
        utils_print("HI_MPI_VENC_GetFd faild with%#x!\n", venc_fd);
        return;
    }

    FD_ZERO(&read_fds);
    FD_SET(venc_fd, &read_fds);
    timeout.tv_sec  = 10;
    timeout.tv_usec = 0;
    ret = select(venc_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret < 0)
    {
        utils_print("snap select failed!\n");
        return;
    }
    else if (0 == ret)
    {
        utils_print("snap time out!\n");
        return;
    }
    else
    {
        if (FD_ISSET(venc_fd, &read_fds))
        {
            ret = HI_MPI_VENC_QueryStatus(venc_chn, &venc_chn_stat);
            if (ret != HI_SUCCESS)
            {
                utils_print("HI_MPI_VENC_QueryStatus failed with %#x!\n", ret);
                return;
            }

            if (0 == venc_chn_stat.u32CurPacks)
            {
                utils_print("NOTE: Current  frame is NULL!\n");
                return;
            }
            stream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * venc_chn_stat.u32CurPacks);
            if (NULL == stream.pstPack)
            {
                utils_print("malloc memory failed!\n");
                return;
            }
            stream.u32PackCount = venc_chn_stat.u32CurPacks;
            ret = HI_MPI_VENC_GetStream(venc_chn, &stream, -1);
            if (HI_SUCCESS != ret)
            {
                utils_print("HI_MPI_VENC_GetStream failed with %#x!\n", ret);

                free(stream.pstPack);
                stream.pstPack = NULL;
                return;
            }

            char jpg_file[FILE_NAME_LEN]    = {0};
            struct tm * tm;
            time_t now = time(0);
            tm = localtime(&now);
            snprintf(jpg_file, 128, "/user/%04d%02d%02d%02d%02d%02d.jpg",
                                    tm->tm_year + 1900,
                                    tm->tm_mon + 1,
                                    tm->tm_mday,
                                    tm->tm_hour,
                                    tm->tm_min,
                                    tm->tm_sec);
	         
            FILE* pFile = fopen(jpg_file, "wb");
            if (pFile == NULL)
            {
                utils_print("open file err\n");
                free(stream.pstPack);
                stream.pstPack = NULL;
                return;
            }
            for (int i = 0; i < stream.u32PackCount; i++)
            {
                fwrite(stream.pstPack[i].pu8Addr + stream.pstPack[i].u32Offset,
                        stream.pstPack[i].u32Len - stream.pstPack[i].u32Offset, 1, pFile);

                fflush(pFile);
            }
            fclose(pFile);
            
            ret = HI_MPI_VENC_ReleaseStream(venc_chn, &stream);
            if (HI_SUCCESS != ret)
            {
                utils_print("HI_MPI_VENC_ReleaseStream failed with %#x!\n", ret);

                free(stream.pstPack);
                stream.pstPack = NULL;
                return;
            }

            free(stream.pstPack);
            stream.pstPack = NULL;
        }
    }

    ret = HI_MPI_VENC_StopRecvFrame(venc_chn);
    if (ret != HI_SUCCESS)
    {
        utils_print("HI_MPI_VENC_StopRecvPic failed with %#x!\n",  ret);
        return;
    }

    return;
}

pthread_t start_sample_video()
{
    pthread_t video_tid;
    pthread_create(&video_tid, NULL, sample_video_cb, NULL);
    return video_tid;
}

pthread_t start_play_mp3()
{
    pthread_t play_id;
    pthread_create(&play_id, NULL, play_mp3_cb, NULL);
    return play_id;
}

pthread_t start_sample_voice()
{
    pthread_t voice_tid;
    pthread_create(&voice_tid, NULL, sample_pcm_cb, NULL);
    return voice_tid;
}


void start_video_recording()
{
    board_create_mp4_file();
    g_start_record = TRUE;
}

void stop_video_recording()
{
    g_start_record = FALSE;
    board_close_mp4_file();
}

void delete_posture_video()
{
    g_start_record = FALSE;
    board_delete_current_mp4_file();
}

