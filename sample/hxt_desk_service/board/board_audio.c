#
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
#include <signal.h>

#include "sample_comm.h"
#include "acodec.h"
#include "audio_mp3_adp.h"

#include "common.h"
#include "utils.h"

static HI_BOOL bPlayed = HI_TRUE;
static HI_BOOL bSampled = HI_TRUE;

static HI_S32 sample_pcm(HI_VOID)
{
    HI_S32 i, j, s32Ret;
    AI_CHN      AiChn;
    ADEC_CHN    AdChn = 0;
    HI_S32      s32AiChnCnt;
    HI_S32      s32AencChnCnt;
    AENC_CHN    AeChn = 0;
    FILE*       pfd = NULL;
    AIO_ATTR_S stAioAttr;

    AUDIO_DEV   AiDev = SAMPLE_AUDIO_INNER_AI_DEV;
    stAioAttr.enSamplerate   = AUDIO_SAMPLE_RATE_16000;
    stAioAttr.enBitwidth     = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode     = AIO_MODE_I2S_MASTER;
    stAioAttr.enSoundmode    = AUDIO_SOUND_MODE_MONO;
    stAioAttr.u32EXFlag      = 0;
    stAioAttr.u32FrmNum      = 30;
    stAioAttr.u32PtNumPerFrm = 1024;
    stAioAttr.u32ChnCnt      = 2;
    stAioAttr.u32ClkSel      = 0;
    stAioAttr.enI2sType      = AIO_I2STYPE_INNERCODEC;

    //开启Audio In
    s32AiChnCnt = stAioAttr.u32ChnCnt;
    s32Ret = SAMPLE_COMM_AUDIO_StartAi(AiDev, s32AiChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE, NULL, 0);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        goto AIAENC_ERR6;
    }

    //音频codec属性设置
    s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        goto AIAENC_ERR5;
    }

    //音频编码器属性设置
    s32AencChnCnt = stAioAttr.u32ChnCnt >> 1; //stAioAttr.enSoundmode;
    s32Ret = SAMPLE_COMM_AUDIO_StartAenc(s32AencChnCnt, &stAioAttr, PT_LPCM);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
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
            SAMPLE_DBG(s32Ret);
            for (j=0; j<i; j++)
            {
                SAMPLE_COMM_AUDIO_AencUnbindAi(AiDev, j, j);
            }
            goto AIAENC_ERR4;
        }

        printf("Ai(%d,%d) bind to AencChn:%d ok!\n", AiDev , AiChn, AeChn);
    }

    FILE *fp = fopen("/tmp/board_sample.pcm", "w");
    if(fp == NULL)
    {
        goto AIAENC_ERR4;
    }

    //编码存成文件
    s32Ret = SAMPLE_COMM_AUDIO_CreatTrdAencAdec(AeChn, AdChn, fp);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        goto AIAENC_ERR1;
    }

    bSampled = HI_TRUE;
    while(bSampled) 
    {
        sleep(5);
    }


    s32Ret = SAMPLE_COMM_AUDIO_DestoryTrdAencAdec(AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
    }   

AIAENC_ERR1:
    for (i = 0; i < s32AencChnCnt; i++)
    {
        AeChn = i;
        AiChn = i;
    
        s32Ret |= SAMPLE_COMM_AUDIO_AencUnbindAi(AiDev, AiChn, AeChn);
        if (s32Ret != HI_SUCCESS)
        {
        SAMPLE_DBG(s32Ret);
        }
    }

AIAENC_ERR4:
    s32Ret |= SAMPLE_COMM_AUDIO_StopAenc(s32AencChnCnt);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
    }

AIAENC_ERR5:
    s32Ret |= SAMPLE_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
    }

AIAENC_ERR6:

    return s32Ret;
}

static HI_S32 play_mp3()

{
    HI_S32      s32Ret;
    AO_CHN      AoChn = 0;
    ADEC_CHN    AdChn = 0;
    HI_S32      s32AoChnCnt;
    AIO_ATTR_S stAioAttr;

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
        SAMPLE_DBG(s32Ret);
        goto ADECAO_ERR3;
    }

    //音频输出设置
    s32AoChnCnt = stAioAttr.u32ChnCnt;
    s32Ret = SAMPLE_COMM_AUDIO_StartAo(AoDev, s32AoChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        goto ADECAO_ERR2;
    }

    s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        goto ADECAO_ERR1;
    }

    s32Ret = SAMPLE_COMM_AUDIO_AoBindAdec(AoDev, AoChn, AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        goto ADECAO_ERR1;
    }

    s32Ret = HI_MPI_AO_SetVolume(AoDev, -30);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        goto ADECAO_ERR1;
    }
 
    int fd =  utils_open_fifo(IFLYOS_MP3_FIFO, O_RDONLY | O_NONBLOCK);
    s32Ret = SAMPLE_COMM_AUDIO_CreatTrdFileAdec(AdChn, fd);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        goto ADECAO_ERR0;
    }

    printf("bind adec:%d to ao(%d,%d) ok \n", AdChn, AoDev, AoChn);
   
    bPlayed = HI_TRUE;
    while(bPlayed)
    {
        sleep(5);
    }

    s32Ret = SAMPLE_COMM_AUDIO_DestoryTrdFileAdec(AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

ADECAO_ERR0:
    s32Ret = SAMPLE_COMM_AUDIO_AoUnbindAdec(AoDev, AoChn, AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
    }

ADECAO_ERR1:
    s32Ret |= SAMPLE_COMM_AUDIO_StopAo(AoDev, s32AoChnCnt, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
    }
ADECAO_ERR2:
    s32Ret |= SAMPLE_COMM_AUDIO_StopAdec(AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
    }

ADECAO_ERR3:
    return s32Ret;

}

static void handle_signal(HI_S32 signo)
{
    // signal(SIGINT, SIG_IGN);
    // signal(SIGTERM, SIG_IGN);

    if (SIGINT == signo || SIGTERM == signo)
    {
        bPlayed = HI_FALSE;
        bSampled = HI_FALSE;

        SAMPLE_COMM_AUDIO_DestoryAllTrd();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    exit(0);
}

void board_audio_start()
{
    HI_S32 s32Ret = HI_SUCCESS;
    VB_CONFIG_S stVbConf;
    HI_CHAR aszFileName[FILE_NAME_LEN] = {0};
    HI_S32 s32Vol = 0;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal); 

    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        printf("%s: system init failed with %d!\n", __FUNCTION__, s32Ret);
        return;
    }       

    HI_MPI_AENC_AacInit();
    HI_MPI_ADEC_AacInit();
    HI_MPI_ADEC_Mp3Init();

    pthread_t play_tid, sample_tid;
    pthread_create(&play_tid, NULL, play_mp3, NULL);
    pthread_detach(play_tid);

    pthread_create(&sample_tid, NULL, sample_pcm, NULL);
    pthread_detach(sample_tid);

    HI_BOOL bStop = HI_FALSE;
    while(!bStop)
    {
        sleep(1);
    }

    HI_MPI_AENC_AacDeInit();

    SAMPLE_COMM_SYS_Exit();

    return;
}
