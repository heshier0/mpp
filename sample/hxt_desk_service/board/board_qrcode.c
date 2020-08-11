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

BOOL get_qrcode_yuv_buffer(void **qrcode_info)
{
    char *yuv_data = NULL;
    int width = hxt_get_video_height_cfg();
    int height = hxt_get_video_width_cfg();
    int size = width * height * 3 / 2;
    board_get_yuv_from_vpss_chn(yuv_data);
    if(NULL == yuv_data)
    {
        return FALSE;
    }
    
    //for test, write buffer to file
 #ifdef DEBUG   
    FILE* pfd = fopen("qrcode_vpss.yuv", "wb");
    fwrite(yuv_data, size, 1, pfd);
    fflush(pfd);
    fclose(pfd);
 #endif 

    int ret = QRcode_Detection(yuv_data, width, height, qrcode_info);
    if (ret == -1)
    {
        utils_print("QRCode not recognize...\n");
        return FALSE;
    }

    if(yuv_data != NULL)
    {
        utils_free(yuv_data);
        yuv_data = NULL;
    }

    return TRUE;
}
