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

#include "hi_common.h"
#include "mpi_sys.h"

#include "sample_common.h"

unsigned int  board_get_chipId()
{
    //0x3516D300
    unsigned int chip = 0;
    HI_MPI_SYS_GetChipId(&chip);
    
    return chip;
}

void board_init_sys()
{
    int ret = 0;

    int vi_count = 2;
    VI_DEV vi_dev[2] = {0, 1};

    SAMPLE_VI_CONFIG_S stViConfig;

    // SIZE_S             stSize;
    // VB_CONFIG_S        stVbConf;
    // PIC_SIZE_E         enPicSize;
    // HI_U32             u32BlkSize;
    // VO_CHN             VoChn          = 0;
    // SAMPLE_VO_CONFIG_S stVoConfig;

    // WDR_MODE_E         enWDRMode      = WDR_MODE_NONE;
    // DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_SDR8;
    // PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    // VIDEO_FORMAT_E     enVideoFormat  = VIDEO_FORMAT_LINEAR;
    // COMPRESS_MODE_E    enCompressMode = COMPRESS_MODE_NONE;
    // VI_VPSS_MODE_E     enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;

    // VPSS_GRP           VpssGrp[2]        = {0, 1};
    // VPSS_GRP_ATTR_S    stVpssGrpAttr;
    // VPSS_CHN           VpssChn        = VPSS_CHN0;
    // HI_BOOL            abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {1, 1};
    // VPSS_CHN_ATTR_S    astVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];

    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    stViConfig.s32WorkingViNum = vi_count;

    stViConfig.as32WorkingViId[0]                     = 0;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev         = vi_dev[0];
    stViConfig.astViInfo[0].stSnsInfo.s32BusId        = 0;
    stViConfig.astViInfo[0].stDevInfo.ViDev           = vi_dev[0];
    stViConfig.astViInfo[0].stDevInfo.enWDRMode       = WDR_MODE_NONE;
    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]       = 0;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]       = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]       = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]       = -1;
    stViConfig.astViInfo[0].stChnInfo.ViChn           = 0;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat     = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange  = DYNAMIC_RANGE_SDR8;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat   = VIDEO_FORMAT_LINEAR;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode  = COMPRESS_MODE_NONE;

    stViConfig.as32WorkingViId[1]                     = 1;
    stViConfig.astViInfo[1].stSnsInfo.MipiDev         = vi_dev[1];
    stViConfig.astViInfo[1].stSnsInfo.s32BusId        = 1;
    stViConfig.astViInfo[1].stDevInfo.ViDev           = vi_dev[1];
    stViConfig.astViInfo[1].stDevInfo.enWDRMode       = WDR_MODE_NONE;
    stViConfig.astViInfo[1].stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[0]       = 2; // VIPIPE2
    stViConfig.astViInfo[1].stPipeInfo.aPipe[1]       = -1;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[2]       = -1;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[3]       = -1;
    stViConfig.astViInfo[1].stChnInfo.ViChn           = 0;
    stViConfig.astViInfo[1].stChnInfo.enPixFormat     = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stViConfig.astViInfo[1].stChnInfo.enDynamicRange  = DYNAMIC_RANGE_SDR8;
    stViConfig.astViInfo[1].stChnInfo.enVideoFormat   = VIDEO_FORMAT_LINEAR;
    stViConfig.astViInfo[1].stChnInfo.enCompressMode  = COMPRESS_MODE_NONE;

    SAMPLE_COMM_SYS_Init()
}

void board_deinit_sys()
{

}

void board_init_sensor()
{

}

void board_deinit_sensor()
{

}

void board_init_video()
{

}

void board_deinit_video()
{

}

void board_init_audio()
{

}

void board_deinit_audio()
{

}
