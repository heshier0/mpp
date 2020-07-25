
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


#include "sample_comm.h"
#include "acodec.h"
#include "audio_mp3_adp.h"

#include "common.h"
#include "utils.h"

static void* sample_pcm_cb(void *data)
{
    HI_S32 i, j, s32Ret;
    AI_CHN      AiChn = 0;
    ADEC_CHN    AdChn = 0;
    HI_S32      s32AiChnCnt;
    HI_S32      s32AencChnCnt;
    AENC_CHN    AeChn = 0;
    AIO_ATTR_S stAioAttr;
    AI_VQE_CONFIG_S stAiVqeTalkAttr;
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
    memset(&stAiVqeTalkAttr, 0, sizeof(AI_TALKVQE_CONFIG_S));
    stAiVqeTalkAttr.enWorkstate = VQE_WORKSTATE_COMMON;
    stAiVqeTalkAttr.s32FrameSample = 1024;
    stAiVqeTalkAttr.s32WorkSampleRate = AUDIO_SAMPLE_RATE_16000;
    stAiVqeTalkAttr.stAecCfg.bUsrMode = HI_TRUE;
    stAiVqeTalkAttr.stAecCfg.s16EchoBandLow = 10;
    stAiVqeTalkAttr.stAecCfg.s16EchoBandLow2 = 25;
    stAiVqeTalkAttr.stAecCfg.s16EchoBandHigh = 28;
    stAiVqeTalkAttr.stAecCfg.s16EchoBandHigh2 = 35;
    stAiVqeTalkAttr.stAgcCfg.bUsrMode = HI_FALSE;
    stAiVqeTalkAttr.stAnrCfg.bUsrMode = HI_FALSE;
    stAiVqeTalkAttr.stHpfCfg.bUsrMode = HI_FALSE;
    stAiVqeTalkAttr.u32OpenMask = AI_TALKVQE_MASK_AEC | AI_TALKVQE_MASK_AGC | AI_TALKVQE_MASK_ANR | AI_TALKVQE_MASK_HPF;

    //开启Audio In
    s32AiChnCnt = stAioAttr.u32ChnCnt;
    s32Ret = SAMPLE_COMM_AUDIO_StartAi(AiDev, s32AiChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE, pAiVqeAttr, 2);
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
    s32Ret = HI_MPI_AO_SetVolume(AoDev, -30);
    if (s32Ret != HI_SUCCESS)
    {
        utils_print("ret=%d\n",s32Ret);
        goto ADECAO_ERR1;
    }
 

    //解码
    int fd =  utils_open_fifo(MP3_FIFO, O_RDONLY | O_NONBLOCK);
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
    HI_S32             s32Ret = HI_SUCCESS;

    HI_S32             s32ViCnt       = 2;
    VI_DEV             ViDev[2]       = {0, 1};
    VI_PIPE            ViPipe[4]      = {0, 1, 2, 3};
    VI_CHN             ViChn          = 0;
    HI_S32             s32WorkSnsId   = 0;
    SAMPLE_VI_CONFIG_S stViConfig;

    SIZE_S             stSize;
    PIC_SIZE_E         enPicSize;

    WDR_MODE_E         enWDRMode      = WDR_MODE_NONE;
    DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E     enVideoFormat  = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E    enCompressMode = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E     enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;

    VPSS_GRP           VpssGrp[2]        = {0, 1};
    VPSS_GRP_ATTR_S    stVpssGrpAttr;
    VPSS_CHN           VpssChn0 = VPSS_CHN0, VpssChn1 = VPSS_CHN1;
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

    stViConfig.as32WorkingViId[1]                     = 1;
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
        goto EXIT;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get picture size failed!\n");
        goto EXIT;
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
    stVpssGrpAttr.enPixelFormat                  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.u32MaxW                        = stSize.u32Width;
    stVpssGrpAttr.u32MaxH                        = stSize.u32Height;
    stVpssGrpAttr.bNrEn                          = HI_TRUE;
    stVpssGrpAttr.stNrAttr.enCompressMode        = COMPRESS_MODE_NONE;
    stVpssGrpAttr.stNrAttr.enNrMotionMode        = NR_MOTION_MODE_NORMAL;
    //通道0不能缩小
    astVpssChnAttr[VpssChn0].u32Width                    = 1920; //stSize.u32Width;
    astVpssChnAttr[VpssChn0].u32Height                   = 1080; //stSize.u32Height;
    astVpssChnAttr[VpssChn0].enChnMode                   = VPSS_CHN_MODE_USER;
    astVpssChnAttr[VpssChn0].enCompressMode              = enCompressMode;
    astVpssChnAttr[VpssChn0].enDynamicRange              = enDynamicRange;
    astVpssChnAttr[VpssChn0].enVideoFormat               = enVideoFormat;
    astVpssChnAttr[VpssChn0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    astVpssChnAttr[VpssChn0].stFrameRate.s32SrcFrameRate = 30;
    astVpssChnAttr[VpssChn0].stFrameRate.s32DstFrameRate = 30;
    astVpssChnAttr[VpssChn0].u32Depth                    = 0;
    astVpssChnAttr[VpssChn0].bMirror                     = HI_FALSE;
    astVpssChnAttr[VpssChn0].bFlip                       = HI_FALSE;
    astVpssChnAttr[VpssChn0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
	//通道1只能缩小
    astVpssChnAttr[VpssChn1].u32Width                    = 704;//hxt_get_video_width_cfg();
    astVpssChnAttr[VpssChn1].u32Height                   = 576;//hxt_get_video_height_cfg();
    astVpssChnAttr[VpssChn1].enChnMode                   = VPSS_CHN_MODE_USER;
    astVpssChnAttr[VpssChn1].enCompressMode              = enCompressMode;
    astVpssChnAttr[VpssChn1].enDynamicRange              = enDynamicRange;
    astVpssChnAttr[VpssChn1].enVideoFormat               = enVideoFormat;
    astVpssChnAttr[VpssChn1].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    astVpssChnAttr[VpssChn1].stFrameRate.s32SrcFrameRate = 30;
    astVpssChnAttr[VpssChn1].stFrameRate.s32DstFrameRate = 30;
    astVpssChnAttr[VpssChn1].u32Depth                    = 0;
    astVpssChnAttr[VpssChn1].bMirror                     = HI_FALSE;
    astVpssChnAttr[VpssChn1].bFlip                       = HI_FALSE;
    astVpssChnAttr[VpssChn1].stAspectRatio.enMode        = ASPECT_RATIO_NONE;

    /*start vpss*/
    abChnEnable[0] = HI_TRUE;
    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp[0], abChnEnable, &stVpssGrpAttr, astVpssChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT1;
    }
    /* start vpssgrp 1 */
    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp[1], abChnEnable, &stVpssGrpAttr, astVpssChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT1;
    }

    //rotate 
    // HI_MPI_VPSS_SetChnRotation(VpssGrp[0], VPSS_CHN0, ROTATION_270);
    // HI_MPI_VPSS_SetChnRotation(VpssGrp[0], VPSS_CHN1, ROTATION_270);
    // HI_MPI_VPSS_SetChnRotation(VpssGrp[1], VPSS_CHN0, ROTATION_270);
    // HI_MPI_VPSS_SetChnRotation(VpssGrp[1], VPSS_CHN1, ROTATION_270);

    /* vi bind vpss */
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

    while(g_video_status)
    {
        usleep(10*1000);
    }

    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe[2], ViChn, VpssGrp[1]);
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe[0], ViChn, VpssGrp[0]);
EXIT3:
    SAMPLE_COMM_VPSS_Stop(VpssGrp[1], abChnEnable);
