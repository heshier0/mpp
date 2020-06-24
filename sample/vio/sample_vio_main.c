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

#include "hi_common.h"
#include "sample_vio.h"
#include "mpi_sys.h"
#include "../common/sample_comm.h"

/******************************************************************************
* function : show usage
******************************************************************************/
void SAMPLE_VIO_Usage(char *sPrgNm)
{
    printf("Usage : %s <index>\n", sPrgNm);
    printf("index:\n");
    printf("\t 0)VI (Online) - VPSS(Online) - VO.\n");
    printf("\t 1)WDR(Offline)- VPSS(Offline) - VO. LDC+DIS+SPREAD.\n");
    printf("\t 2)Resolute Ratio Switch.\n");
    printf("\t 3)GDC - VPSS LowDelay.\n");
    printf("\t 4)Double WDR Pipe.\n");
    printf("\t 5)FPN Calibrate & Correction.\n");
    printf("\t 6)WDR Switch.\n");
    printf("\t 7)90/180/270/0/free Rotate.\n");
    printf("\t 8)UserPic.\n");
    printf("\t 9)VI-VPSS-VO(MIPI_TX).\n");
    printf("\t 10)VI-VPSS-VO(two cameras).\n");
    printf("\t 11)VI-VPSS-VO(HDMI).\n");
    printf("\t 12)VI-VPSS-VO(two cameras).\n\n");

	printf("\t use option to start sample_vio\n");
	printf("\t ./sample_vio -f 10 -s 0 -r 90 -R 180 -l 0\n");
	printf("\t -f:0 - 12, call function like VI-VPSS-VO(MIPI_TX) 9:default\n");
	printf("\t -s:0 - 3, 0:default; 1:set sensor0 saturation to 0; 2:set sensor1 saturation to 0; 3 set sensor0 and sensor1 saturation to 0\n");
	printf("\t -r:0 - 3, 0:default; 1:rotate sensor0 90 degree; 2: rotate sensor0 180 degree; 3 rotate sensor0 270 degree \n");
	printf("\t -R:0 - 3, 0:default; 1:rotate sensor1 90 degree; 2: rotate sensor1 180 degree; 3 rotate sensor1 270 degree \n");
	printf("\t -l:0 - n, 0:default, mipi_7inch_1024_600; 1: mipi_8inch_800_1280; 2: mipi_10inch_800_1280\n");
	printf("\t refer to sample/common/sample_comm.h SAMPLE_SNS_TYPE_E. sensor config:\n \ 
		0 SONY_IMX327_MIPI_2M_30FPS_12BIT,\n \
   		1 SONY_IMX327_MIPI_2M_30FPS_12BIT_WDR2TO1,\n \
   		2 SONY_IMX327_2L_MIPI_2M_30FPS_12BIT,\n \
   		3 SONY_IMX327_2L_MIPI_2M_30FPS_12BIT_WDR2TO1,\n \
   		4 SONY_IMX307_MIPI_2M_30FPS_12BIT,\n \
   		5 SONY_IMX307_MIPI_2M_30FPS_12BIT_WDR2TO1,\n \
   		6 SONY_IMX335_MIPI_5M_30FPS_12BIT,\n \
   		7 SONY_IMX335_MIPI_5M_30FPS_10BIT_WDR2TO1,\n \
   		8 SONY_IMX335_MIPI_4M_30FPS_12BIT,\n \
   		9 SONY_IMX335_MIPI_4M_30FPS_10BIT_WDR2TO1,\n \
   		10 SONY_IMX458_MIPI_8M_30FPS_10BIT,\n \
   		11 SONY_IMX458_MIPI_12M_20FPS_10BIT,\n \
   		12 SONY_IMX458_MIPI_4M_60FPS_10BIT,\n \
   		13 SONY_IMX458_MIPI_4M_40FPS_10BIT,\n \
   		14 SONY_IMX458_MIPI_2M_90FPS_10BIT,\n \
   		15 SONY_IMX458_MIPI_1M_129FPS_10BIT,\n \
   		16 PANASONIC_MN34220_LVDS_2M_30FPS_12BIT,\n \
   		17 OMNIVISION_OS05A_MIPI_4M_30FPS_12BIT,\n \
   		18 OMNIVISION_OS05A_MIPI_4M_30FPS_10BIT_WDR2TO1,\n \
   		19 SAMPLE_SNS_TYPE_BUTT\n");
	printf("\t -t:0 - 19,sensor0 config select 2:default\n");
	printf("\t -T:0 - 19,sensor1 config select 2:default\n");


    printf("\t Hi3516DV300/Hi3559V200/Hi3556V200) vo HDMI output.\n");
    printf("\t Hi3516CV500) vo BT1120 output.\n");
    printf("\t If you have any questions, please look at readme.txt!\n");
    return;
}

/******************************************************************************
* function    : main()
* Description : main
******************************************************************************/
#ifdef __HuaweiLite__
int app_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_S32 s32Index  = 10;
    HI_U32 u32VoIntfType = 0;
    HI_U32  u32ChipId;

	HI_S32 ch;
	HI_U32 saturation = 0;
	HI_U32 sns0_rotat = 0, sns1_rotat = 0;
	HI_U32 lcd_sel = 0;

	
	//char *func_str[STR_LEN] = "10";
	//char *satu_str[STR_LEN] = "0";
	//char *rota_str[STR_LEN] = "2";
	//char *lcd_str[STR_LEN]  = "mipi_7inch_1024_600";
	
	/*
	 * f for function, arg:1 2 3 4 5 6 7 8 9 10 11 12 mipi_tx mipi_tx_2s hdmi hdmi_2s. 2s for 2 sensor 
	 * s for saturation, arg:0 1 2 3. 0:default; 1:set sensor 0 saturation to 0; 2:set sensor 1 saturation to 0; 3:set sensor0 and sensor1 saturation to 0
	 * r for rotation, arg:0 1 2 3  0 for sensor0, 1 for 90 degree, 2 means rotate sensor1 180 degree
	 * R same as r , work for sensor1
	 * l for lcd selection, arg:mipi_7inch_1024_600 mipi_8inch_800_1280 
	 */
	while((ch=getopt(argc,argv,"f:s:r:R:l:t:T:h")) != -1)
	{
	    //printf("optind:%d\n",optind);
	    //printf("optarg:%s\n",optarg);
	    //printf("ch:%c\n",ch);
		
	    switch(ch)
	    {
	      case 'f':
			s32Index = atoi(optarg);
			printf("option f:'%s', s32Index = %d\n",optarg, s32Index);
	        break;
	
	      case 's':
		    saturation = atoi(optarg);
	        printf("option s:'%s' saturation = %d\n",optarg, saturation);
	        break;
	
	      case 'r':
		    sns0_rotat = atoi(optarg);
	        printf("option r:'%s', sensor0_ratation = %d\n",optarg, sns0_rotat);
	        break;
	      
		  case 'R':
		    sns1_rotat = atoi(optarg);
	        printf("option T:'%s', sensor1_ratation =%d\n",optarg, sns1_rotat);
	        break;
	
	      case 'l':
		    lcd_sel = atoi(optarg);
	        printf("option l:'%s', lcd_sel = %d\n",optarg, lcd_sel);
			switch(lcd_sel)
			{
				case 0:
					g_mipi_tx_config = &MIPI_TX_7INCH_1024X600_60_CONFIG;
					g_mipi_tx_sync_info = &MIPI_TX_7INCH_1024X600_60_SYNC_INFO;
					g_mipi_tx_user_intfsync_info = &MIPI_TX_7INCH_1024X600_60_USER_INTFSYNC_INFO;
					g_mipi_tx_lcd_resolution = &MIPI_TX_7INCH_1024X600_60_LCD_RESOLUTION;
					g_mipi_lcd_init = &InitScreen_mipi_7inch_1024x600;
					break;

				case 1:
					g_mipi_tx_config = &MIPI_TX_8INCH_800X1280_60_CONFIG;
					g_mipi_tx_sync_info = &MIPI_TX_8INCH_800X1280_60_SYNC_INFO;
					g_mipi_tx_user_intfsync_info = &MIPI_TX_8INCH_800X1280_60_USER_INTFSYNC_INFO;
					g_mipi_tx_lcd_resolution = &MIPI_TX_8INCH_800X1280_60_LCD_RESOLUTION;
					g_mipi_lcd_init = &InitScreen_mipi_8inch_800x1280;
					break;

				case 2:
					g_mipi_tx_config = &MIPI_TX_10INCH_800X1280_60_CONFIG;
					g_mipi_tx_sync_info = &MIPI_TX_10INCH_800X1280_60_SYNC_INFO;
					g_mipi_tx_user_intfsync_info = &MIPI_TX_10INCH_800X1280_60_USER_INTFSYNC_INFO;
					g_mipi_tx_lcd_resolution = &MIPI_TX_10INCH_800X1280_60_LCD_RESOLUTION;
					g_mipi_lcd_init = &InitScreen_mipi_10inch_800x1280;
					break;
				case 3:
					g_mipi_tx_config = &MIPI_TX_5INCH_720X1280_60_CONFIG;
					g_mipi_tx_sync_info = &MIPI_TX_5INCH_720X1280_60_SYNC_INFO;
					g_mipi_tx_user_intfsync_info = &MIPI_TX_5INCH_720X1280_60_USER_INTFSYNC_INFO;
					g_mipi_tx_lcd_resolution = &MIPI_TX_5INCH_720X1280_60_LCD_RESOLUTION;
					g_mipi_lcd_init = &InitScreen_mipi_5inch_720x1280;
					break;
				case 4:
					g_mipi_tx_config = &MIPI_TX_4INCH_480X800_60_CONFIG;
					g_mipi_tx_sync_info = &MIPI_TX_4INCH_480X800_60_SYNC_INFO;
					g_mipi_tx_user_intfsync_info = &MIPI_TX_4INCH_480X800_60_USER_INTFSYNC_INFO;
					g_mipi_tx_lcd_resolution = &MIPI_TX_4INCH_480X800_60_LCD_RESOLUTION;
					g_mipi_lcd_init = &InitScreen_mipi_4inch_480x800;
					break;
				case 5:
					g_mipi_tx_config = &MIPI_TX_5_5INCH_720X1280_60_CONFIG;
                                        g_mipi_tx_sync_info = &MIPI_TX_5_5INCH_720X1280_60_SYNC_INFO;
                                        g_mipi_tx_user_intfsync_info = &MIPI_TX_5_5INCH_720X1280_60_USER_INTFSYNC_INFO;
                                        g_mipi_tx_lcd_resolution = &MIPI_TX_5_5INCH_720X1280_60_LCD_RESOLUTION;
                                        g_mipi_lcd_init = &InitScreen_mipi_5_5inch_720x1280;
                                        break;
					
				default:
					g_mipi_tx_config = &MIPI_TX_8INCH_800X1280_60_CONFIG;
					g_mipi_tx_sync_info = &MIPI_TX_8INCH_800X1280_60_SYNC_INFO;
					g_mipi_tx_user_intfsync_info = &MIPI_TX_8INCH_800X1280_60_USER_INTFSYNC_INFO;
					g_mipi_tx_lcd_resolution = &MIPI_TX_8INCH_800X1280_60_LCD_RESOLUTION;
					g_mipi_lcd_init = &InitScreen_mipi_8inch_800x1280;
					//printf("wrong lcd optarg\n");
			}
	        break;

	      case 't':
		    g_sns_type[0] = atoi(optarg);
	        printf("option t:'%s', sns0_type = %d\n",optarg, g_sns_type[0]);
	        break;
	      
		  case 'T':
		    g_sns_type[1] = atoi(optarg);
	        printf("option T:'%s', sns1_type = %d\n",optarg, g_sns_type[1]);
	        break;
	
		  case 'h':
			SAMPLE_VIO_Usage(argv[0]);
			return 0;
			break;

	      default:
			SAMPLE_VIO_Usage(argv[0]);
			return 0;
	        //printf("other option:%c\n",ch);
	    }
	}

/*
    if (argc < 2 || argc > 2)
    {
        SAMPLE_VIO_Usage(argv[0]);
        return HI_FAILURE;
    }

    if (!strncmp(argv[1], "-h", 2))
    {
        SAMPLE_VIO_Usage(argv[0]);
        return HI_SUCCESS;
    }
*/
#ifdef __HuaweiLite__
#else
    signal(SIGINT, SAMPLE_VIO_HandleSig);
    signal(SIGTERM, SAMPLE_VIO_HandleSig);
#endif

    HI_MPI_SYS_GetChipId(&u32ChipId);

    if (HI3516C_V500 == u32ChipId)
    {
        u32VoIntfType = 1;
    }
    else
    {
        u32VoIntfType = 0;
    }

    SAMPLE_VIO_MsgInit();

//    s32Index = atoi(argv[1]);
    switch (s32Index)
    {
        case 0:
            s32Ret = SAMPLE_VIO_ViOnlineVpssOnlineRoute(u32VoIntfType);
            break;

        case 1:
            s32Ret = SAMPLE_VIO_WDR_LDC_DIS_SPREAD(u32VoIntfType);
            break;

        case 2:
            s32Ret = SAMPLE_VIO_ResoSwitch(u32VoIntfType);
            break;

        case 3:
            s32Ret = SAMPLE_VIO_ViVpssLowDelay(u32VoIntfType);
            break;

        case 4:
            s32Ret = SAMPLE_VIO_ViDoubleWdrPipe(u32VoIntfType);
            break;

        case 5:
            s32Ret = SAMPLE_VIO_FPN(u32VoIntfType);
            break;

        case 6:
            s32Ret = SAMPLE_VIO_ViWdrSwitch(u32VoIntfType);
            break;

        case 7:
            s32Ret = SAMPLE_VIO_Rotate(u32VoIntfType);
            break;

        case 8:
            s32Ret = SAMPLE_VIO_SetUsrPic(u32VoIntfType);
            break;
			
        case 9:
            s32Ret = SAMPLE_VIO_VPSS_VO_MIPI_TX(u32VoIntfType, saturation, sns0_rotat, lcd_sel);
            break;

        case 10:
            s32Ret = SAMPLE_VIO_VPSS_VO_MIPI_TX_DOUBLE_VI(u32VoIntfType, saturation, sns0_rotat, sns1_rotat, lcd_sel);
            break;

        case 11:
            s32Ret = SAMPLE_VIO_VPSS_VO_HDMI(u32VoIntfType, saturation, sns0_rotat);
            break;

        case 12:
            s32Ret = SAMPLE_VIO_VPSS_VO_HDMI_DOUBLE_VI(u32VoIntfType, saturation, sns0_rotat, sns1_rotat);
            break;

        default:
            SAMPLE_PRT("the index %d is invaild!\n",s32Index);
            SAMPLE_VIO_Usage(argv[0]);
            SAMPLE_VIO_MsgExit();
            return HI_FAILURE;
    }

    if (HI_SUCCESS == s32Ret)
    {
        SAMPLE_PRT("sample_vio exit success!\n");
    }
    else
    {
        SAMPLE_PRT("sample_vio exit abnormally!\n");
    }

    SAMPLE_VIO_MsgExit();

    return s32Ret;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
