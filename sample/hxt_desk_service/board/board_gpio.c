#include <sys/prctl.h>
#include <gpiod.h>
#include <hi_comm_aio.h>
#include <semaphore.h>

#include "utils.h"
#include "posture_check.h"
#include "server_comm.h"
#include "board_func.h"
#include "db.h"


#define FLASH_TIMES         3
#define LED_SLEEP_TIME      (200*1000)

/*
** LED状态 **
-----           ---------           ---------          ------ 
                 GPIO0_1              gpio01            红色 
                 GPIO9_2              gpio74            绿色    
                 GPIO2_4              gpio20            蓝色

                 GPIO2_5              gpio21            蓝色 
                 GPIO2_6              gpio22            红色 
                 GPIO2_7              gpio23            绿色

                 GPIO0_5              gpio05            蓝色 
                 GPIO9_4              gpio76            红色                 
                 GPIO10_1             gpio81            绿色                  
*/

/*
** 按键 **
                                      gpio66            vol-
                                      gpio68            vol+
                                      gpio86            mute
                                      gpio47            reset
*/

static struct gpiod_chip *gpio0 = NULL;
static struct gpiod_chip *gpio2 = NULL;
static struct gpiod_chip *gpio5 = NULL;
static struct gpiod_chip *gpio8 = NULL;
static struct gpiod_chip *gpio9 = NULL;
static struct gpiod_chip *gpio10 = NULL;

static struct gpiod_line *btn_vol_inc = NULL;
static struct gpiod_line *btn_vol_dec = NULL;
static struct gpiod_line *btn_mute = NULL;
static struct gpiod_line *btn_reset = NULL;

static struct gpiod_line *led_camera_green = NULL;
static struct gpiod_line *led_camera_blue = NULL;
static struct gpiod_line *led_camera_red = NULL;
static struct gpiod_line *led_wifi_green = NULL;
static struct gpiod_line *led_wifi_blue = NULL;
static struct gpiod_line *led_wifi_red = NULL;
static struct gpiod_line *led_mute_green = NULL;
static struct gpiod_line *led_mute_blue = NULL;
static struct gpiod_line *led_mute_red = NULL;

static AUDIO_DEV ao_dev = 0;
static BOOL dec_vol_pressed = FALSE;
static BOOL inc_vol_pressed = FALSE;
static int dec_vol_pressed_intv = 0;
static int inc_vol_pressed_intv = 0;

static BOOL ao_mute = FALSE;
static int scan_count = 0;
static LED_STATUS led_status = BOOTING;

extern BOOL g_deploying_net;
extern BOOL g_posture_running;
extern sem_t g_bind_sem;
extern BOOL g_device_sleeping;


static int gpio_set_value(struct gpiod_line *line, int value)
{
    int ret = 0;
    gpiod_line_set_value(line, value);

    return ret;
}

static int gpio_get_value(struct gpiod_line *line)
{
    return gpiod_line_get_value(line);
}

/* led status func */
static void set_camera_led_off()
{
    gpio_set_value(led_camera_green, 1);
    gpio_set_value(led_camera_blue, 1);
    gpio_set_value(led_camera_red, 1);
}

static void set_wifi_led_off()
{
    gpio_set_value(led_wifi_blue, 1);
    gpio_set_value(led_wifi_red, 1);
    gpio_set_value(led_wifi_green, 1);
}

static void clear_wifi_info()
{
    utils_disconnect_wifi();
    system("rm /userdata/config/wifi.conf");
    deinit_wifi_params();
}

static void board_led_all_off()
{
    set_camera_led_off();

    set_wifi_led_off();

    gpio_set_value(led_mute_green, 1);
    gpio_set_value(led_mute_blue, 1);
    gpio_set_value(led_mute_red, 1);
}

static void* check_reset_event(void *param)
{
    struct gpiod_line_event event;
    time_t start = 0, end = 0;
    BOOL pressed = FALSE;

    prctl(PR_SET_NAME, "check_reset_event");
    while(1)
    {
        gpiod_line_event_wait(btn_reset, NULL);
        if (gpiod_line_event_read(btn_reset, &event) != 0)
        {
            continue;
        }

        if(event.event_type == GPIOD_LINE_EVENT_RISING_EDGE)
        {
            if (!pressed)
            {
                utils_print("reset btn not pressed\n");
                continue;
            }
            printf("reset btn pressed up\n");
            end = time(0);
            printf("inteval is %ld\n", end - start);
            if (end - start > 3)
            {
                board_set_led_status(RESETING);
                sleep(3);
                /* reset */
                utils_system_reset();
                utils_system_reboot();
            }
            else
            {
                board_led_all_off();
                /* reboot */
                utils_print("Btn pressed to reboot\n");
                utils_system_reboot();
            }
        }
        if(event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE)
        {
            printf("reset btn pressed down\n");
            start = time(0);
            pressed = TRUE;
        }
    }

    return;
}

