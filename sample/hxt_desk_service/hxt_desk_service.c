
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <sitting_posture.h>

#include "utils.h"
#include "common.h"
#include "server_comm.h"

static pthread_t play_tid, voice_tid, video_tid;
static pid_t iflyos_pid = -1, hxt_pid = -1;
static BOOL g_processing = TRUE;

static int g_client_fd = -1;



static void start_hxt_process()
{
    hxt_pid = fork();
    if (hxt_pid == 0)
    {
        hxt_websocket_start();
        return 0;
    }
}

static void start_iflyos_process()
{
    iflyos_pid = fork();
    if (iflyos_pid == 0)
    {
        iflyos_websocket_start();
        return 0;
    }
}

static void main_process_cycle()
{
    pid_t child;
    while(g_processing)
    {
        child = waitpid(hxt_pid, NULL, WNOHANG);
        if (child == hxt_pid)
        {
            sleep(120);
            utils_print("hxt child process exit, restart it\n");
            start_hxt_process();
        }
        
        child = waitpid(iflyos_pid, NULL, WNOHANG);
        if (child == iflyos_pid)
        {
            sleep(120);
            utils_print("iflyos child process exit, restart it\n");
            start_iflyos_process();
        }

        sleep(5);
    }
}

static void handle_signal(int signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        g_processing = FALSE;

        // g_play_status = 0;
        // g_voice_status = 0;
        // g_video_status = 0;
   }
}

static void  deploy_network()
{
    BOOL first_notice = TRUE;

    // utils_disconnect_wifi();
    /* check if wifi info in cfg */
    while (1)
    {
        while (hxt_get_wifi_ssid_cfg() == NULL || strlen(hxt_get_wifi_ssid_cfg()) == 0)
        {
            /* step into qrcode scan */
            sleep(3);
            // utils_send_local_voice(VOICE_SCAN_QRCODE);
            continue;
        }

        /* to connect wifi */
        if (!utils_check_wifi_state())
        {
            utils_link_wifi(hxt_get_wifi_ssid_cfg(), hxt_get_wifi_pwd_cfg());
        }
        
        sleep(5);
        
        /*link ok, play voice*/
        if (utils_check_wifi_state())
        {
            //check desk bind status 
            if(hxt_get_desk_bind_status_cfg() == 1)
            {
                break;
            }
            else
            {
                if(!board_get_qrcode_scan_status())
                {
                    if (hxt_bind_desk_with_wifi_request())
                    {
                        board_stop_connect_led_blinking();

                        hxt_confirm_desk_bind_request();

                        utils_send_local_voice(VOICE_WIFI_BIND_OK);
                        hxt_set_desk_bind_status_cfg(1);

                        break;
                    }
                    else
                    {
                        utils_send_local_voice(VOICE_WIFI_BIND_ERROR);
                        sleep(5);
                    }
                }
            }
        }
        else
        {
            utils_disconnect_wifi();
        }
    }
    
    return;
}

int main(int argc, char **argv)
{
    int st1, st2, st3;
    BOOL server_started = TRUE;
    BOOL config_get = FALSE;
    BOOL wifi_exist = FALSE;
    utils_print("HXT V1.0.0\n");

    // signal(SIGCHLD, child_process_signal_handle);
#ifdef DEBUG
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);   
#endif

    /* load config */
    hxt_load_cfg();

    /* init gpio */
    board_gpio_init();

    /* init board media process */
    // if (!board_mpp_init())
    // {
    //     utils_print("board mpp init error...\n");
    //     goto EXIT;
    // }

    // g_play_status = TRUE;
    // play_tid = start_play_mp3();
    // g_voice_status = TRUE;
    // voice_tid = start_sample_voice();
    // g_video_status = TRUE;
    // video_tid = start_sample_video();

    sleep(3);

    utils_send_local_voice(VOICE_DEVICE_OPEN);

    board_stop_boot_led_blinking();

    // deploy_network();


    connect_to_mpp_service();
#if 1
    /* connect to hxt server */
    int connect_count = 1;
    while(1)
    {
        server_started = hxt_refresh_token_request();
        if (server_started)
        {
            break;
        }
        sleep(10*connect_count);
    }
    if(server_started)
    {
        config_get = hxt_get_desk_cfg_request();
        if (config_get)
        {
            start_hxt_process();
        }
        start_iflyos_process();

        main_process_cycle();
    }  
#endif
    // while(g_processing)
    // {
    //     sleep(5);
    // }

    
    // g_play_status = FALSE;
    // g_voice_status = FALSE;
    // g_video_status = FALSE;

    // board_mpp_deinit();
EXIT:
    utils_print("~~~~EXIT~~~~\n");
    board_gpio_uninit();
    hxt_unload_cfg();

    

    return 0;
}
