
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <sitting_posture.h>

#include "yuv2mp4.h"
#include "utils.h"
#include "common.h"

#include "file_utils.h"

static pthread_t play_tid, voice_tid, video_tid;

static void handle_signal(int signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        g_play_status = 0;
        g_voice_status = 0;
        g_video_status = 0;

        stop_video_recording();
   }
}


int main(int argc, char **argv)
{
    int st1, st2, st3;
    BOOL server_started = TRUE;
    BOOL wifi_exist = FALSE;
    utils_print("HXT V1.0.0\n");

#ifdef DEBUG
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);   
#endif

    /* init gpio */
    // board_gpio_init();

    /* load config */
    hxt_load_cfg();

    /* init board */
    if (!board_mpp_init())
    {
        utils_print("board mpp init error...\n");
        goto EXIT;
    }

    g_play_status = 1;
    play_tid = start_play_mp3();
    g_voice_status = 1;
    voice_tid = start_sample_voice();
    g_video_status = 1;
    video_tid = start_sample_video();

    usleep(500);
    utils_send_local_voice(VOICE_DEVICE_OPEN);
    usleep(500);


    // /* QRcode parse */
    // // wifi_exist = (hxt_get_wifi_ssid_cfg() != NULL);
#if 1    
    int retry_count = 5;
    while (retry_count > 0)
    {
       
        printf("To scan QRcode....\n");    
        if (!get_qrcode_yuv_buffer())
        {
            utils_send_local_voice(VOICE_CONNECT_ERROR);
            retry_count --;
            sleep(3);
            continue;
        }
        retry_count = 0;
    }

    // /* connect to wifi */
    utils_link_wifi(hxt_get_wifi_ssid_cfg(), hxt_get_wifi_pwd_cfg());
#endif   

#if 0
    /* connect to hxt server */
    server_started = hxt_check_token();
    
    if (server_started)
    {
        pid_t hxt_pid = fork();
        if (hxt_pid == 0)
        {
            hxt_websocket_start();
            return 0;
        }

        // start_posture_recognize();

        // pid_t iflyos_pid = fork();
        // if (iflyos_pid == 0)
        // {
        //     iflyos_websocket_start();
        //     return 0;
        // }

        waitpid(hxt_pid, &st1, 0);
        // waitpid(iflyos_pid, &st2, 0);

        utils_print("child process exit...\n");
    }
#endif

    pthread_join(play_tid, NULL);
    pthread_join(voice_tid, NULL);
    pthread_join(video_tid, NULL);

    board_mpp_deinit();


EXIT:
    utils_print("~~~~EXIT~~~~\n");
    hxt_unload_cfg();

    // board_gpio_uninit();

    return 0;
}
