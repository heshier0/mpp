#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>


#include <sitting_posture.h>

#include "databuffer.h"
#include "utils.h"
#include "common.h"
#include "yuv2mp4.h"

#define NORMAL_POSTURE         0
#define BAD_POSTURE             1
#define AWAY_STATUS             2

#define DEPART_ALARM_TIMEVAL       (60) //(3*60)         
#define ALARM_TIMEVAL              (30)//(3*60)          
#define BAD_ALARM_TIMEVAL           (10)
#define CORRECT_JUDGE_TIMEVAL       (5)             
#define MIN_DURATION_TIME         (5)

typedef struct check_status_t
{
    time_t _start_time;
    time_t _last_time;
    time_t _last_alarm_time;
    int _start_posture;
    int _last_posture;
};

static void* g_recog_handle = NULL;
static pthread_t g_proc_yuv_tid = NULL;
static BOOL g_keep_processing = TRUE;

static BOOL g_is_recording = FALSE;
static BOOL g_is_inited = FALSE;
static BOOL g_change_to_correct = FALSE;
static BOOL g_first_alarm = TRUE;
static BOOL g_first_away = TRUE;

static void play_random_warn_voice()
{
    char* voice[5] = {VOICE_SITTING_WARM1, VOICE_SITTING_WARM2, VOICE_SITTING_WARM3, VOICE_SITTING_WARM4, VOICE_SITTING_WARM5};
    int idx = rand() % 5;
    
    utils_send_local_voice(voice[idx]);
}

static void play_random_praise_voice()
{
    char* voice[5] = {VOICE_SITTING_PRAISE1, VOICE_SITTING_PRAISE2, VOICE_SITTING_PRAISE3, VOICE_SITTING_PRAISE4, VOICE_SITTING_PRAISE5};
    int idx = rand() % 5;
    
    utils_send_local_voice(voice[idx]);
}

static void take_rest(int time_ms)
{
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = time_ms * 1000;

    select(0, NULL, NULL, NULL, &timeout);
}

static BOOL begin_recording()
{
    if(!g_is_recording)
    {
        start_video_recording();
        g_is_recording = TRUE;
        utils_print("START To record.......\n");
    }
    return TRUE;
}

static void delete_recorded()
{
    if(g_is_recording)
    {
        utils_print("Deleting video.....\n");
        delete_posture_video();
        g_is_recording = FALSE;
    }
}

static void stop_record()
{
    if(g_is_recording)
    {
        stop_video_recording();
        g_is_recording = FALSE;
    }

}

static void mark_first_away_alarm()
{
    if(!g_first_away)
    {
        g_first_away = TRUE;
    }
}

static BOOL init_check_status(struct check_status_t *check_status, int check_result)
{
    if(!g_is_inited)
    {
        check_status->_start_posture = check_status->_last_posture = check_result;
        check_status->_start_time = check_status->_last_time = time(NULL);

        g_is_inited = TRUE;

        return TRUE;
    }

    return FALSE;
}

/* confirm if change to normal posture */
static BOOL check_posture_changed(struct check_status_t *check_status, int check_result)
{
    time_t now = time(NULL); 

    if (check_status->_start_posture == check_result)
    {
        check_status->_last_posture = check_result;
        check_status->_last_time = now;

        if (check_status->_start_posture != BAD_POSTURE)
        {
            delete_recorded();
        }

        return FALSE;
    }

    if (check_status->_last_posture != check_result)
    {
        check_status->_last_posture = check_result;
        check_status->_last_time = now;
    }
    else
    {
        int interval = now - check_status->_last_time;
        if (interval >= MIN_DURATION_TIME)
        {
            if(check_status->_last_posture != BAD_POSTURE)
            {
                delete_recorded();
            }
            else
            {
                begin_recording();
            }
            
            if (check_status->_start_posture == BAD_POSTURE)
            {
                if (check_result == NORMAL_POSTURE)
                {
                     play_random_praise_voice();
                }
            }

            if (check_status->_start_posture == AWAY_STATUS)
            {
                if(!g_first_away)
                {
                    utils_send_local_voice(VOICE_CHILD_REAPPEAR);
                }
                mark_first_away_alarm();
                
            }

            check_status->_start_posture = check_result;
            check_status->_start_time = now;
            check_status->_last_posture = check_result;
            check_status->_last_time = now;

            return TRUE;
        }    
    }      

    return FALSE;
}

