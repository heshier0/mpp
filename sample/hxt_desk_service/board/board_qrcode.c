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

#include "common.h"
#include "utils.h"


BOOL get_qrcode_yuv_buffer()
{
    char *qrcode_info = NULL;
    char *yuv_data = NULL;
    int width = hxt_get_video_height_cfg();
    int height = hxt_get_video_width_cfg();
    int size = width * height * 3 / 2;
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

    /* write wifi info into hxt config file */
    cJSON* root = cJSON_Parse(qrcode_info);
    if (root == NULL)
    {
        utils_print("parse qrcode info to json failed\n");
        return FALSE;
    }
    cJSON *item = cJSON_GetObjectItem(root, "ssid");
    char* wifi_ssid = item->valuestring;
    utils_print("ssid:%s\n", wifi_ssid);
    hxt_set_wifi_ssid_cfg(wifi_ssid);
    item = cJSON_GetObjectItem(root, "password");
    char* wifi_password = item->valuestring;
    utils_print("password:%s\n", wifi_password);
    hxt_set_wifi_pwd_cfg(wifi_password);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }


    return TRUE;
}