static void* check_inc_vol_event(void *param)
{
    struct gpiod_line_event event;
    time_t start = 0, end = 0;
    BOOL already_max = FALSE;

    prctl(PR_SET_NAME, "check_inc_volume");

    while(1)
     {
        gpiod_line_event_wait(btn_vol_inc, NULL);
        if (gpiod_line_event_read(btn_vol_inc, &event) != 0)
        {
            continue;
        }

        if(event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE)
        {
            printf("inc vol btn pressed down\n");
            start = time(0);
        }    

        if(event.event_type == GPIOD_LINE_EVENT_RISING_EDGE)
        {
            printf("inc vol btn pressed up\n");
            end = time(0);
            printf("inteval is %ld\n", end - start);
            if (end - start >= 5)
            {
                inc_vol_pressed = TRUE;
                inc_vol_pressed_intv = time(NULL);
            }
            else
            {
                int vol = board_inc_volume();
                utils_print("Now volume is %d\n", vol);
            }
        }

     }
    return NULL;
}

static void* check_dec_vol_event(void *param)
{
    struct gpiod_line_event event;
    time_t start = 0, end = 0;
    prctl(PR_SET_NAME, "check_dec_volume");
    while(1)
     {
        gpiod_line_event_wait(btn_vol_dec, NULL);
        if (gpiod_line_event_read(btn_vol_dec, &event) != 0)
        {
            continue;
        }
        if(event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE)
        {
            printf("dec vol btn pressed down\n");
            start = time(0);
        }

        if(event.event_type == GPIOD_LINE_EVENT_RISING_EDGE)
        {
            printf("dec vol btn press up\n");
            end = time(0);
            printf("inteval is %ld\n", end - start);
            if (end - start >= 5)
            {
                dec_vol_pressed = TRUE;
                dec_vol_pressed_intv = time(NULL);
            }
            else
            {
                int vol = board_dec_volume();
                utils_print("Now volume is %d\n", vol);
            }
        }
     }
    return NULL;
}

static void* check_posture_event(void *param)
{
    struct gpiod_line_event event;
    time_t start = 0, end = 0;
    time_t begin_time = 0;
    prctl(PR_SET_NAME, "check_posture");
    while(1)
    {
        gpiod_line_event_wait(btn_mute, NULL);
        if (gpiod_line_event_read(btn_mute, &event) != 0)
        {
            continue;
        }
        
        if (event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE)
        {
            start = time(NULL);
            utils_print("camera btn pressed down\n");
        }

        if(event.event_type == GPIOD_LINE_EVENT_RISING_EDGE)
        {
            utils_print("camera btn pressed up\n");
            end = time(NULL); 
            utils_print("inteval: %ld - %ld = %ld\n", end, start, (end-start));

            if(g_deploying_net)
            {
                utils_print("deploying network....\n");
                continue;
            }

            if (end - start >= 5)
            {
                if (g_device_sleeping)      //device sleeping
                {
                    g_device_sleeping = FALSE;
                    iflyos_websocket_start();
                    board_set_led_status(NORMAL);
                }
                else
                {
                    g_device_sleeping = TRUE;
                    /*stop iflyos voice*/
                    iflyos_websocket_stop();
                    if (g_posture_running)
                    {
                        stop_posture_recognize();
                    }
                    board_set_led_status(SLEEPING);
                }    
            }
            else
            {
                if (g_device_sleeping)
                {
                    continue;
                }

                if(!g_posture_running)
                {
                    start_posture_recognize();
                    begin_time = time(NULL);
                }
                else
                {
                    stop_posture_recognize();   
                }
            }
        }
    }
    return NULL;
}