static BOOL check_posture_alarm(struct check_status_t *check_status, int check_result)
{
    time_t now = time(NULL);
    int interval = now - check_status->_start_time;
    int start_posture = check_status->_start_posture;
    if (start_posture != check_result)
    {
        return FALSE;
    }

    switch (check_result)
    {
    case NORMAL_POSTURE:
        break;
    case BAD_POSTURE:
        if (interval >= BAD_ALARM_TIMEVAL)
        {
            if(g_first_alarm)
            {
                play_random_warn_voice();
                utils_print("BAD POSTURE ALARM !!!\n");
                check_status->_last_alarm_time = now;
                g_first_alarm = FALSE;
            }
            else
            {
                if ((now - check_status->_last_alarm_time) >= ALARM_TIMEVAL)
                {
                    play_random_warn_voice();
                    utils_print("BAD POSTURE ALARM2222 !!!\n");
                    check_status->_last_alarm_time = now;
                }
            }
            check_status->_start_posture = check_result;
            check_status->_start_time = now;
            stop_record();
        }                   
        break;
    case AWAY_STATUS:
        if(interval >= DEPART_ALARM_TIMEVAL)
        {
            if (g_first_away)
            {
                utils_send_local_voice(VOICE_CHILD_AWAY);
                utils_print("AWAY ALARM !!!\n");
                check_status->_start_time = now;
                g_first_away = FALSE;
            }
            else
            {
                utils_send_local_voice(VOICE_CAMERA_SLEEP);
                utils_print("AWAY ALARM22222 !!!\n");
                /* exit recog thread */
                g_keep_processing = FALSE;
            }
            delete_recorded();

            /* send message to hxt server */
            
        }
        break;    
    default:
        break;
    }

    return TRUE;
}

static void* thread_proc_yuv_data_cb(void *param)
{
    int one_check_result = 0;
    BOOL inited_status = FALSE;
    BOOL change_to_correct = FALSE;
    BOOL first_away_alarm = TRUE;
    BOOL first_bad_alarm = TRUE;
    BOOL recording = FALSE;

    struct check_status_t check_status;
    memset(&check_status, 0, sizeof(struct check_status_t));

    int width = hxt_get_video_height_cfg();
    int height = hxt_get_video_width_cfg();
    int alarm_time = 10;// hxt_get_posture_judge_cfg();

    char *yuv_buf = NULL;
    
    utils_print("To process yuv data from vpss ....\n");
    while (g_keep_processing)
    {
        /* get yuv data from vpss */
        board_get_yuv_from_vpss_chn(&yuv_buf);
        if(NULL == yuv_buf)
        {
            continue;
        }

        /* recog posture */
        /* 0 : Normal */
        /* 1 : bad posture */
        /* 2 : away */
        one_check_result = run_sit_posture(g_recog_handle, yuv_buf, width, height, 3);
        utils_print("%s -----> %d\n", get_current_time(), one_check_result);
        if (init_check_status(&check_status, one_check_result))
        {
            continue;
        }

        // BOOL recording = begin_recording(one_check_result);
        check_posture_changed(&check_status, one_check_result);
        if(check_posture_changed(&check_status, one_check_result))
        {
            utils_print("POSTURE CHANGED\n");
            continue;
        }

        check_posture_alarm(&check_status, one_check_result);

        if (yuv_buf != NULL)
        {
            free(yuv_buf);
            yuv_buf = NULL;
        }
        
        take_rest(150); 
    }

    if (g_recog_handle != NULL)
    {
        uninit_sit_posture(&g_recog_handle);
    }
    g_keep_processing = FALSE;
    utils_print("rocognize thread exit...\n");

    return NULL;
}

void start_posture_recognize()
{
    char *model_path1 = hxt_get_posture_detect_model_path();
    char *model_path2 = hxt_get_posture_class_model_path();
    if(NULL == model_path1 || NULL == model_path2)
    {
        return;
    }
    // init 
    g_recog_handle = init_sit_posture(model_path1, model_path2);

    pthread_create(&g_proc_yuv_tid, NULL, thread_proc_yuv_data_cb, NULL);

    return;
}

void posture_stop_recognize()
{
    if(g_keep_processing)
    {
        g_keep_processing = FALSE;

        if(g_proc_yuv_tid != NULL)
        {
            pthread_join(g_proc_yuv_tid, NULL); 
        }
        
        if (g_recog_handle != NULL)
        {
            uninit_sit_posture(&g_recog_handle);
        }
    }
}