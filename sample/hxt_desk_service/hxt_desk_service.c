
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
#include "report_info_db.h"

BOOL g_hxt_wbsc_running = TRUE;
BOOL g_iflyos_wbsc_running = TRUE;

// static pthread_t play_tid, voice_tid, video_tid;
static pid_t iflyos_pid = -1, hxt_pid = -1;
static BOOL g_processing = TRUE;


static void* hxt_websocket_cb(void* data)
{
    hxt_websocket_start();

    return NULL;
}

static void* iflyos_websocket_cb(void* data)
{
    iflyos_websocket_start();

    return NULL;
}

static void start_hxt_websocket_thread()
{
    pthread_t hxt_tid;
    if (hxt_get_desk_cfg_request())
    {
        pthread_create(&hxt_tid, NULL, hxt_websocket_cb, NULL);
    }
    
    return;
}

static void start_iflyos_websocket_thread()
{
    pthread_t iflyos_tid;
    if(hxt_get_iflyos_token_cfg() != NULL || hxt_get_iflyos_cae_sn() != NULL)
    {
        pthread_create(&iflyos_tid, NULL, iflyos_websocket_cb, NULL);
    }

    return;
}

static void check_websocket_running()
{
    while(g_processing)
    {
        if(!g_hxt_wbsc_running)
        {
            sleep(120);
            start_hxt_websocket_thread();
        }

        if(!g_iflyos_wbsc_running)
        {
            sleep(120);
           start_iflyos_websocket_thread();
        }
        
        sleep(10);
    }
}

static void handle_signal(int signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        g_processing = FALSE;
    }
}

static void  deploy_network()
{
    BOOL first_notice = TRUE;

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
    BOOL server_started = TRUE;

    utils_print("HXT V1.0.0\n");

    // signal(SIGCHLD, main_process_cycle);
#ifdef DEBUG
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);   
#endif


#if 1
    /* load config */
    hxt_load_cfg();

    /* init gpio */
    board_gpio_init();

    sleep(3);

    HI_MPI_AO_SetVolume(0, -35);
    sleep(1);
    utils_send_local_voice(VOICE_DEVICE_OPEN);

    board_stop_boot_led_blinking();

    deploy_network();

    /* check time by ntp */
    ntp_sync_time();

    /* to c onnect to mpp service */
    connect_to_mpp_service();


    /* connect to hxt server */
    int connect_count = 0;
    while(connect_count < 6)
    {
        server_started = hxt_refresh_token_request();
        if (server_started)
        {
            break;
        }
        connect_count++;
        sleep(10*connect_count);
    }

    if(server_started)
    {
        start_hxt_websocket_thread();
        start_iflyos_websocket_thread();

        check_websocket_running();
    }  

 EXIT:
    utils_print("~~~~EXIT~~~~\n");
    board_gpio_uninit();
    hxt_unload_cfg();
#endif
    

    return 0;
}
