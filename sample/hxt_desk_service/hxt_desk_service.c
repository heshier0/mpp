
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/prctl.h>

#include <sitting_posture.h>

#include "utils.h"
#include "common.h"
#include "server_comm.h"
#include "databuffer.h"
#include "db.h"
#include "hxt_client.h"
#include "board_func.h"
#include "posture_check.h"

volatile BOOL g_hxt_wbsc_running = FALSE;
volatile BOOL g_iflyos_wbsc_running = FALSE;
volatile BOOL g_device_sleeping = FALSE;
volatile BOOL g_iflyos_first_login = TRUE;
volatile BOOL g_hxt_first_login = TRUE;


DATABUFFER g_msg_buffer;
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
    char *token = get_iflyos_token();
    char *sn = get_iflyos_sn();
    if (token != NULL && sn != NULL)
    {
        pthread_create(&iflyos_tid, NULL, iflyos_websocket_cb, NULL);

        utils_free(token);
        utils_free(sn);
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
        if (end - begin >= 20)
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
        sleep(30);
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
    while (g_processing)
    {
        char* ssid = get_wifi_ssid();
        char* pwd = get_wifi_pwd();
        if(ssid == NULL || pwd == NULL)
        {
            /* step into qrcode scan */
            utils_print("To wait scan wifi info....\n");
            sleep(3);
            continue;
        }
        
        utils_send_local_voice(VOICE_QUERY_WIFI_INFO);
        /* to connect wifi */
        if (!utils_check_wifi_state())
        {
            utils_link_wifi(ssid, pwd);
            sleep(10);
        }
        utils_free(ssid);
        utils_free(pwd); 

        /*link ok, play voice*/
        if (utils_check_wifi_state())
        {
            //device already bind
            if (get_desk_bind_status() == 1)
            {
                break;
            }
            
            while(1)
            {
                utils_send_local_voice(VOICE_SERVER_CONNECT_OK);//联网成功，正在绑定
                if(!board_get_qrcode_scan_status())
                {
                    if (hxt_bind_desk_with_wifi_request())
                    {
                        if (hxt_confirm_desk_bind_request())
                        {
                            set_desk_bind_status(1);
                            break;
                        }
                    }
                    sleep(5);
                }
            }
            break;
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
    create_buffer(&g_msg_buffer, 16*1024);
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

        init_posture_model();

        check_websocket_running();
    }  

 EXIT:
    board_gpio_uninit();
    destroy_buffer(&g_msg_buffer);
    deinit_posture_model();
#endif
    close_hxt_service_db();

    utils_print("~~~~EXIT~~~~\n");

    return 0;
}
