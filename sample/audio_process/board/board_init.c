#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

#include "sample_comm.h"
#include "audio_mp3_adp.h"


#include "utils.h"


unsigned int  board_get_chipId()
{
    //0x3516D300
    unsigned int chip = 0;
    HI_MPI_SYS_GetChipId(&chip);
    
    return chip;
}

int board_init_sys()
{
    int ret = 0;
    int blk_size = 0;

    int vi_count = 2;
    VI_DEV vi_dev[2] = {0, 1};
    VI_PIPE vi_pipe[4] = {0, 1, 2, 3};
    VI_CHN vi_chn = 0;
    VPSS_GRP vpss_grp[2] = {0, 1};
    VPSS_CHN vpss_chn = VPSS_CHN0;
    HI_BOOL chn_enable[VPSS_MAX_PHY_CHN_NUM] = {1, 1};

    SIZE_S video_size;
    video_size.u32Width = hxt_get_video_width_cfg();
    video_size.u32Height = hxt_get_video_height_cfg();

    VB_CONFIG_S vb_conf;
    hi_memset(&vb_conf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    vb_conf.u32MaxPoolCnt = 2;

    blk_size = COMMON_GetPicBufferSize(video_size.u32Width, video_size.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    vb_conf.astCommPool[0].u64BlkSize  = blk_size;
    vb_conf.astCommPool[0].u32BlkCnt   = 20;

    blk_size = VI_GetRawBufferSize(video_size.u32Width, video_size.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    vb_conf.astCommPool[1].u64BlkSize  = blk_size;
    vb_conf.astCommPool[1].u32BlkCnt   = 4;

    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();

    ret = HI_MPI_VB_SetConfig(&vb_conf);
    if (HI_SUCCESS != ret)
    {
        utils_print("HI_MPI_VB_SetConfig failed!\n");
        return ret;
    }

    ret = HI_MPI_VB_Init();
    if (HI_SUCCESS != ret)
    {
        utils_print("HI_MPI_VB_Init failed!\n");
        return ret;
    }

    ret = HI_MPI_SYS_Init();
    if (HI_SUCCESS != ret)
    {
        utils_print("HI_MPI_SYS_Init failed!\n");
        HI_MPI_VB_Exit();
        return ret;
    }

    return ret;
}

void board_deinit_sys()
{
    HI_MPI_SYS_Exit();
    HI_MPI_VB_ExitModCommPool(VB_UID_VDEC);
    HI_MPI_VB_Exit();
    return;
}

int board_init_video()
{
    int vi_count = 2;
    VI_DEV vi_dev[2] = {0, 1};
    VI_PIPE vi_pipe[4] = {0, 1, 2, 3};
    VI_CHN vi_chn = 0;
    VPSS_GRP vpss_grp[2] = {0, 1};
    VPSS_CHN vpss_chn = VPSS_CHN0;
    HI_BOOL chn_enable[VPSS_MAX_PHY_CHN_NUM] = {1, 1};
   
    int ret = HI_SUCCESS;
    
    SIZE_S             video_size;
    PIC_SIZE_E         pic_size;

    VPSS_GRP_ATTR_S    vpss_grp_attrs;
    VPSS_CHN_ATTR_S    vpss_chn_attrs[VPSS_MAX_PHY_CHN_NUM];

    SAMPLE_VI_CONFIG_S vi_cfg = {0};
    SAMPLE_COMM_VI_GetSensorInfo(&vi_cfg);
    vi_cfg.s32WorkingViNum = vi_count;

    vi_cfg.as32WorkingViId[0]                     = 0;
    vi_cfg.astViInfo[0].stSnsInfo.MipiDev         = vi_dev[0];
    vi_cfg.astViInfo[0].stSnsInfo.s32BusId        = 0;
    vi_cfg.astViInfo[0].stDevInfo.ViDev           = vi_dev[0];
    vi_cfg.astViInfo[0].stDevInfo.enWDRMode       = WDR_MODE_NONE;
    vi_cfg.astViInfo[0].stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    vi_cfg.astViInfo[0].stPipeInfo.aPipe[0]       = 0;
    vi_cfg.astViInfo[0].stPipeInfo.aPipe[1]       = -1;
    vi_cfg.astViInfo[0].stPipeInfo.aPipe[2]       = -1;
    vi_cfg.astViInfo[0].stPipeInfo.aPipe[3]       = -1;
    vi_cfg.astViInfo[0].stChnInfo.ViChn           = 0;
    vi_cfg.astViInfo[0].stChnInfo.enPixFormat     = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vi_cfg.astViInfo[0].stChnInfo.enDynamicRange  = DYNAMIC_RANGE_SDR8;
    vi_cfg.astViInfo[0].stChnInfo.enVideoFormat   = VIDEO_FORMAT_LINEAR;
    vi_cfg.astViInfo[0].stChnInfo.enCompressMode  = COMPRESS_MODE_NONE;

    vi_cfg.as32WorkingViId[1]                     = 1;
    vi_cfg.astViInfo[1].stSnsInfo.MipiDev         = vi_dev[1];
    vi_cfg.astViInfo[1].stSnsInfo.s32BusId        = 1;
    vi_cfg.astViInfo[1].stDevInfo.ViDev           = vi_dev[1];
    vi_cfg.astViInfo[1].stDevInfo.enWDRMode       = WDR_MODE_NONE;
    vi_cfg.astViInfo[1].stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    vi_cfg.astViInfo[1].stPipeInfo.aPipe[0]       = 2; 
    vi_cfg.astViInfo[1].stPipeInfo.aPipe[1]       = -1;
    vi_cfg.astViInfo[1].stPipeInfo.aPipe[2]       = -1;
    vi_cfg.astViInfo[1].stPipeInfo.aPipe[3]       = -1;
    vi_cfg.astViInfo[1].stChnInfo.ViChn           = 0;
    vi_cfg.astViInfo[1].stChnInfo.enPixFormat     = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vi_cfg.astViInfo[1].stChnInfo.enDynamicRange  = DYNAMIC_RANGE_SDR8;
    vi_cfg.astViInfo[1].stChnInfo.enVideoFormat   = VIDEO_FORMAT_LINEAR;
    vi_cfg.astViInfo[1].stChnInfo.enCompressMode  = COMPRESS_MODE_NONE;
    
    /*get picture size*/
    ret = SAMPLE_COMM_VI_GetSizeBySensor(vi_cfg.astViInfo[0].stSnsInfo.enSnsType, &pic_size);
    if (HI_SUCCESS != ret)
    {
        utils_print("get picture size by sensor failed!\n");
        return ret;
    }

    ret = SAMPLE_COMM_SYS_GetPicSize(pic_size, &video_size);
    if (HI_SUCCESS != ret)
    {
        SAMPLE_PRT("get picture size failed!\n");
        return ret;
    }

    /*start vi*/
    ret = SAMPLE_COMM_VI_StartVi(&vi_cfg);
    if (HI_SUCCESS != ret)
    {
        utils_print("start vi failed.s32Ret:0x%x !\n", ret);
        return ret;
    }

    /*setup vpss*/
    hi_memset(&vpss_grp_attrs, sizeof(VPSS_GRP_ATTR_S), 0, sizeof(VPSS_GRP_ATTR_S));
    vpss_grp_attrs.stFrameRate.s32SrcFrameRate    = -1;
    vpss_grp_attrs.stFrameRate.s32DstFrameRate    = -1;
    vpss_grp_attrs.enDynamicRange                 = DYNAMIC_RANGE_SDR8;
    vpss_grp_attrs.enPixelFormat                  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vpss_grp_attrs.u32MaxW                        = video_size.u32Width;
    vpss_grp_attrs.u32MaxH                        = video_size.u32Height;
    vpss_grp_attrs.bNrEn                          = HI_TRUE;
    vpss_grp_attrs.stNrAttr.enCompressMode        = COMPRESS_MODE_FRAME;
    vpss_grp_attrs.stNrAttr.enNrMotionMode        = NR_MOTION_MODE_NORMAL;
    //通道0不能缩小
    vpss_chn_attrs[vpss_chn].u32Width                    = video_size.u32Width;
    vpss_chn_attrs[vpss_chn].u32Height                   = video_size.u32Height;
    vpss_chn_attrs[vpss_chn].enChnMode                   = VPSS_CHN_MODE_USER;
    vpss_chn_attrs[vpss_chn].enCompressMode              = COMPRESS_MODE_NONE;
    vpss_chn_attrs[vpss_chn].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    vpss_chn_attrs[vpss_chn].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    vpss_chn_attrs[vpss_chn].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vpss_chn_attrs[vpss_chn].stFrameRate.s32SrcFrameRate = 30;
    vpss_chn_attrs[vpss_chn].stFrameRate.s32DstFrameRate = 30;
    vpss_chn_attrs[vpss_chn].u32Depth                    = 0;
    vpss_chn_attrs[vpss_chn].bMirror                     = HI_FALSE;
    vpss_chn_attrs[vpss_chn].bFlip                       = HI_FALSE;
    vpss_chn_attrs[vpss_chn].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
	//通道1只能缩小
    vpss_chn_attrs[1].u32Width                    = hxt_get_video_width_cfg();
    vpss_chn_attrs[1].u32Height                   = hxt_get_video_height_cfg();
    vpss_chn_attrs[1].enChnMode                   = VPSS_CHN_MODE_USER;
    vpss_chn_attrs[1].enCompressMode              = COMPRESS_MODE_NONE;
    vpss_chn_attrs[1].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    vpss_chn_attrs[1].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    vpss_chn_attrs[1].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vpss_chn_attrs[1].stFrameRate.s32SrcFrameRate = 30;
    vpss_chn_attrs[1].stFrameRate.s32DstFrameRate = 30;
    vpss_chn_attrs[1].u32Depth                    = 0;
    vpss_chn_attrs[1].bMirror                     = HI_FALSE;
    vpss_chn_attrs[1].bFlip                       = HI_FALSE;
    vpss_chn_attrs[1].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
    /*start vpss*/
    chn_enable[0] = HI_TRUE;
    ret = SAMPLE_COMM_VPSS_Start(vpss_grp[0], chn_enable, &vpss_grp_attrs, vpss_chn_attrs);
    if (HI_SUCCESS != ret)
    {
        utils_print("start vpss group failed. s32Ret: 0x%x !\n", ret);
        SAMPLE_COMM_VI_StopVi(&vi_cfg); 
        return ret;
    }
    /* start vpssgrp 1 */
    ret = SAMPLE_COMM_VPSS_Start(vpss_grp[1], chn_enable, &vpss_grp_attrs, vpss_chn_attrs);
    if (HI_SUCCESS != ret)
    {
        utils_print("start vpss group failed. s32Ret: 0x%x !\n", ret);
        SAMPLE_COMM_VI_StopVi(&vi_cfg); 
        return ret;
    }

    /*vpss bind vo*/
    ret = SAMPLE_COMM_VI_Bind_VPSS(vi_pipe[0], vi_chn, vpss_grp[0]);
    if (HI_SUCCESS != ret)
    {
        SAMPLE_PRT("vpss bind vi failed. s32Ret: 0x%x !\n", ret);
        SAMPLE_COMM_VPSS_Stop(vpss_grp[0], chn_enable);  
        SAMPLE_COMM_VI_StopVi(&vi_cfg); 
        return ret;
    }

    ret = SAMPLE_COMM_VI_Bind_VPSS(vi_pipe[2], vi_chn, vpss_grp[1]);
    if (HI_SUCCESS != ret)
    {
        SAMPLE_PRT("vpss bind vi failed. s32Ret: 0x%x !\n", ret);
        SAMPLE_COMM_VI_UnBind_VPSS(vi_pipe[0], vi_chn, vpss_grp[0]);
        SAMPLE_COMM_VPSS_Stop(vpss_grp[1], chn_enable);
        SAMPLE_COMM_VI_StopVi(&vi_cfg); 
        return ret;
    }

    return ret;
}

void board_deinit_video()
{
    VI_DEV vi_dev[2] = {0, 1};
    VI_PIPE vi_pipe[4] = {0, 1, 2, 3};
    VI_CHN vi_chn = 0;
    VPSS_GRP vpss_grp[2] = {0, 1};
    HI_BOOL chn_enable[VPSS_MAX_PHY_CHN_NUM] = {1, 1};
    SAMPLE_VI_CONFIG_S vi_cfg = {0};
    SAMPLE_COMM_VI_GetSensorInfo(&vi_cfg);
    vi_cfg.s32WorkingViNum = 2;

    vi_cfg.as32WorkingViId[0]                     = 0;
    vi_cfg.astViInfo[0].stSnsInfo.MipiDev         = vi_dev[0];
    vi_cfg.astViInfo[0].stSnsInfo.s32BusId        = 0;
    vi_cfg.astViInfo[0].stDevInfo.ViDev           = vi_dev[0];
    vi_cfg.astViInfo[0].stDevInfo.enWDRMode       = WDR_MODE_NONE;
    vi_cfg.astViInfo[0].stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    vi_cfg.astViInfo[0].stPipeInfo.aPipe[0]       = 0;
    vi_cfg.astViInfo[0].stPipeInfo.aPipe[1]       = -1;
    vi_cfg.astViInfo[0].stPipeInfo.aPipe[2]       = -1;
    vi_cfg.astViInfo[0].stPipeInfo.aPipe[3]       = -1;
    vi_cfg.astViInfo[0].stChnInfo.ViChn           = 0;
    vi_cfg.astViInfo[0].stChnInfo.enPixFormat     = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vi_cfg.astViInfo[0].stChnInfo.enDynamicRange  = DYNAMIC_RANGE_SDR8;
    vi_cfg.astViInfo[0].stChnInfo.enVideoFormat   = VIDEO_FORMAT_LINEAR;
    vi_cfg.astViInfo[0].stChnInfo.enCompressMode  = COMPRESS_MODE_NONE;

    vi_cfg.as32WorkingViId[1]                     = 1;
    vi_cfg.astViInfo[1].stSnsInfo.MipiDev         = vi_dev[1];
    vi_cfg.astViInfo[1].stSnsInfo.s32BusId        = 1;
    vi_cfg.astViInfo[1].stDevInfo.ViDev           = vi_dev[1];
    vi_cfg.astViInfo[1].stDevInfo.enWDRMode       = WDR_MODE_NONE;
    vi_cfg.astViInfo[1].stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    vi_cfg.astViInfo[1].stPipeInfo.aPipe[0]       = 2; 
    vi_cfg.astViInfo[1].stPipeInfo.aPipe[1]       = -1;
    vi_cfg.astViInfo[1].stPipeInfo.aPipe[2]       = -1;
    vi_cfg.astViInfo[1].stPipeInfo.aPipe[3]       = -1;
    vi_cfg.astViInfo[1].stChnInfo.ViChn           = 0;
    vi_cfg.astViInfo[1].stChnInfo.enPixFormat     = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vi_cfg.astViInfo[1].stChnInfo.enDynamicRange  = DYNAMIC_RANGE_SDR8;
    vi_cfg.astViInfo[1].stChnInfo.enVideoFormat   = VIDEO_FORMAT_LINEAR;
    vi_cfg.astViInfo[1].stChnInfo.enCompressMode  = COMPRESS_MODE_NONE;

    SAMPLE_COMM_VI_UnBind_VPSS(vi_pipe[2], vi_chn, vpss_grp[1]);
    SAMPLE_COMM_VI_UnBind_VPSS(vi_pipe[0], vi_chn, vpss_grp[0]);
    SAMPLE_COMM_VPSS_Stop(vpss_grp[1], chn_enable);
    SAMPLE_COMM_VPSS_Stop(vpss_grp[0], chn_enable);    
    SAMPLE_COMM_VI_StopVi(&vi_cfg);    
}

int board_init_audio_in()
{
    int ret = HI_SUCCESS;

    AUDIO_DEV ai_dev = SAMPLE_AUDIO_INNER_AI_DEV;
    AI_CHN ai_chn = 0;
    int ai_chn_count = 2;
    AENC_CHN aenc_chn = 0;
    int aenc_chn_count = 1;

    /* init encoder for aac */
    HI_MPI_AENC_AacInit();

    /* init board */
    AIO_ATTR_S ai_attrs;
    ai_attrs.enSamplerate   = AUDIO_SAMPLE_RATE_16000;
    ai_attrs.enBitwidth     = AUDIO_BIT_WIDTH_16;
    ai_attrs.enWorkmode     = AIO_MODE_I2S_MASTER;
    ai_attrs.enSoundmode    = AUDIO_SOUND_MODE_MONO;
    ai_attrs.u32EXFlag      = 0;
    ai_attrs.u32FrmNum      = 30;
    ai_attrs.u32PtNumPerFrm = 1024;
    ai_attrs.u32ChnCnt      = 2;
    ai_attrs.u32ClkSel      = 0;
    ai_attrs.enI2sType      = AIO_I2STYPE_INNERCODEC;

    /* 开启Audio In */
    ret = SAMPLE_COMM_AUDIO_StartAi(ai_dev, ai_chn_count, &ai_attrs, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE, NULL, 0);
    if (ret != HI_SUCCESS)
    {
        utils_print("start ai failed. ret:0x%x !\n", ret);
        return ret;
    }

    /* 音频codec属性设置 */
    ret = SAMPLE_COMM_AUDIO_CfgAcodec(&ai_attrs);
    if (ret != HI_SUCCESS)
    {
        utils_print("start ai codec failed. ret:0x%x !\n", ret);
        SAMPLE_COMM_AUDIO_StopAi(ai_dev, ai_chn_count, HI_FALSE, HI_FALSE);
        return ret;
    }

    /* 音频编码器属性设置 */
    aenc_chn_count = ai_attrs.u32ChnCnt >> ai_attrs.enSoundmode;
    ret = SAMPLE_COMM_AUDIO_StartAenc(aenc_chn_count, &ai_attrs, PT_LPCM);
    if (ret != HI_SUCCESS)
    {
        utils_print("start aenc failed. ret:0x%x !\n", ret);
        SAMPLE_COMM_AUDIO_StopAi(ai_dev, ai_chn_count, HI_FALSE, HI_FALSE);
        return ret;
    }

    for (int i = 0; i < aenc_chn_count; i++)
    {
        aenc_chn = i;
        ai_chn = i;

        ret = SAMPLE_COMM_AUDIO_AencBindAi(ai_dev, ai_chn, aenc_chn);
        if (ret != HI_SUCCESS)
        {
            utils_print("start aenc bind ai failed. ret:0x%x !\n", ret);
            for (int j=0; j<i; j++)
            {
                SAMPLE_COMM_AUDIO_AencUnbindAi(ai_dev, j, j);
            }
            SAMPLE_COMM_AUDIO_StopAenc(aenc_chn_count);
            SAMPLE_COMM_AUDIO_StopAi(ai_dev, ai_chn_count, HI_FALSE, HI_FALSE);
            return ret;
        }
    }

    return ret;

}

void board_deinit_audio_in()
{
    AUDIO_DEV ai_dev = SAMPLE_AUDIO_INNER_AI_DEV;
    AI_CHN ai_chn = 0;
    int ai_chn_count = 2;
    AENC_CHN aenc_chn = 0;
    int aenc_chn_count = 2;

    HI_MPI_AENC_AacDeInit();

    /* deinit board */
    for (int i = 0; i < aenc_chn_count; i++)
    {
        aenc_chn = i;
        ai_chn = i;
        SAMPLE_COMM_AUDIO_AencUnbindAi(ai_dev, ai_chn, aenc_chn);
    }
    
    SAMPLE_COMM_AUDIO_StopAenc(aenc_chn_count);
    SAMPLE_COMM_AUDIO_StopAi(ai_dev, ai_chn_count, HI_FALSE, HI_FALSE);

    return;
}

int board_init_audio_out()
{
    int ret = HI_SUCCESS;
    AUDIO_DEV ao_dev = SAMPLE_AUDIO_INNER_AO_DEV;
    AO_CHN ao_chn = 0;
    int ao_chn_count = 0;
    ADEC_CHN adec_chn = 0;

    HI_MPI_ADEC_AacInit();
    HI_MPI_ADEC_Mp3Init();

    /* init board */
    AIO_ATTR_S ao_attrs;
    ao_attrs.enSamplerate   = AUDIO_SAMPLE_RATE_16000;
    ao_attrs.enBitwidth     = AUDIO_BIT_WIDTH_16;
    ao_attrs.enWorkmode     = AIO_MODE_I2S_MASTER;
    ao_attrs.enSoundmode    = AUDIO_SOUND_MODE_STEREO;
    ao_attrs.u32EXFlag      = 0;
    ao_attrs.u32FrmNum      = 30;
    ao_attrs.u32PtNumPerFrm = MP3_SAMPLES_PER_FRAME;
    ao_attrs.u32ChnCnt      = 2;
    ao_attrs.u32ClkSel      = 0;
    ao_attrs.enI2sType      = AIO_I2STYPE_INNERCODEC;

    /* bind decoder */
    ret = SAMPLE_COMM_AUDIO_StartAdec(adec_chn, PT_MP3);
    if (ret != HI_SUCCESS)
    {
        utils_print("start audio decoder failed. ret:0x%x !\n", ret);
        return ret;
    }

    /* start ao */
    ao_chn_count = ao_attrs.u32ChnCnt;
    ret = SAMPLE_COMM_AUDIO_StartAo(ao_dev, ao_chn_count, &ao_attrs, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE);
    if (ret != HI_SUCCESS)
    {
        utils_print("start ao failed. ret:0x%x !\n", ret);
        SAMPLE_COMM_AUDIO_StopAdec(adec_chn);
        return ret;
    }

    ret = SAMPLE_COMM_AUDIO_CfgAcodec(&ao_attrs);
    if (ret != HI_SUCCESS)
    {
        utils_print("config audio codec failed. ret:0x%x !\n", ret);
        SAMPLE_COMM_AUDIO_StopAo(ao_dev, ao_chn_count, HI_FALSE);
        SAMPLE_COMM_AUDIO_StopAdec(adec_chn);
        return ret;
    }

    ret = SAMPLE_COMM_AUDIO_AoBindAdec(ao_dev, ao_chn, adec_chn);
    if (ret != HI_SUCCESS)
    {
        utils_print("config audio codec failed. ret:0x%x !\n", ret);
        SAMPLE_COMM_AUDIO_StopAo(ao_dev, ao_chn_count, HI_FALSE);
        SAMPLE_COMM_AUDIO_StopAdec(adec_chn);
        return ret;
    }

    ret = HI_MPI_AO_SetVolume(ao_dev, -40);
    if (ret != HI_SUCCESS)
    {
        utils_print("set volume failed. ret:0x%x !\n", ret);
        SAMPLE_COMM_AUDIO_AoUnbindAdec(ao_dev, ao_chn, adec_chn);
        SAMPLE_COMM_AUDIO_StopAo(ao_dev, ao_chn_count, HI_FALSE);
        SAMPLE_COMM_AUDIO_StopAdec(adec_chn);
        return ret;
    }

    return ret;
}

void board_deinit_audio_out()
{
    AUDIO_DEV ao_dev = SAMPLE_AUDIO_INNER_AO_DEV;
    AO_CHN ao_chn = 0;
    int ao_chn_count = 2;
    ADEC_CHN adec_chn = 0;

    SAMPLE_COMM_AUDIO_AoUnbindAdec(ao_dev, ao_chn, adec_chn);
    SAMPLE_COMM_AUDIO_StopAo(ao_dev, ao_chn_count, HI_FALSE);
    SAMPLE_COMM_AUDIO_StopAdec(adec_chn);

    return;
}