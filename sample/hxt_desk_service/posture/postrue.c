#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sitting_posture.h>

#include "utils.h"

static void* g_recog_handle = NULL;
static pthread_t g_read_yuv_tid;
static pthread_t g_proc_yuv_tid;
static BOOL g_keep_reading = TRUE;
static BOOL g_keep_processing = TRUE;

static void* thread_read_yuv_data_cb(void *param)
{
    while(g_keep_reading)
    {
        sleep(1);
    }

    return NULL;
}

static void* thread_proc_yuv_data_cb(void *param)
{
    while (g_keep_processing)
    {
        sleep(1);
    }
    
    return NULL;
}

void posture_start_recognize()
{
    char *model_path1 = hxt_get_posture_coco_model_path();
    char *model_path2 = hxt_get_posture_class_model_path();
    if(NULL == model_path1 || NULL == model_path2)
    {
        return;
    }
    // init 
    g_recog_handle = init_sit_posture(model_path1, model_path2);

    pthread_create(&g_read_yuv_tid, NULL, thread_read_yuv_data_cb, NULL);     
    pthread_create(&g_proc_yuv_tid, NULL, thread_proc_yuv_data_cb, NULL);

    return;
}

void posture_stop_recognize()
{
    g_keep_reading = FALSE;
    g_keep_processing = FALSE;

    pthread_join(g_keep_reading, 0);
    pthread_join(g_keep_processing, 0);

    uninit_sit_posture(&g_recog_handle);
}