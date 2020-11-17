#include <sys/prctl.h>
#include <gpiod.h>
#include <hi_comm_aio.h>
#include <semaphore.h>

#include "utils.h"
#include "posture_check.h"
#include "server_comm.h"
#include "board_func.h"
#include "db.h"
#include "iflyos_func.h"

#include "multi_button.h"


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

static BOOL dec_vol_pressed = FALSE;
static BOOL inc_vol_pressed = FALSE;

static BOOL btn_mute_long_pressed = TRUE;

static time_t mute_pressed_start = 0;
static time_t mute_pressed_end = 0;

static time_t inc_pressed_start = 0;
static time_t inc_pressed_end  = 0;

static time_t dec_pressed_start = 0;
static time_t dec_pressed_end = 0;

static time_t reset_pressed_start = 0;
static time_t reset_pressed_end = 0;

static int scan_count = 0;
static LED_STATUS led_status = BOOTING;

extern BOOL g_deploying_net;
extern BOOL g_posture_running;
extern BOOL g_device_sleeping;
extern BOOL g_iflyos_first_login;
extern BOOL g_hxt_first_login;
extern int g_connect_count;
volatile BOOL g_hxt_wbsc_running;
volatile BOOL g_iflyos_wbsc_running;


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
    // system("ifconfig wlan0 down");
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

/*camera*/
uint8_t read_button_mute_gpio()
{
    return gpio_get_value(btn_mute);
}

void button_mute_callback(void *btn)
{
    uint32_t btn_event_val;

    if(g_deploying_net)
    {
        utils_print("deploying network....\n");
        return;
    }

    btn_event_val = get_button_event((struct Button *)btn);
    switch (btn_event_val)
    {
    case PRESS_DOWN:
    break;
    case PRESS_UP:
        btn_mute_long_pressed = TRUE;
    break;
    case PRESS_REPEAT:
    break;
    case SINGLE_CLICK:
        if (g_device_sleeping)
        {
            utils_print("Still sleeping...\n");
            break;
        }
        if(!g_posture_running)
        {
            board_set_led_status(CHECKING);
            start_posture_recognize();
        }
        else
        {
            stop_posture_recognize();
            board_set_led_status(CHECKING_EXIT);   
        }
    break;
    case DOUBLE_CLICK:
    break;
    case LONG_PRESS_START:
        mute_pressed_start = time(NULL);
    break;
    case LONG_PRESS_HOLD:
        mute_pressed_end = time(NULL);
        if ((mute_pressed_end - mute_pressed_start) >= 3)
        {
            if (btn_mute_long_pressed)
            {
                if (g_device_sleeping)      //device sleeping
                {
                    utils_print("To wake....\n");
                    g_iflyos_first_login = TRUE;
                    g_hxt_first_login =  TRUE;
                    g_device_sleeping = FALSE;
                    board_set_led_status(NET_ERR);
                    utils_print("OK!!\n");
                }
                else
                {
                    utils_print("To sleeping...\n");
                    g_device_sleeping = TRUE;
                    if (g_posture_running)
                    {
                        stop_posture_recognize();
                    }
                    iflyos_websocket_stop();
                    hxt_websocket_stop();
                    board_set_led_status(SLEEPING);
                    utils_disconnect_wifi();
                    utils_print("ZZZZZZZZ\n");
                }   
                btn_mute_long_pressed = FALSE;
            }
        }
    break;    
    default:
    break;
    }

    return;
}

/*inc volume*/
uint8_t read_button_vol_inc_gpio()
{
    return gpio_get_value(btn_vol_inc);
}

