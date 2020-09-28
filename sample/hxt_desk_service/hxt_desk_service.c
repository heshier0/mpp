
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

volatile BOOL g_hxt_wbsc_running = FALSE;
volatile BOOL g_iflyos_wbsc_running = FALSE;
volatile BOOL g_device_sleeping = FALSE;
volatile BOOL g_iflyos_first_login = TRUE;
volatile BOOL g_hxt_first_login = TRUE;


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
    if (!g_hxt_wbsc_running)
    {
        return;
    }

    pthread_t iflyos_tid;
    if(hxt_get_iflyos_token_cfg() != NULL && hxt_get_iflyos_cae_sn() != NULL)
    {
        pthread_create(&iflyos_tid, NULL, iflyos_websocket_cb, NULL);
    }

    return;
}

static void check_websocket_running()
{
    time_t begin, end;
    begin = time(NULL);
    while(g_processing)
    {
        end = time(NULL);
        if (end - begin >= 10)
        {
            if(!g_hxt_wbsc_running)
            {
                start_hxt_websocket_thread();
            }

            if(!g_iflyos_wbsc_running)
            {
                start_iflyos_websocket_thread();
            }
        }
        sleep(20);
    }
}

static void handle_signal(int signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        g_processing = FALSE;
    }
}

static void  hxt_bind_user()
{
    BOOL first_notice = TRUE;

    /* check if wifi info in cfg */
    while (1)
    {
        while (hxt_get_wifi_ssid_cfg() == NULL || strlen(hxt_get_wifi_ssid_cfg()) == 0)
        {
            /* step into qrcode scan */
            sleep(3);
            continue;
        }
        utils_send_local_voice(VOICE_QUERY_WIFI_INFO);
        /* to connect wifi */
        if (!utils_check_wifi_state())
        {
            utils_link_wifi(hxt_get_wifi_ssid_cfg(), hxt_get_wifi_pwd_cfg());
            sleep(10);
        }
        
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
                utils_send_local_voice(VOICE_SERVER_CONNECT_OK);
                if(!board_get_qrcode_scan_status())
                {
                    if (hxt_bind_desk_with_wifi_request())
                    {
                        if (hxt_confirm_desk_bind_request())
                        {
                            hxt_set_desk_bind_status_cfg(1);
                            break;
                        }
                    }
                    sleep(5);
                }
            }
        }
        else
        {
            utils_disconnect_wifi();
        }
    }
    board_set_led_status(NORMAL);
    return;
}

int main(int argc, char **argv)
{
    utils_print("%s\n", HXT_DESK_SERVICE_VERSION);
    
    BOOL server_started = TRUE;

#ifdef DEBUG
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);   
#endif
    
    open_hxt_service_db();
    
#if 1
    /* load config */
    hxt_load_cfg();

    /* init gpio */
    board_gpio_init();

    HI_MPI_AO_SetVolume(0, -35);
    sleep(1);
    utils_send_local_voice(VOICE_DEVICE_OPEN);
    board_set_led_status(NO_BIND);
    sleep(5);
    /* bind user */
    hxt_bind_user();

    /* sync time to ntp.ntsc.ac.cn */
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
    board_gpio_uninit();
    hxt_unload_cfg();
    
#endif
    close_hxt_service_db();
    utils_print("~~~~EXIT~~~~\n");

    return 0;
}
