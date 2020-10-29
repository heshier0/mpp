
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/prctl.h>
#include <semaphore.h>

#include <sitting_posture.h>

#include "utils.h"
#include "common.h"
#include "server_comm.h"
#include "databuffer.h"
#include "db.h"
#include "hxt_client.h"
#include "board_func.h"
#include "posture_check.h"
#include "iflyos_func.h"

volatile BOOL g_hxt_wbsc_running = FALSE;
volatile BOOL g_iflyos_wbsc_running = FALSE;
volatile BOOL g_deploying_net = FALSE;
volatile BOOL g_iflyos_first_login = TRUE;
volatile BOOL g_linking_wifi = TRUE;
volatile BOOL g_hxt_first_login = TRUE;
volatile BOOL g_device_sleeping = FALSE;
volatile int g_connect_count = 0;

DATABUFFER g_msg_buffer;
static BOOL g_processing = TRUE;

static void* hxt_websocket_cb(void* data)
{
    hxt_websocket_start();
    return NULL;
}

static void start_hxt_websocket_thread()
{
    pthread_t hxt_tid;
    // if (hxt_get_desk_cfg_request())
    // {
        pthread_create(&hxt_tid, NULL, hxt_websocket_cb, NULL);
    // }
    
    return;
}

static void handle_signal(int signo)
{
    if (SIGPIPE == signo)
    {
        utils_print("HxtDeskService PIPE BREAK!!!\n");
    }

    if (SIGINT == signo || SIGTERM == signo)
    {
        g_processing = FALSE;
    }
}

int main(int argc, char **argv)
{
    utils_print("%s\n", HXT_DESK_SERVICE_VERSION);
    
    BOOL server_started = TRUE;
    BOOL first_start = TRUE;
    
#ifdef DEBUG
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);   
    signal(SIGPIPE, handle_signal);
#endif
    open_hxt_service_db();

    create_buffer(&g_msg_buffer, 16*1024);
    
    /* init gpio */
    board_gpio_init();
    board_init_volume();
    utils_send_local_voice(VOICE_DEVICE_OPEN);
    board_set_led_status(NO_BIND);
    
    while(g_processing)
    {
        //????
        if (g_device_sleeping)
        {
            utils_print("sleeping...\n");
            sleep(120);
            continue;
        }
        /*check wifi*/
        char* ssid = get_wifi_ssid();
        char* pwd = get_wifi_pwd();
        if(ssid == NULL || pwd == NULL)
        {
            /* step into qrcode scan */
            utils_print("To wait scan wifi info....\n");
            sleep(3);
            continue;
        }
        
        /* to connect wifi */
        if (!utils_check_wifi_state())
        {
            if (g_connect_count < 3)
            {
                utils_send_local_voice(VOICE_QUERY_WIFI_INFO); //正在联网
            }
            utils_link_wifi(ssid, pwd);
            sleep(10);
            g_connect_count ++;
        }
        utils_free(ssid);
        utils_free(pwd); 

        if (!utils_check_wifi_state())
        {
            if(g_connect_count < 3)
            {
                utils_disconnect_wifi();
                sleep(5);
                continue;
            }
            if (!g_deploying_net)
            {
                utils_print("wifi not link...\n");
                board_set_led_status(WIFI_ERR);
            }
    
            continue;
        }
        g_connect_count = 0; 
        
        if (first_start)
        {
            /* sync time to ntp.ntsc.ac.cn */
            ntp_sync_time();
            /* to c onnect to mpp service */
            connect_to_mpp_service();
            /*load posture check pattern*/
            init_posture_model();
            first_start = FALSE;
        }

        /*check bind*/
        if (get_desk_bind_status() != 1)
        {
            if (hxt_bind_desk_with_wifi_request())
            {
                if (hxt_confirm_desk_bind_request())
                {
                    set_desk_bind_status(1);
                }
            }
        }

        server_started = hxt_refresh_token_request();       
        if (server_started)
        {
            if (get_desk_bind_status() != 1)
            {
                set_desk_bind_status(1);
            }

            if(!g_hxt_wbsc_running)
            {
                start_hxt_websocket_thread();
            }
            else
            {
               board_set_led_status(NORMAL);
            }
        
            if(!g_device_sleeping && g_hxt_wbsc_running && !g_iflyos_wbsc_running)
            {
                start_iflyos_websocket_thread();
            }       
        }

        sleep(20);
    }

    board_gpio_uninit();
    destroy_buffer(&g_msg_buffer);
    deinit_posture_model();
    close_hxt_service_db();

    utils_print("~~~~EXIT~~~~\n");

    return 0;
}