void button_vol_inc_callback(void *btn)
{
    int vol = -20;
    uint32_t btn_event_val;

    if (g_device_sleeping)
    {
        utils_print("Still sleeping...\n");
        return;
    }

    btn_event_val = get_button_event((struct Button *)btn);
    switch (btn_event_val)
    {
    case PRESS_DOWN:
        break;
    case PRESS_UP:
        inc_vol_pressed = FALSE;
        break;
    case PRESS_REPEAT:
    case SINGLE_CLICK:
        vol = board_inc_volume();
        utils_print("Now volume is %d\n", vol);
        break;
    case DOUBLE_CLICK:
        break;
    case LONG_PRESS_START:
        inc_pressed_start = time(NULL);
        break;
    case LONG_PRESS_HOLD:
        inc_pressed_end = time(NULL);
        if ((inc_pressed_end - inc_pressed_start) >= 3)
        {
            inc_vol_pressed = TRUE;
        }
        break;    
    default:
        break;
    }
}

/*dec volume*/
uint8_t read_button_vol_dec_gpio()
{
    return gpio_get_value(btn_vol_dec);
}

void button_vol_dec_callback(void *btn)
{
    int vol = -20;
    uint32_t btn_event_val;

    if (g_device_sleeping)
    {
        utils_print("Still sleeping...\n");
        return;
    }

    btn_event_val = get_button_event((struct Button *)btn);
    switch (btn_event_val)
    {
    case PRESS_DOWN:
        break;
    case PRESS_UP:
        dec_vol_pressed = FALSE;
    break;
    case PRESS_REPEAT:
    case SINGLE_CLICK:
        vol = board_dec_volume();
    break;
    case DOUBLE_CLICK:
        break;
    case LONG_PRESS_START:
        dec_pressed_start = time(NULL);
        break;
    case LONG_PRESS_HOLD:
        dec_pressed_end = time(NULL);
        if ((dec_pressed_end - dec_pressed_start) >= 3)
        {
            dec_vol_pressed = TRUE;
        }
    break;    
    default:
        break;
    }
}

/*reset*/
uint8_t read_button_reset_gpio()
{
    return gpio_get_value(btn_reset);
}

void button_reset_callback(void *btn)
{
    uint32_t btn_event_val;
    btn_event_val = get_button_event((struct Button *)btn);
    switch (btn_event_val)
    {
    case PRESS_DOWN:
        break;
    case PRESS_UP:
        break;
    case PRESS_REPEAT:
        break;
    case SINGLE_CLICK:
        board_led_all_off();
        /* reboot */
        utils_print("Btn pressed to reboot\n");
        utils_system_reboot();
        break;
    case DOUBLE_CLICK:
        break;
    case LONG_PRESS_START:
        reset_pressed_start = time(NULL);
        break;
    case LONG_PRESS_HOLD:
        reset_pressed_end = time(NULL);
        if ((reset_pressed_end - reset_pressed_start) >= 3)
        {
            board_set_led_status(RESETING);
            sleep(3);
            /* reset */
            utils_system_reset();
            utils_system_reboot();
        }
        break;    
    default:
        break;
    }
}

static void* check_reset_event(void *param)
{
    struct Button button_reset;
    button_init(&button_reset, read_button_reset_gpio, 0);
    button_attach(&button_reset, PRESS_DOWN, button_reset_callback);
    button_attach(&button_reset, PRESS_UP, button_reset_callback);
    button_attach(&button_reset, PRESS_REPEAT, button_reset_callback);
    button_attach(&button_reset, SINGLE_CLICK, button_reset_callback);
    button_attach(&button_reset, DOUBLE_CLICK, button_reset_callback);
    button_attach(&button_reset, LONG_PRESS_START, button_reset_callback);
    button_attach(&button_reset, LONG_PRESS_HOLD, button_reset_callback);
    button_start(&button_reset);

    prctl(PR_SET_NAME, "check_reset_event");
    pthread_detach(pthread_self());

    while (1)
    {
        button_ticks();
        usleep(2000);
    }

    #if 0
    struct gpiod_line_event event;
    time_t start = 0, end = 0;
    BOOL pressed = FALSE;

    prctl(PR_SET_NAME, "check_reset_event");
    pthread_detach(pthread_self());
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
    #endif
    return;
}

