#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/prctl.h>

#include <sample_comm.h>
#include <acodec.h>
#include <audio_mp3_adp.h>

#include "defines.h"
#include "command.h"
#include "board_media.h"
#include "board_mpp.h"



/*********************MPP各模块对应关系**********************************
 VI     VI_PIPE     VI_CHN      VPSS_GRP      VPSS_CHN      VENC_CHN
----------------------------------------------------------------------
0       0           0           0               1 (YUV)         0(Video)
                                                2 (ENC)         2(Snap) 
1       2           0           1               1               1(Video)
                                                2               3(Snap)
*********************************************************************/


static SAMPLE_VI_CONFIG_S g_vi_configs;

static BOOL g_sample_pcm_status = TRUE;  //sample pcm data to fifo
static BOOL g_play_mp3_status = TRUE;   //get mp3 data from fifo
static BOOL g_enc_stream_status = TRUE;  //get stream from venc

static BOOL g_start_record = FALSE;
static BOOL g_recording = FALSE;
static BOOL g_stop_record = FALSE;

static char s_file_name[255] = {0};

/* fifo */
static BOOL create_fifo(const char* name)
{
    char* fifo_name = (char *)name;
    if(NULL == fifo_name)
    {
        return FALSE;
    }
    if (access(fifo_name, F_OK) == -1)
    {
        int res = mkfifo(fifo_name, 0666);
        if(res != 0)
        {
            utils_print("could not create fifo %s\n", fifo_name);
            return FALSE;
        }
    }

    return TRUE;
}

static void delete_fifo(const char* name)
{
    char* fifo_name = (char *)name;
    if(NULL == fifo_name)
    {
        return;
    }
    unlink(name);
}

static int open_pcm_fifo()
{
    return open(PCM_FIFO, O_WRONLY);
}

BOOL create_pcm_fifo()
{
    return create_fifo(PCM_FIFO);
}

void delete_pcm_fifo()
{
    delete_fifo(PCM_FIFO);
}

static int open_mp3_fifo()
{
    return open(MP3_FIFO, O_RDONLY);
}

BOOL create_mp3_fifo()
{
    return create_fifo(MP3_FIFO);
}

void delete_mp3_fifo()
{
    delete_fifo(MP3_FIFO);
}

/* mpp */
static void stop_vi()
{
    SAMPLE_COMM_VI_StopVi(&g_vi_configs);
}