static void* check_scan_qrcode_event(void *param)
{
    prctl(PR_SET_NAME, "scan_qrcode");
    while(1)
    {
        if (inc_vol_pressed && dec_vol_pressed)
        {
            if (abs(inc_vol_pressed_intv - dec_vol_pressed_intv) >= 0 && 
                    abs(inc_vol_pressed_intv - dec_vol_pressed_intv) < 5) //check btn if pressed meantime
            {

                clear_wifi_info();

                if(g_posture_running)
                {
                    stop_posture_recognize();
                }
                
                g_deploying_net = TRUE;
                board_set_led_status(BINDING);

                printf("To scan qrcode....\n");
                inc_vol_pressed = dec_vol_pressed = FALSE;
                inc_vol_pressed_intv = dec_vol_pressed_intv = 0;
                
                /*qrcode recognize*/
                while (!board_get_qrcode_yuv_buffer())
                {
                    if (scan_count < 5)
                    {
                        utils_send_local_voice(VOICE_DEPLOYING_NET);
                        sleep(10);
                        scan_count ++;
                        continue;
                    }         
                    else
                    {
                        utils_send_local_voice(VOICE_QUERY_WIFI_ERROR);
                        board_set_led_status(WIFI_ERR);
                        sleep(10);
                        break;
                    }
                }
                char* ssid = get_wifi_ssid();
                char* pwd = get_wifi_pwd();
                utils_link_wifi(ssid, pwd);
                sleep(3);
                g_deploying_net = FALSE;
                scan_count = 0;

                if (ssid != NULL)
                {
                    utils_free(ssid);
                }

                if(pwd != NULL)
                {
                    utils_free(pwd);
                }

                sem_post(&g_bind_sem);
            }
        }
       
        sleep(3);
    }

    return NULL;
}

static void* led_blinking_thread(void* param)
{
    BOOL changed_to_normal = FALSE;
    prctl(PR_SET_NAME, "check_led_status");
    while(1)
    {
        time_t t = time(0);
        switch(led_status)
        {
        case BOOTING:
            gpio_set_value(led_camera_blue, 0);
            gpio_set_value(led_wifi_blue, 0);
            usleep(200 * 1000);
            gpio_set_value(led_camera_blue, 1);
            gpio_set_value(led_wifi_blue, 1);
            usleep(200 * 1000);
        break;
        case NO_BIND:
            set_camera_led_off();
            gpio_set_value(led_wifi_blue, 0);
        break;
        case BINDING:
            set_wifi_led_off();
            gpio_set_value(led_wifi_blue, 1);
            gpio_set_value(led_wifi_green, 1);
            usleep(200 * 1000);
            gpio_set_value(led_wifi_green, 0);
            usleep(200 * 1000);
            gpio_set_value(led_camera_green, 0);
        break;
        case CHECKING:
            gpio_set_value(led_camera_green, 0);
            gpio_set_value(led_wifi_green, 0);
        break;
        case WIFI_ERR:
            gpio_set_value(led_wifi_red, 0);
            set_camera_led_off();
        break;
        case CAMERA_ERR:
            gpio_set_value(led_camera_red, 0);
            set_camera_led_off();
        break;
        case RESETING:
            board_led_all_off();
            while((time(0) - t) <= 2)
            {
                gpio_set_value(led_wifi_green, 0);
                gpio_set_value(led_camera_green, 0);
                usleep(100 * 1000);
                gpio_set_value(led_wifi_green, 1);
                gpio_set_value(led_camera_green, 1);
                usleep(100 * 1000);
            }
            board_led_all_off();
        break;
        case SLEEPING:
            board_led_all_off();
        break;
        case NORMAL:
        default:    
            set_camera_led_off();
            if (!changed_to_normal)
            {
                set_wifi_led_off();
                changed_to_normal = TRUE;
            }
            gpio_set_value(led_wifi_green, 0);
        break;
        }
    }

    return NULL;
}

static void board_listen_button_event()
{
    pthread_t t1, t2, t3, t4, t5;
    pthread_create(&t1, NULL, check_inc_vol_event, NULL);
    pthread_create(&t2, NULL, check_dec_vol_event, NULL);
    pthread_create(&t3, NULL, check_scan_qrcode_event, NULL);
    pthread_create(&t4, NULL, check_posture_event, NULL);
    pthread_create(&t5, NULL, check_reset_event, NULL);
}