static void* check_inc_vol_event(void *param)
{
    struct Button button_vol_inc;
    button_init(&button_vol_inc, read_button_vol_inc_gpio, 0);
    button_attach(&button_vol_inc, PRESS_DOWN, button_vol_inc_callback);
    button_attach(&button_vol_inc, PRESS_UP, button_vol_inc_callback);
    button_attach(&button_vol_inc, PRESS_REPEAT, button_vol_inc_callback);
    button_attach(&button_vol_inc, SINGLE_CLICK, button_vol_inc_callback);
    button_attach(&button_vol_inc, DOUBLE_CLICK, button_vol_inc_callback);
    button_attach(&button_vol_inc, LONG_PRESS_START, button_vol_inc_callback);
    button_attach(&button_vol_inc, LONG_PRESS_HOLD, button_vol_inc_callback);
    button_start(&button_vol_inc);

    prctl(PR_SET_NAME, "check_inc_volume");
    pthread_detach(pthread_self());

    while (1)
    {
        button_ticks();
        usleep(2000);
    }
    

    #if 0
    struct gpiod_line_event event;
    time_t start = 0, end = 0;
    BOOL already_max = FALSE;

    prctl(PR_SET_NAME, "check_inc_volume");
    pthread_detach(pthread_self());
    while(1)
     {
         if (g_device_sleeping)
         {
             utils_print("not inc device sleeping...\n");
             sleep(5);
             continue;
         }

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
     #endif
    return NULL;
}

static void* check_dec_vol_event(void *param)
{
    struct Button button_vol_dec;
    button_init(&button_vol_dec, read_button_vol_dec_gpio, 0);
    button_attach(&button_vol_dec, PRESS_DOWN, button_vol_dec_callback);
    button_attach(&button_vol_dec, PRESS_UP, button_vol_dec_callback);
    button_attach(&button_vol_dec, PRESS_REPEAT, button_vol_dec_callback);
    button_attach(&button_vol_dec, SINGLE_CLICK, button_vol_dec_callback);
    button_attach(&button_vol_dec, DOUBLE_CLICK, button_vol_dec_callback);
    button_attach(&button_vol_dec, LONG_PRESS_START, button_vol_dec_callback);
    button_attach(&button_vol_dec, LONG_PRESS_HOLD, button_vol_dec_callback);
    button_start(&button_vol_dec);

    prctl(PR_SET_NAME, "check_dec_volume");
    pthread_detach(pthread_self());

    while (1)
    {
        button_ticks();
        usleep(2000);
    }
    
    #if 0
    struct gpiod_line_event event;
    time_t start = 0, end = 0;
    prctl(PR_SET_NAME, "check_dec_volume");
    pthread_detach(pthread_self());
    while(1)
     {
         if (g_device_sleeping)
         {
             utils_print("not dec device sleeping...\n");
             sleep(5);
             continue;
         }

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
    #endif

    return NULL;
}

static void* check_posture_event(void *param)
{
    struct Button button_mute;
    button_init(&button_mute, read_button_mute_gpio, 0);
    button_attach(&button_mute, PRESS_DOWN, button_mute_callback);
    button_attach(&button_mute, PRESS_UP, button_mute_callback);
    button_attach(&button_mute, PRESS_REPEAT, button_mute_callback);
    button_attach(&button_mute, SINGLE_CLICK, button_mute_callback);
    button_attach(&button_mute, DOUBLE_CLICK, button_mute_callback);
    button_attach(&button_mute, LONG_PRESS_START, button_mute_callback);
    button_attach(&button_mute, LONG_PRESS_HOLD, button_mute_callback);
    button_start(&button_mute);

    prctl(PR_SET_NAME, "check_posture");
    pthread_detach(pthread_self());

    while (1)
    {
        button_ticks();
        usleep(2000);
    }

    #if 0
    struct gpiod_line_event event;
    time_t start = 0, end = 0;
    time_t begin_time = 0;
    prctl(PR_SET_NAME, "check_posture");
    pthread_detach(pthread_self());
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
                    utils_print("To wake....\n");
                    g_device_sleeping = FALSE;
                    start_iflyos_websocket_thread();
                    board_set_led_status(NORMAL);
                    utils_print("OK!!\n");
                }
                else
                {
                    utils_print("To sleeping...\n");
                    g_device_sleeping = TRUE;
                    iflyos_websocket_stop();
                    if (g_posture_running)
                    {
                        stop_posture_recognize();
                    }
                    board_set_led_status(SLEEPING);
                    utils_print("ZZZZZZZZ\n");
                }    
            }
            else
            {
                if (g_device_sleeping)
                {
                    utils_print("Still sleeping...\n");
                    continue;
                }

                if(!g_posture_running)
                {
                    board_set_led_status(CHECKING);
                    start_posture_recognize();
                    begin_time = time(NULL);
                }
                else
                {
                    board_set_led_status(NORMAL);
                    stop_posture_recognize();   
                }
            }
        }
    }
    #endif
    return NULL;
}

