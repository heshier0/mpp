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

#define DEPART_ALARM_TIMEVAL       (3*60)          // 
#define ALARM_TIMEVAL              (3*60)          //5 minutes   
#define BAD_ALARM_TIMEVAL           (10)
#define CORRECT_JUDGE_TIMEVAL       (5)             //5 senconds

typedef struct check_status_t
{
    time_t _time;
    int _result;
};

static void* g_recog_handle = NULL;
static pthread_t g_proc_yuv_tid = NULL;
static BOOL g_keep_processing = TRUE;

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

static void* thread_proc_yuv_data_cb(void *param)
{
    int one_check_result = 0;
    BOOL inited_status = FALSE;
    BOOL change_to_correct = FALSE;
    BOOL first_away_alarm = TRUE;
    BOOL first_bad_alarm = TRUE;

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
        utils_print("one_check_reuslt = %d\n", one_check_result);
        if(!inited_status)
        {
            check_status._result = one_check_result;
            check_status._time = time(NULL);

            inited_status = TRUE;
        }

        if (one_check_result == BAD_POSTURE)
        {
            /* start record video */

        }

        if(one_check_result != check_status._result)
        {
            if (check_status._result == BAD_POSTURE)
            {
                change_to_correct = TRUE;
            }
            else 
            {
                change_to_correct = FALSE;
            }
            check_status._result = one_check_result;
            check_status._time = time(NULL);
        }
        else
        {
            time_t now = time(NULL);
            int interval = now - check_status._time;
            utils_print("%ld - %ld = %ld\n", now, check_status._time, interval);
            if( interval >= CORRECT_JUDGE_TIMEVAL)
            {
                switch (one_check_result)
                {
                case NORMAL_POSTURE:
                    if(change_to_correct)
                    {                    
                        if(interval >= CORRECT_JUDGE_TIMEVAL)
                        {
                            play_random_praise_voice();
                            check_status._time = now;
                        }
                    }
                    break;
                case BAD_POSTURE:
                    if (interval >= BAD_ALARM_TIMEVAL)
                    {
                        if(first_bad_alarm)
                        {
                            play_random_warn_voice();
                            check_status._time = now;
                            first_bad_alarm = FALSE;
                        }
                        else
                        {
                            if (interval >= ALARM_TIMEVAL)
                            {
                                play_random_warn_voice();
                                check_status._time = now;
                            }
                        }
                        /* save video */
                        
                    }                   
                    break;
                case AWAY_STATUS:
                    if(interval >= DEPART_ALARM_TIMEVAL)
                    {
                        if (first_away_alarm)
                        {
                            utils_send_local_voice(VOICE_CHILD_AWAY);
                            check_status._time = now;
                            first_away_alarm = FALSE;
                        }
                        else
                        {
                            utils_send_local_voice(VOICE_CAMERA_SLEEP);
                            /* exit recog thread */
                            g_keep_processing = FALSE;
                        }
                    }
                    break;    
                default:
                    break;
                }
            }
        }        

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