static void init_wifi_led()
{
    led_wifi_red = gpiod_chip_get_line(gpio0, 1);
    gpiod_line_request_output(led_wifi_red, "HxtDeskService", GPIOD_LINE_ACTIVE_STATE_HIGH);
    led_wifi_blue = gpiod_chip_get_line(gpio2, 4);
    gpiod_line_request_output(led_wifi_blue, "HxtDeskService", GPIOD_LINE_ACTIVE_STATE_HIGH);
    led_wifi_green = gpiod_chip_get_line(gpio9, 2);
    gpiod_line_request_output(led_wifi_green, "HxtDeskService", GPIOD_LINE_ACTIVE_STATE_HIGH);

    return;
}

static void init_camera_led()
{
    led_camera_blue = gpiod_chip_get_line(gpio2, 5);
    gpiod_line_request_output(led_camera_blue, "HxtDeskService", GPIOD_LINE_ACTIVE_STATE_HIGH);
    led_camera_red = gpiod_chip_get_line(gpio2, 6);
    gpiod_line_request_output(led_camera_red, "HxtDeskService", GPIOD_LINE_ACTIVE_STATE_HIGH);
    led_camera_green = gpiod_chip_get_line(gpio2, 7);
    gpiod_line_request_output(led_camera_green, "HxtDeskService", GPIOD_LINE_ACTIVE_STATE_HIGH);

    return;
}

static void init_mute_led()
{
    led_mute_red = gpiod_chip_get_line(gpio9, 4);
    gpiod_line_request_output(led_mute_red, "HxtDeskService", GPIOD_LINE_ACTIVE_STATE_HIGH);
    led_mute_blue = gpiod_chip_get_line(gpio0, 5);
    gpiod_line_request_output(led_mute_blue, "HxtDeskService", GPIOD_LINE_ACTIVE_STATE_HIGH);
    led_mute_green = gpiod_chip_get_line(gpio10, 1);
    gpiod_line_request_output(led_mute_green, "HxtDeskService", GPIOD_LINE_ACTIVE_STATE_HIGH);

    return;
}

static void init_btn_events()
{
    btn_mute = gpiod_chip_get_line(gpio10, 6);
    gpiod_line_request_both_edges_events(btn_mute, "HxtDeskService");
    btn_vol_dec = gpiod_chip_get_line(gpio8, 2);
    gpiod_line_request_both_edges_events(btn_vol_dec, "HxtDeskService");
    btn_vol_inc = gpiod_chip_get_line(gpio8, 4);
    gpiod_line_request_both_edges_events(btn_vol_inc, "HxtDeskService");

    btn_reset = gpiod_chip_get_line(gpio5, 7);
    gpiod_line_request_both_edges_events(btn_reset, "HxtDeskService");
}

static void board_led_blinking()
{
    pthread_t tid; 
    pthread_create(&tid, NULL, led_blinking_thread, NULL);
}

void board_set_led_status(LED_STATUS status)
{
    led_status = status;
}

BOOL board_get_qrcode_scan_status()
{
    return g_deploying_net;
}

int board_gpio_init()
{
    gpio0 = gpiod_chip_open_by_name("gpiochip0");
    gpio2 = gpiod_chip_open_by_name("gpiochip2");
    gpio5 = gpiod_chip_open_by_name("gpiochip5");
    gpio8 = gpiod_chip_open_by_name("gpiochip8");
    gpio9 = gpiod_chip_open_by_name("gpiochip9");
    gpio10 = gpiod_chip_open_by_name("gpiochip10");

    /* led */
    init_wifi_led();
    init_camera_led();
    init_mute_led();
    /*led blinking*/
    board_led_blinking();

    /* button */
    init_btn_events();
    /* button event */
    board_listen_button_event();


    return 0;
}

void board_gpio_uninit()
{
    gpiod_line_release(btn_reset);
    gpiod_line_release(btn_mute);
    gpiod_line_release(btn_vol_inc);
    gpiod_line_release(btn_vol_dec);
    gpiod_line_release(led_camera_green);
    gpiod_line_release(led_camera_blue);
    gpiod_line_release(led_camera_red);
    gpiod_line_release(led_wifi_green);
    gpiod_line_release(led_wifi_blue);
    gpiod_line_release(led_wifi_red);
    gpiod_line_release(led_mute_green);
    gpiod_line_release(led_mute_blue);
    gpiod_line_release(led_mute_red);

    gpiod_chip_close(gpio0);
    gpiod_chip_close(gpio2);
    gpiod_chip_close(gpio5);
    gpiod_chip_close(gpio8);
    gpiod_chip_close(gpio9);
    gpiod_chip_close(gpio10);

    return;
}