static void stop_all_vpss()
{
    HI_BOOL chn_enable[VPSS_MAX_PHY_CHN_NUM] = { HI_FALSE, HI_TRUE, HI_TRUE };
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
/* sensor0: sony imx307, use library with imx327*/
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

static BOOL start_vpss(int width, int height)
{
    BOOL ret = TRUE;
    
    HI_S32             ret_val = HI_FAILURE;
    VPSS_GRP           vpss_grp[2]        = {0, 1};
    VPSS_GRP_ATTR_S    vpss_grp_attrs     = {0};
    VPSS_CHN           vpss_chn0  = VPSS_CHN1, vpss_chn1 = VPSS_CHN2;
    HI_BOOL            chn_enable[VPSS_MAX_PHY_CHN_NUM] = {HI_FALSE, HI_TRUE, HI_TRUE};
    VPSS_CHN_ATTR_S    vpss_chn_attrs[VPSS_MAX_PHY_CHN_NUM] = {0};
    PIC_SIZE_E enPicSize;
    SIZE_S stSize;

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
    vpss_chn_attrs[vpss_chn0].u32Width                    = width;
    vpss_chn_attrs[vpss_chn0].u32Height                   = height;
    vpss_chn_attrs[vpss_chn0].enChnMode                   = VPSS_CHN_MODE_USER;
    vpss_chn_attrs[vpss_chn0].enCompressMode              = COMPRESS_MODE_NONE;
    vpss_chn_attrs[vpss_chn0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    vpss_chn_attrs[vpss_chn0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    vpss_chn_attrs[vpss_chn0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vpss_chn_attrs[vpss_chn0].stFrameRate.s32SrcFrameRate = 30;
    vpss_chn_attrs[vpss_chn0].stFrameRate.s32DstFrameRate = 25;
    vpss_chn_attrs[vpss_chn0].u32Depth                    = 0;
    vpss_chn_attrs[vpss_chn0].bMirror                     = HI_FALSE;
    vpss_chn_attrs[vpss_chn0].bFlip                       = HI_FALSE;
    vpss_chn_attrs[vpss_chn0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;

    chn_enable[vpss_chn1] = HI_TRUE;
    vpss_chn_attrs[vpss_chn1].u32Width                    = width;
    vpss_chn_attrs[vpss_chn1].u32Height                   = height;
    vpss_chn_attrs[vpss_chn1].enChnMode                   = VPSS_CHN_MODE_USER;
    vpss_chn_attrs[vpss_chn1].enCompressMode              = COMPRESS_MODE_NONE;
    vpss_chn_attrs[vpss_chn1].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    vpss_chn_attrs[vpss_chn1].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    vpss_chn_attrs[vpss_chn1].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vpss_chn_attrs[vpss_chn1].stFrameRate.s32SrcFrameRate = 30;
    vpss_chn_attrs[vpss_chn1].stFrameRate.s32DstFrameRate = 25;
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

static BOOL start_venc(int width, int height)
{
    BOOL ret = TRUE;
    HI_S32 ret_val = HI_FAILURE;
    VENC_GOP_MODE_E gop_mode  = VENC_GOPMODE_NORMALP;
    VENC_GOP_ATTR_S gop_attrs = {0};
    VENC_CHN venc_chn[4] = {0, 1, 2, 3};
    SAMPLE_RC_E rc_mode = SAMPLE_RC_VBR;
    HI_BOOL rcn_ref_shared_buf = HI_TRUE;
    int gop_value = 0;
    
    SIZE_S picture_size;
    picture_size.u32Width = height;
    picture_size.u32Height = width;

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

BOOL rotate_picture()
{
    HI_S32 ret_val = HI_FAILURE;
    ROTATION_E rotate =  ROTATION_270;
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

BOOL start_audio_system()
{
    HI_S32 i, j, s32Ret;
    AI_CHN      AiChn = 0;
    AO_CHN      AoChn = 0;
    ADEC_CHN    AdChn = 0;
    HI_S32      s32AiChnCnt;
    HI_S32      s32AoChnCnt;
    AIO_ATTR_S              stAioAttr;
    AI_RECORDVQE_CONFIG_S   stAiVqeRecordAttr;
    AI_TALKVQE_CONFIG_S     stAiVqeTalkAttr;
    HI_VOID     *pAiVqeAttr = NULL;

    AUDIO_DEV   AiDev = SAMPLE_AUDIO_INNER_AI_DEV;
    AUDIO_DEV   AoDev = SAMPLE_AUDIO_INNER_AO_DEV;
    stAioAttr.enSamplerate   = AUDIO_SAMPLE_RATE_16000;
    stAioAttr.enBitwidth     = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode     = AIO_MODE_I2S_MASTER;
    stAioAttr.enSoundmode    = AUDIO_SOUND_MODE_MONO;
    stAioAttr.u32EXFlag      = 0;
    stAioAttr.u32FrmNum      = 30;
    stAioAttr.u32PtNumPerFrm = SAMPLE_AUDIO_PTNUMPERFRM; //MP3_SAMPLES_PER_FRAME
    stAioAttr.u32ChnCnt      = 2;
    stAioAttr.u32ClkSel      = 0;
    stAioAttr.enI2sType      = AIO_I2STYPE_INNERCODEC;

    memset(&stAiVqeTalkAttr, 0, sizeof(AI_TALKVQE_CONFIG_S));
    stAiVqeTalkAttr.enWorkstate = VQE_WORKSTATE_COMMON;
    stAiVqeTalkAttr.s32FrameSample = SAMPLE_AUDIO_PTNUMPERFRM;
    stAiVqeTalkAttr.s32WorkSampleRate = AUDIO_SAMPLE_RATE_16000;
    stAiVqeTalkAttr.stAecCfg.bUsrMode = HI_FALSE;
    stAiVqeTalkAttr.stAnrCfg.bUsrMode = HI_FALSE;
    stAiVqeTalkAttr.stHpfCfg.bUsrMode = HI_FALSE;
    stAiVqeTalkAttr.u32OpenMask = AI_TALKVQE_MASK_AEC | AI_TALKVQE_MASK_ANR | AI_TALKVQE_MASK_HPF;
    pAiVqeAttr = (HI_VOID *)&stAiVqeTalkAttr;

    s32AiChnCnt = stAioAttr.u32ChnCnt;    
    s32Ret = SAMPLE_COMM_AUDIO_StartAi(AiDev, s32AiChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE, pAiVqeAttr, 2);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        return FALSE;
    }

    s32AoChnCnt = stAioAttr.u32ChnCnt;
    s32Ret = SAMPLE_COMM_AUDIO_StartAo(AoDev, s32AoChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        SAMPLE_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_TRUE);
        return FALSE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        SAMPLE_COMM_AUDIO_StopAo(AoDev, s32AoChnCnt, HI_FALSE);
        SAMPLE_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_TRUE);
        return FALSE;
    }

    //mp3解码
    HI_MPI_ADEC_Mp3Init();

    s32Ret = SAMPLE_COMM_AUDIO_StartAdec(AdChn, PT_MP3);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        SAMPLE_COMM_AUDIO_StopAo(AoDev, s32AoChnCnt, HI_FALSE);
        SAMPLE_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_TRUE);
        return FALSE;
    }

    //Adec绑定Ao
    s32Ret = SAMPLE_COMM_AUDIO_AoBindAdec(AoDev, AoChn, AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        SAMPLE_COMM_AUDIO_StopAdec(AdChn);
        SAMPLE_COMM_AUDIO_StopAo(AoDev, s32AoChnCnt, HI_FALSE);
        SAMPLE_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_TRUE);
        return FALSE;
    }

    //设置音量
    // s32Ret = HI_MPI_AO_SetVolume(AoDev, -30);
    // if (s32Ret != HI_SUCCESS)
    // {
    //     utils_print("ret=%d\n",s32Ret);
    //     SAMPLE_COMM_AUDIO_AoUnbindAdec(AoDev, AoChn, AdChn);
    //     SAMPLE_COMM_AUDIO_StopAdec(AdChn);
    //     SAMPLE_COMM_AUDIO_StopAo(AoDev, s32AoChnCnt, HI_FALSE);
    //     SAMPLE_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_TRUE);
    //     return FALSE;
    // }
    
    return TRUE;
}

void stop_audio_system()
{
    AUDIO_DEV   AiDev = SAMPLE_AUDIO_INNER_AI_DEV;
    AUDIO_DEV   AoDev = SAMPLE_AUDIO_INNER_AO_DEV;
    AI_CHN      AiChn = 0;
    AO_CHN      AoChn = 0;
    ADEC_CHN    AdChn = 0;
    HI_S32      s32AiChnCnt = 2;
    HI_S32      s32AoChnCnt = 2;

    SAMPLE_COMM_AUDIO_AoUnbindAdec(AoDev, AoChn, AdChn);
    SAMPLE_COMM_AUDIO_StopAdec(AdChn);
    SAMPLE_COMM_AUDIO_StopAo(AoDev, s32AoChnCnt, HI_FALSE);
    SAMPLE_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_TRUE);

    return;
}

BOOL start_video_system(int width, int height)
{
    // stop_video_system();

    BOOL init_result = FALSE;

    init_result = start_vpss(width, height);
    if(!init_result)
    {
        utils_print("start vpss failed\n");
        return FALSE;
    }

    init_result = bind_vi_vpss();
    if(!init_result)
    {
        utils_print("bind vi and vpss failed\n");
        stop_all_vpss();
        return FALSE;
    }

    init_result = start_venc(width, height);
    if(!init_result)
    {
        utils_print("start venc failed\n");
        unbind_vi_vpss();
        stop_all_vpss();
        return FALSE;
    }
    
    init_result = bind_vpss_venc();
    if(!init_result)
    {
        utils_print("bind vi and vpss failed\n");
        stop_all_venc();
        unbind_vi_vpss();
        stop_all_vpss();
        return FALSE;
    }

    init_result = rotate_picture();
    if(!init_result)
    {
        utils_print("rotate vi failed\n");
        unbind_vpss_venc();
        stop_all_venc();
        unbind_vi_vpss();
        stop_all_vpss();
        return FALSE;
    }

    return init_result;
}

void stop_video_system()
{
    unbind_vpss_venc();
    stop_all_venc();
    unbind_vi_vpss();
    stop_all_vpss();
}

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

static int sample_pcm_32bit_dump(const AUDIO_FRAME_S *audio_frame, const AEC_FRAME_S *aec_frame, void** pOutbuf)
{
    short* tmp_short_mic_ptr = NULL;
    short* tmp_short_reff_ptr = NULL;

    if(audio_frame == NULL || aec_frame == NULL)
    {
        utils_print("invalid mic buffer or invalid reff buffer\n");
        return -1;
    }

    int reff_buf_len = aec_frame->stRefFrame.u32Len;
    int mic_buf_len = audio_frame->u32Len;

    char* user_addr[2] = {NULL, NULL};
    unsigned long long int mic_phy_addr;
    mic_phy_addr = audio_frame->u64PhyAddr[0];    
    user_addr[0] = (char*)HI_MPI_SYS_Mmap(mic_phy_addr, mic_buf_len);
    if (NULL == user_addr[0])
    {
        utils_print("HI_MPI_SYS_Mmap mic pyh_addr error\n");
        return -1;
    }
    tmp_short_mic_ptr = (short*)user_addr[0];

    unsigned long long int reff_phy_addr;
    reff_phy_addr = aec_frame->stRefFrame.u64PhyAddr[0];
    user_addr[1] = (char*)HI_MPI_SYS_Mmap(reff_phy_addr, reff_buf_len);
    if (NULL == user_addr[1])
    {
        utils_print("HI_MPI_SYS_Mmap reff pyh_addr error\n");
        return -1;
    }
    tmp_short_reff_ptr = (short*)user_addr[1];


    *pOutbuf = utils_malloc(reff_buf_len*2 + mic_buf_len*2);
    memset(*pOutbuf, 0, (reff_buf_len*2 + mic_buf_len*2));

    int tmp_val = 0;
    int idx = 0;
    for(idx = 0; idx < audio_frame->u32Len/2; idx++)
    {
        tmp_val = *tmp_short_mic_ptr++;
        tmp_val = (tmp_val << 16) & 0xFFFF0000;
        memcpy(*pOutbuf + idx * 2 * sizeof(int), &tmp_val, sizeof(int));
        tmp_val = 0;

        tmp_val = *tmp_short_reff_ptr++;
        tmp_val = (tmp_val << 16) & 0xFFFF0000;
        memcpy(*pOutbuf + (2*idx+1) * sizeof(int), &tmp_val, sizeof(int));
        tmp_val = 0;
    }

    HI_MPI_SYS_Munmap(user_addr[0], mic_buf_len);
    user_addr[0] = NULL;
    HI_MPI_SYS_Munmap(user_addr[1], reff_buf_len);
    user_addr[1] = NULL;

    return (reff_buf_len*2 + mic_buf_len*2);

}

static void* sample_pcm_cb(void *data)
{
    HI_S32 s32Ret;
     
    AUDIO_DEV   AiDev = SAMPLE_AUDIO_INNER_AI_DEV;
    AI_CHN      AiChn = 0;
    AI_CHN_PARAM_S stAiChnPara;

    /* open fifo */
    int fd = open_pcm_fifo();
    if (-1 == fd)
    {
        utils_print("open pcm fifo error\n");
        return NULL;
    }

    /* 2020-08-29 added */
    // s32Ret = HI_MPI_AI_GetChnParam(AiDev, AiChn, &stAiChnPara);
    // if (HI_SUCCESS != s32Ret)
    // {
    //     utils_print("%s: Get ai chn param failed\n");
    //     goto ERROR_EXIT;
    // }
    // stAiChnPara.u32UsrFrmDepth = 30;
    // s32Ret = HI_MPI_AI_SetChnParam(AiDev, AiChn, &stAiChnPara);
    // if (HI_SUCCESS != s32Ret)
    // {
    //     utils_print("%s: set ai chn param failed\n");
    //     goto ERROR_EXIT;
    // }
    /*end added*/

    /* get frame after AEC */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    HI_S32 AiFd;
    AiFd = HI_MPI_AI_GetFd(AiDev, AiChn);
    FD_SET(AiFd, &read_fds);

    struct timeval TimeoutVal;
    AEC_FRAME_S   stAecFrm;
    AUDIO_FRAME_S stFrame;

    prctl(PR_SET_NAME, "mpp_sample_voice");

    while(g_sample_pcm_status)
    {
        TimeoutVal.tv_sec = 1;
        TimeoutVal.tv_usec = 0;
        FD_ZERO(&read_fds);
        FD_SET(AiFd, &read_fds);
        s32Ret = select(AiFd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if(s32Ret < 0)
        {
            utils_print("aenc stream fd error\n");
            continue;
        }
        else if (s32Ret == 0)
        {
            utils_print("get aenc stream select time out\n");
            continue;
        }
        char* out_buff = NULL;
        int size = 0;
        if(FD_ISSET(AiFd, &read_fds))
        {
            /* get frame from ai chn */
            memset(&stAecFrm, 0, sizeof(AEC_FRAME_S));
            s32Ret = HI_MPI_AI_GetFrame(AiDev, AiChn, &stFrame, &stAecFrm, HI_FALSE);
            if (HI_SUCCESS != s32Ret )
            {
                utils_print("HI_MPI_AI_GetFrame(%d, %d), failed with %#x!\n",AiDev, AiChn, s32Ret);
                continue;
            }

            if (stFrame.u32Len != stAecFrm.stRefFrame.u32Len)
            {
                utils_print("mic frame and reff frame not equal\n");
                HI_MPI_AI_ReleaseFrame(AiDev, AiChn, &stFrame, &stAecFrm);
                continue;
            }

            size = sample_pcm_32bit_dump(&stFrame, &stAecFrm, &out_buff);
            /* finally you must release the stream */
            s32Ret = HI_MPI_AI_ReleaseFrame(AiDev, AiChn, &stFrame, &stAecFrm);
            if (HI_SUCCESS != s32Ret )
            {
                utils_print("HI_MPI_AI_ReleaseFrame(%d, %d), failed with %#x!\n", AiDev, AiChn, s32Ret);
                continue;
            }
            
            if (out_buff == NULL)
            {
                utils_print("get pcm data NULL\n");
                continue;
            }  
        }
        int write_count = write(fd, out_buff, size);
        utils_free(out_buff);
    }

ERROR_EXIT:    
    close(fd);
   
    utils_print("voice sample thread exit...\n");
    return NULL;
}

static void* play_mp3_cb(void* data)
{
    HI_S32      s32Ret;

    int fd =  open_mp3_fifo();

    //解码
    AUDIO_STREAM_S stAudioStream;
    HI_U32 u32Len = 640;
    HI_S32 s32AdecChn = 0;
    HI_U8* pu8AudioStream = NULL;

    pu8AudioStream = (HI_U8*)utils_malloc(sizeof(HI_U8) * MAX_AUDIO_STREAM_LEN);
    if (NULL == pu8AudioStream)
    {
        utils_print("malloc failed!\n");
        return NULL;
    }

    prctl(PR_SET_NAME, "mpp_play_mp3");

    while (g_play_mp3_status)
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
                continue;
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

    utils_free(pu8AudioStream);
    pu8AudioStream = NULL;
    close(fd);

    utils_print("mp3 play thread exit...\n");
    return NULL;

}

static void* sample_video_cb(void* data)
{
    video_ratio_t *ptmp = NULL;
    ptmp = (video_ratio_t*)data;
    if(ptmp == NULL)
    {
        return NULL;
    }

    board_get_stream_from_venc_chn(ptmp->width, ptmp->height);
    return NULL;
}

BOOL init_mpp()
{
    BOOL init_result = FALSE;
    HI_S32 ret_val = HI_SUCCESS;
    int blk_size = 0;
    PIC_SIZE_E pic_size_type;
    SIZE_S pic_size;

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
    vb_conf.u32MaxPoolCnt = 4;

    blk_size = COMMON_GetPicBufferSize(pic_size.u32Width, pic_size.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    vb_conf.astCommPool[0].u64BlkSize  = blk_size;
    vb_conf.astCommPool[0].u32BlkCnt   = 8;
    
    blk_size = VI_GetRawBufferSize(pic_size.u32Width, pic_size.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    vb_conf.astCommPool[1].u64BlkSize  = blk_size;
    vb_conf.astCommPool[1].u32BlkCnt   = 8;    
    
    blk_size = COMMON_GetPicBufferSize(pic_size.u32Width, pic_size.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    vb_conf.astCommPool[2].u64BlkSize  = blk_size;
    vb_conf.astCommPool[2].u32BlkCnt   = 8;
    
    blk_size = VI_GetRawBufferSize(pic_size.u32Width, pic_size.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    vb_conf.astCommPool[3].u64BlkSize  = blk_size;
    vb_conf.astCommPool[3].u32BlkCnt   = 8;    
 
    /* init system */
    ret_val = SAMPLE_COMM_SYS_Init(&vb_conf);
    if (HI_SUCCESS != ret_val)
    {
        utils_print("system init failed with %d!\n", ret_val);
        return FALSE;
    }

    if(!start_audio_system())
    {
        return FALSE;
    }

    /*config vi*/
    init_result = start_vi();
    if(!init_result)
    {
        utils_print("start vi failed\n");
        return FALSE;
    }

    return TRUE;       
}

void deinit_mpp()
{
    stop_vi();
    stop_audio_system();
    /* exit system */
    SAMPLE_COMM_SYS_Exit();
}

#if 0
void board_get_yuv_from_vpss_chn(char** yuv_buf)
{
    HI_S32 ret_val;
    VPSS_GRP vpss_grp = 0;
    HI_S32 time_out = 3000;
    VPSS_CHN vpss_chn = 2;
    VIDEO_FRAME_INFO_S  video_frame;
    
    ret_val = HI_MPI_VPSS_GetChnFrame(vpss_grp, vpss_chn, &video_frame, time_out);
    if (HI_SUCCESS != ret_val)
    {
        SAMPLE_PRT("vpss_grp[%d] get vpss_chn[%d] frame failed, ret:0x%08x\n",vpss_grp, vpss_chn, ret_val);
        return;
    }

    /* to change to yuv picture */
    sample_yuv_8bit_dump(&video_frame.stVFrame, (void**)yuv_buf);
 
    ret_val = HI_MPI_VPSS_ReleaseChnFrame(vpss_grp, vpss_chn, &video_frame);
    if ( HI_SUCCESS != ret_val )
    {
        SAMPLE_PRT("vpss release frame failed, ret:0x%08x\n", ret_val);
        return;
    }  

    return;
}
#endif 
/* default venc chn 0 */
void board_get_stream_from_venc_chn(int width, int height)
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

    prctl(PR_SET_NAME, "board_get_stream_from_venc_chn");

    g_enc_stream_status = TRUE;
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

    while (g_enc_stream_status)
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
            utils_print("get venc chn %d stream time out, exit thread\n", venc_chn);
            continue;
        }
        else
        {
            if (g_start_record && !g_recording)
            {   
                board_create_mp4_file(s_file_name);
                g_recording = TRUE;
            }

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
                venc_stream.pstPack = (VENC_PACK_S*)utils_malloc(sizeof(VENC_PACK_S) * chn_stat.u32CurPacks);
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
                    utils_free(venc_stream.pstPack);
                    venc_stream.pstPack = NULL;
                    utils_print("HI_MPI_VENC_GetStream failed with %#x!\n", ret_val);
                    break;
                }
                
                /* to save mp4 */
                if(g_recording)
                {   
                    board_write_mp4(&venc_stream, width, height);  
                    g_start_record = FALSE;
                }                          
                /*******************************************************
                 step 2.6 : release stream
                *******************************************************/
                ret_val = HI_MPI_VENC_ReleaseStream(venc_chn, &venc_stream);
                if (HI_SUCCESS != ret_val)
                {
                    utils_print("HI_MPI_VENC_ReleaseStream failed!\n");
                    utils_free(venc_stream.pstPack);
                    venc_stream.pstPack = NULL;
                    break;
                }
                /*******************************************************
                 step 2.7 : free pack nodes
                *******************************************************/
                utils_free(venc_stream.pstPack);
                venc_stream.pstPack = NULL;
            }    

            if (g_stop_record)
            {
                board_close_mp4_file();
                g_start_record = FALSE;
                g_recording = FALSE;
                g_stop_record = FALSE;
              }
        }
    }
    g_enc_stream_status = FALSE;
    utils_print("get venc chn stream exit...\n");
    return;
}

/* default venc chn 2 */
void board_get_snap_from_venc_chn(const char* jpg_file)
{
    VENC_CHN venc_chn = 2;

    struct timeval timeout;
    fd_set read_fds;
    HI_S32 venc_fd;
    VENC_CHN_STATUS_S venc_chn_stat;
    VENC_STREAM_S stream;
    HI_S32 ret;
    HI_U32 idx;
    VENC_RECV_PIC_PARAM_S  snap_recv_param;

    if (NULL == jpg_file)
    {
        return;
    }

    snap_recv_param.s32RecvPicNum = 1;
    ret = HI_MPI_VENC_StartRecvFrame(venc_chn, &snap_recv_param);
    if (HI_SUCCESS != ret)
    {
        utils_print("HI_MPI_VENC_StartRecvPic faild with%#x!\n", ret);
        return;
    }

    venc_fd = HI_MPI_VENC_GetFd(venc_chn);
    if (venc_fd < 0)
    {
        utils_print("HI_MPI_VENC_GetFd faild with%#x!\n", venc_fd);
        return;
    }

    FD_ZERO(&read_fds);
    FD_SET(venc_fd, &read_fds);
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;
    int retry_count = 0;
    while(retry_count < 3)
    {
        ret = select(venc_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ret < 0)
        {
            utils_print("snap select failed!\n");
            return;
        }
        else if (0 == ret)
        {
            utils_print("snap time out!\n");
            retry_count ++;
            continue;
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
                stream.pstPack = (VENC_PACK_S*)utils_malloc(sizeof(VENC_PACK_S) * venc_chn_stat.u32CurPacks);
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

                    utils_free(stream.pstPack);
                    stream.pstPack = NULL;
                    return;
                }
              
                FILE* pFile = fopen(jpg_file, "wb");
                if (pFile == NULL)
                {
                    utils_print("open file %s err\n", jpg_file);
                    ret = HI_MPI_VENC_ReleaseStream(venc_chn, &stream);
                    utils_free(stream.pstPack);
                    stream.pstPack = NULL;
                    break;
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

                    utils_free(stream.pstPack);
                    stream.pstPack = NULL;
                    return;
                }

                utils_free(stream.pstPack);
                stream.pstPack = NULL;
            }
            break;
        }
    }
    retry_count = 0;

    ret = HI_MPI_VENC_StopRecvFrame(venc_chn);
    if (ret != HI_SUCCESS)
    {
        utils_print("HI_MPI_VENC_StopRecvPic failed with %#x!\n",  ret);
        return;
    }

    return;
}