static void* check_scan_qrcode_event(void *param)
{
    prctl(PR_SET_NAME, "scan_qrcode");
    pthread_detach(pthread_self());
    
    while(1)
    {
        if (g_device_sleeping)
        {
            sleep(5);
            continue;
        }

        if (inc_vol_pressed && dec_vol_pressed)
        {
            if (g_deploying_net )
            {
                utils_print("Now still deploying...\n");
                continue;
            }
            g_deploying_net = TRUE;

            clear_wifi_info();

            if(g_posture_running)
            {
                stop_posture_recognize();
            }

            if (g_hxt_wbsc_running)
            {
                hxt_websocket_stop();
            }

            if(g_iflyos_wbsc_running)
            {
                iflyos_websocket_stop();
            }
            
            board_set_led_status(BINDING);

            utils_print("To scan qrcode....\n");
            
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
                    utils_send_local_voice(VOICE_QUERY_NET_ERROR);
                    board_set_led_status(NET_ERR);
                    sleep(10);
                    break;
                }
            }
            scan_count = 0;
            g_deploying_net = FALSE;
            g_connect_count = 0;
        }
       
        sleep(3);
    }

    return NULL;
}

static void* led_blinking_thread(void* param)
{
    BOOL changed_to_normal = FALSE;
    prctl(PR_SET_NAME, "check_led_status");
    pthread_detach(pthread_self());
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
            sleep(1);
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
            sleep(1);
        break;
        case CHECKING_EXIT:
            gpio_set_value(led_camera_green, 1);
            sleep(1);
        break;
        case NET_ERR:
            gpio_set_value(led_wifi_blue, 1);
            gpio_set_value(led_wifi_green, 1);
            gpio_set_value(led_wifi_red, 0);
            if (!g_posture_running)
            {
                set_camera_led_off();
            }
            changed_to_normal = FALSE;
            sleep(1);
        break;
        case CAMERA_ERR:
            set_camera_led_off();
            gpio_set_value(led_camera_red, 0);
            sleep(1);
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
            sleep(1);
        break;
        case SLEEPING:
            board_led_all_off();
            sleep(1);
        break;
        case NORMAL:
        default:   
            if (!g_posture_running)
            {
                set_camera_led_off();
            } 
            if (!changed_to_normal)
            {
                set_wifi_led_off();
                changed_to_normal = TRUE;
            }
            gpio_set_value(led_wifi_green, 0);
            sleep(1);
        break;
        }
    }

    return NULL;
}

