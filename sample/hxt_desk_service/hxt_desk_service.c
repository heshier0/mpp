
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <sitting_posture.h>

#include "utils.h"
#include "databuffer.h"
#include "common.h"

static pthread_t play_tid, voice_tid, video_tid;

static void handle_signal(int signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        g_play_status = 0;
        g_voice_status = 0;
        g_video_status = 0;
   }
}

int main(int argc, char **argv)
{
    int st1, st2, st3;
    BOOL server_started = TRUE;
    BOOL wifi_exist = FALSE;
    utils_print("HXT V1.0.0\n");

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);   

    /* init gpio */
    board_gpio_init();

    /* load config */
    hxt_load_cfg();

    /* init board */
    board_mpp_init();

    g_video_status = 1;
    video_tid = start_sample_video();
    g_play_status = 1;
    play_tid = start_play_mp3();
    g_voice_status = 1;
    voice_tid = start_sample_voice();

    usleep(500);
    utils_send_local_voice(VOICE_DEVICE_OPEN);
    usleep(500);


    // /* QRcode parse */
    // // wifi_exist = (hxt_get_wifi_ssid_cfg() != NULL);
    // while (0)
    // {
        char* qrcode_info = NULL;
        utils_print("To scan QRcode....\n");    
        if (!get_qrcode_yuv_buffer(&qrcode_info))
        {
            utils_send_local_voice(VOICE_CONNECT_ERROR);
        }
        utils_print("qrcode: [%s]\n", qrcode_info);
    //     if (!hxt_query_wifi_info(qrcode_info))
    //     {
    //         utils_send_local_voice(VOICE_CONNECT_ERROR);
    //         sleep(10);
    //         continue;
    //     }
    //     if(qrcode_info != NULL)
    //     {
    //         free(qrcode_info);　
    //         qrcode_info = NULL;
    //     }
    //     wifi_exist = TRUE;
    // }
    // /* connect to wifi */
    // utils_link_wifi(hxt_get_wifi_ssid_cfg(), hxt_get_wifi_pwd_cfg());

    /* connect to hxt server */
    // if (hxt_get_token_cfg() == NULL)
    // {
    //     utils_print("send request to get token\n");
    //     server_started = hxt_get_token_request();
    // }
    
    // if (server_started)
    // {
    //     pid_t hxt_pid = fork();
    //     if (hxt_pid == 0)
    //     {
    //         hxt_websocket_start();
    //         return 0;
    //     }

    //     pid_t iflyos_pid = fork();
    //     if (iflyos_pid == 0)
    //     {
    //         iflyos_websocket_start();
    //         return 0;
    //     }

    //     waitpid(hxt_pid, &st1, 0);
    //     waitpid(iflyos_pid, &st2, 0);
    // }

    // pthread_join(play_tid, NULL);
    // pthread_join(voice_tid, NULL);
    pthread_join(video_tid, NULL);

    utils_print("~~~~EXIT~~~~\n");

    hxt_unload_cfg();
    board_mpp_deinit();
    board_gpio_uninit();

    return 0;
}
