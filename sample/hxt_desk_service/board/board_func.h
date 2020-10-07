#ifndef __BOARD_FUNC_H__
#define __BOARD_FUNC_H__

#include "common.h"

int board_gpio_init();
void board_gpio_uninit();
void board_set_led_status(LED_STATUS status);
BOOL board_get_qrcode_scan_status();

void board_get_yuv_from_vpss_chn(char** yuv_buf);
BOOL board_get_qrcode_yuv_buffer();

char* board_get_sn();


#endif //!__BOARD_FUNC_H__