static void * check_btn_event(void *param)
{
    struct Button button_mute;
    button_init(&button_mute, read_button_mute_gpio, 0);
    button_attach(&button_mute, PRESS_DOWN, button_mute_callback);
    button_attach(&button_mute, PRESS_UP, button_mute_callback);
    button_attach(&button_mute, PRESS_REPEAT, button_mute_callback);
    button_attach(&button_mute, SINGLE_CLICK, button_mute_callback);
    button_attach(&button_mute, DOUBLE_CLICK, button_mute_callback);
    button_attach(&button_mute, LONG_PRESS_START, button_mute_callback);
    button_attach(&button_mute, LONG_PRESS_HOLD, button_mute_callback);
    button_start(&button_mute);

    struct Button button_vol_dec;
    button_init(&button_vol_dec, read_button_vol_dec_gpio, 0);
    button_attach(&button_vol_dec, PRESS_DOWN, button_vol_dec_callback);
    button_attach(&button_vol_dec, PRESS_UP, button_vol_dec_callback);
    button_attach(&button_vol_dec, PRESS_REPEAT, button_vol_dec_callback);
    button_attach(&button_vol_dec, SINGLE_CLICK, button_vol_dec_callback);
    button_attach(&button_vol_dec, DOUBLE_CLICK, button_vol_dec_callback);
    button_attach(&button_vol_dec, LONG_PRESS_START, button_vol_dec_callback);
    button_attach(&button_vol_dec, LONG_PRESS_HOLD, button_vol_dec_callback);
    button_start(&button_vol_dec);

    struct Button button_vol_inc;
    button_init(&button_vol_inc, read_button_vol_inc_gpio, 0);
    button_attach(&button_vol_inc, PRESS_DOWN, button_vol_inc_callback);
    button_attach(&button_vol_inc, PRESS_UP, button_vol_inc_callback);
    button_attach(&button_vol_inc, PRESS_REPEAT, button_vol_inc_callback);
    button_attach(&button_vol_inc, SINGLE_CLICK, button_vol_inc_callback);
    button_attach(&button_vol_inc, DOUBLE_CLICK, button_vol_inc_callback);
    button_attach(&button_vol_inc, LONG_PRESS_START, button_vol_inc_callback);
    button_attach(&button_vol_inc, LONG_PRESS_HOLD, button_vol_inc_callback);
    button_start(&button_vol_inc);

    struct Button button_reset;
    button_init(&button_reset, read_button_reset_gpio, 0);
    button_attach(&button_reset, PRESS_DOWN, button_reset_callback);
    button_attach(&button_reset, PRESS_UP, button_reset_callback);
    button_attach(&button_reset, PRESS_REPEAT, button_reset_callback);
    button_attach(&button_reset, SINGLE_CLICK, button_reset_callback);
    button_attach(&button_reset, DOUBLE_CLICK, button_reset_callback);
    button_attach(&button_reset, LONG_PRESS_START, button_reset_callback);
    button_attach(&button_reset, LONG_PRESS_HOLD, button_reset_callback);
    button_start(&button_reset);

    prctl(PR_SET_NAME, "btn_event_check");
    pthread_detach(pthread_self());

    while (1)
    {
        button_ticks();
        usleep(2000);
    }

    return NULL;
}

static void board_listen_button_event()
{
    pthread_t t1, t2, t3, t4, t5;
    // pthread_create(&t1, NULL, check_inc_vol_event, NULL);
    // pthread_create(&t2, NULL, check_dec_vol_event, NULL);
    // pthread_create(&t4, NULL, check_posture_event, NULL);
    // pthread_create(&t5, NULL, check_reset_event, NULL);
    pthread_create(&t3, NULL, check_scan_qrcode_event, NULL);
    pthread_create(&t1, NULL, check_btn_event, NULL);
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
    gpiod_line_request_input(btn_mute, "HxtDeskService");
    //gpiod_line_request_both_edges_events(btn_mute, "HxtDeskService");
    btn_vol_dec = gpiod_chip_get_line(gpio8, 2);
    gpiod_line_request_input(btn_vol_dec, "HxtDeskService");
    // gpiod_line_request_both_edges_events(btn_vol_dec, "HxtDeskService");
    btn_vol_inc = gpiod_chip_get_line(gpio8, 4);
    gpiod_line_request_input(btn_vol_inc, "HxtDeskService");
    // gpiod_line_request_both_edges_events(btn_vol_inc, "HxtDeskService");
    btn_reset = gpiod_chip_get_line(gpio5, 7);
    gpiod_line_request_input(btn_reset, "HxtDeskService");
    // gpiod_line_request_both_edges_events(btn_reset, "HxtDeskService");
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