void start_sample_video_thread(void* data)
{
    pthread_t video_tid;

    pthread_create(&video_tid, NULL, sample_video_cb, data);
    pthread_detach(video_tid);

    return;
}

void stop_sample_video_thread()
{
    g_enc_stream_status = FALSE;
}

void start_play_mp3_thread()
{
    pthread_t play_id;

    g_play_mp3_status =  TRUE;
    pthread_create(&play_id, NULL, play_mp3_cb, NULL);
    pthread_detach(play_id);

    return;
}

void stop_play_mp3_thread()
{
    g_play_mp3_status = FALSE;
}

void start_sample_voice_thread(void* data)
{
    pthread_t voice_tid;

    g_sample_pcm_status = TRUE;
    pthread_create(&voice_tid, NULL, sample_pcm_cb, NULL);
    pthread_detach(voice_tid);

    return;
}

void stop_sample_voice_thread()
{
    g_sample_pcm_status = FALSE;
}

void start_video_recording(const char* filename)
{
    strcpy(s_file_name, filename);
    g_start_record = TRUE;
}

void stop_video_recording()
{
    g_stop_record = TRUE;
}

void delete_video()
{
    g_start_record = FALSE;
    g_recording = FALSE;
    g_stop_record = FALSE;
    board_delete_current_mp4_file();
}


