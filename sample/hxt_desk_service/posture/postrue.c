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

#define DEPART_ALARM_TIME       (1080)

static void* g_recog_handle = NULL;
static pthread_t g_proc_yuv_tid = NULL;
static BOOL g_keep_processing = TRUE;


static void take_rest(int time_ms)
{
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = time_ms * 1000;

    select(0, NULL, NULL, NULL, &timeout);
}


static void* thread_proc_yuv_data_cb(void *param)
{
    int recog_result = 0;
    int one_check_result = 0;
    int check_count = 0;
    BOOL post_correct = TRUE;
    int width = hxt_get_video_height_cfg();
    int height = hxt_get_video_width_cfg();
    char *yuv_buf = NULL;
    int alarm_time = 10;// hxt_get_posture_judge_cfg();
    
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
        one_check_result = run_sit_posture(g_recog_handle, yuv_buf, width, height, 3);
        recog_result += one_check_result;
        check_count += 1;
        utils_print("check count %d, recog_result = %d\n", check_count, recog_result);

        if((recog_result == 3 * 1 * alarm_time) && (check_count == 30))
        {
            post_correct = FALSE;
            utils_send_local_voice(VOICE_SITTING_WARM1);
            check_count = 0;
            recog_result = 0;
        }
        else if (recog_result >= DEPART_ALARM_TIME)
        {
            utils_send_local_voice(VOICE_CHILD_AWAY);
            recog_result = 0;
            check_count = 0;

            /* step to sleep countdown */

        }
        else if (recog_result == 0)
        {
            if(!post_correct)
            {
                utils_send_local_voice(VOICE_SITTING_PRAISE1);
            }
            post_correct = TRUE;
            check_count = 0;
        }
        

        if (yuv_buf != NULL)
        {
            free(yuv_buf);
            yuv_buf = NULL;
        }
        
        take_rest(333 * 1000); 
    }
  

    utils_print("rocognize thread exit...\n");
    
    return NULL;
}

void start_posture_recognize()
{
    char *model_path1 = hxt_get_posture_coco_model_path();
    char *model_path2 = hxt_get_posture_class_model_path();
    if(NULL == model_path1 || NULL == model_path2)
    {
        return;
    }
    // init 
    g_recog_handle = init_sit_posture(model_path1, model_path2);

    pthread_create(&g_proc_yuv_tid, NULL, thread_proc_yuv_data_cb, NULL);

    /* every 300ms to get a yuv buffer*/
    

    return;
}

void posture_stop_recognize()
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