EXIT2:
    SAMPLE_COMM_VPSS_Stop(VpssGrp[0], abChnEnable);
EXIT1:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    utils_print("video sample thread exit...\n");
    return NULL;
}


void board_mpp_init()
{
    HI_S32 s32Ret = HI_SUCCESS;
    int blk_size = 0;
    SIZE_S video_size;
    video_size.u32Width = 1920;
    video_size.u32Height = 1080;

    /* init system */
    VB_CONFIG_S vb_conf;
    hi_memset(&vb_conf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    vb_conf.u32MaxPoolCnt = 2;

    blk_size = COMMON_GetPicBufferSize(video_size.u32Width, video_size.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    vb_conf.astCommPool[0].u64BlkSize  = blk_size;
    vb_conf.astCommPool[0].u32BlkCnt   = 20;
    
    blk_size = VI_GetRawBufferSize(video_size.u32Width, video_size.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    vb_conf.astCommPool[1].u64BlkSize  = blk_size;
    vb_conf.astCommPool[1].u32BlkCnt   = 4;    
    
    s32Ret = SAMPLE_COMM_SYS_Init(&vb_conf);
    if (HI_SUCCESS != s32Ret)
    {
        utils_print("system init failed with %d!\n", s32Ret);
        return;
    }       
}

void board_mpp_deinit()
{
    /* exit system */
    SAMPLE_COMM_SYS_Exit();
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


