#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#include <sitting_posture.h>

#include "utils.h"
#include "common.h"
#include "server_comm.h"

#define NORMAL_POSTURE_STATUS         0
#define BAD_POSTURE_STATUS            1
#define AWAY_STATUS                   2

#define FIRST_DEPART_ALARM_TIMEVAL       (3*60)
#define LAST_DEPART_ALARM_TIMEVAL         (5*60)         
#define ALARM_TIMEVAL              (3*60)          
#define BAD_ALARM_TIMEVAL           (10)
#define CORRECT_JUDGE_TIMEVAL       (5)             
#define MIN_DURATION_TIME           (3)

/*
* width: 识别图像宽度
* height: 识别图像高度
* alarm_interval: 持续错误坐姿告警时间
* video_duration: 错误坐姿视频录制时间
* sanp_freq:抽帧频次
*/
typedef struct posture_param_s
{
    int width;
    int height;
    int alarm_interval;
    int video_duration;
    int snap_freq;
    int study_mode;
}PostureParams;

typedef struct check_status_t
{
    time_t _start_time;
    time_t _last_time;
    time_t _last_alarm_time;
    time_t _last_record_time;
    int _start_posture;
    int _last_posture;
};

static void* g_recog_handle = NULL;
static pthread_t g_proc_yuv_tid = NULL;
static BOOL g_keep_processing = FALSE;

static BOOL g_is_recording = FALSE;
static BOOL g_is_inited = FALSE;
static BOOL g_first_alarm = TRUE;
static BOOL g_first_away = TRUE;

static char g_mp4_file[128] = {0};
static char g_snap_file[128] = {0};
static int g_msg_qid;

#define SELF_DEF_VOICE ("/user/child_%d/alarm/P00%d.mp3")
static void play_random_warn_voice()
{
    char* voice[5] = {VOICE_SITTING_WARM1, VOICE_SITTING_WARM2, VOICE_SITTING_WARM3, VOICE_SITTING_WARM4, VOICE_SITTING_WARM5};  
    char self_def_voice[128] = {0};  
    int chlid_unid = hxt_get_child_unid();
    int alarm_type = hxt_get_alarm_type_cfg();
    int idx = rand() % 5;
    switch(alarm_type)
    {
        case 0:
            /* mute */
        break;
        case 1:
            utils_send_local_voice(VOICE_WARNING_BUZZ);
        break;
        case 2:
            utils_send_local_voice(voice[idx]);
        break;
        case 3:
            bzero(self_def_voice, 128);
            sprintf(self_def_voice, SELF_DEF_VOICE, chlid_unid, idx);
            utils_send_local_voice(self_def_voice);
        default:
            utils_send_local_voice(voice[idx]);
        break;
    }
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
        send_recording_mp4_cmd();
        g_is_recording = TRUE;
        utils_print("%s -----> Begin record\n", utils_get_current_time());
    }
    
    return TRUE;
}

static void delete_recorded()
{
    if(g_is_recording)
    {
        send_delete_mp4_cmd();
        g_is_recording = FALSE;
        utils_print("%s -----> Delete record\n", utils_get_current_time());
    }
}

static void stop_record()
{
    if(g_is_recording)
    {
        g_is_recording = FALSE;
        utils_print("%s -----> Stop record\n", utils_get_current_time());
        send_stop_record_mp4_cmd();
    }
}

static BOOL init_check_status(struct check_status_t *check_status, int check_result)
{
    if(!g_is_inited)
    {
        check_status->_start_posture = check_status->_last_posture = check_result;
        check_status->_start_time = check_status->_last_time = time(NULL);
        check_status->_last_record_time = time(NULL);
        check_status->_last_alarm_time = time(NULL);

        g_is_inited = TRUE;

        return TRUE;
    }

    return FALSE;
}

static BOOL send_study_report_type(StudyInfo *info)
{
    info->msg_type = 1;
    if (msgsnd(g_msg_qid, (void*)info, sizeof(StudyInfo) - sizeof(long), 0) < 0)
    {
        utils_print("send study info msg failed, %s\n", strerror(errno));
        return FALSE;
    }
    // utils_print("msg send over, study info type is %d\n", info->info_type);
    return TRUE;
}

/* confirm if change to normal posture */
static BOOL check_posture_changed(struct check_status_t *check_status, int check_result)
{
    time_t now = time(NULL); 
    
    if (check_status->_start_posture == check_result)
    {
        check_status->_last_posture = check_result;
        check_status->_last_time = now;

        // 连续的错误姿势
        if (check_status->_start_posture == BAD_POSTURE_STATUS)
        {
            if (!g_is_recording)
            {
                check_status->_last_record_time = now;
            }
            begin_recording();
        }
        else
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
            if(check_status->_last_posture != BAD_POSTURE_STATUS)
            {
                delete_recorded();
            }
            else
            { 
                /* means already change to bad posture */
                if (!g_is_recording)
                {
                    check_status->_last_record_time = now;
                }
                begin_recording(); 
                
            }
            
            if (check_status->_start_posture == BAD_POSTURE_STATUS)
            {
                if (check_result == NORMAL_POSTURE_STATUS)
                {
                     play_random_praise_voice();
                }
            }

            if (check_status->_start_posture == AWAY_STATUS)
            {
                if(!g_first_away)
                {
                    utils_send_local_voice(VOICE_CHILD_REAPPEAR);
                    g_first_away = TRUE;
                }

                StudyInfo info;
                memset(&info, 0, sizeof(StudyInfo));
                info.info_type = CHILD_BACK;
                // send_study_report_type(&info);
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

static BOOL check_posture_alarm(struct check_status_t *check_status, int check_result, int video_time_len, int check_interval)
{
    time_t now = time(NULL);
    int interval = now - check_status->_start_time;
    int record_interval = now - check_status->_last_record_time;
    int start_posture = check_status->_start_posture;
    if (start_posture != check_result)
    {
        return FALSE;
    }

    switch (check_result)
    {
    case NORMAL_POSTURE_STATUS:
        break;
    case BAD_POSTURE_STATUS:
        if (record_interval >= video_time_len)
        {
            stop_record();
            check_status->_last_record_time = now;
        }
        if (interval >= check_interval)
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
        } 
        // StudyInfo info;
        // memset(&info, 0, sizeof(StudyInfo));
        // info.info_type = BAD_POSTURE;
        // strcpy(info.file, g_mp4_file);
        // strcpy(info.snap, g_snap_file);
        // send_study_report_type(&info);                  
    break;
    case AWAY_STATUS:
        if(interval >= FIRST_DEPART_ALARM_TIMEVAL)
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
                if (interval >= LAST_DEPART_ALARM_TIMEVAL)
                {
                    utils_send_local_voice(VOICE_CAMERA_SLEEP);
                    utils_print("AWAY ALARM22222 !!!\n");
                    /* exit recog thread */
                    g_keep_processing = FALSE;
                }
            }
            delete_recorded(); 

            /* send message to hxt server */
            StudyInfo info;
            memset(&info, 0, sizeof(StudyInfo));
            info.info_type = CHILD_AWAY;
            // send_study_report_type(&info);
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

    if(NULL == param)
    {
        return NULL;
    }

    PostureParams *params = utils_malloc(sizeof(PostureParams));
    memcpy(params, param, sizeof(PostureParams));
    if (params->alarm_interval == 0)
    {
        params->alarm_interval = 10;
    }
    if(params->video_duration == 0)
    {
        params->video_duration = 10;
    }
    if (params->study_mode == 0)
    {
        params->study_mode = 3;
    }
    
    struct check_status_t check_status;
    memset(&check_status, 0, sizeof(struct check_status_t));

    g_msg_qid = msgget(STUDY_INFO_MQ_KEY, 0666 | IPC_CREAT);
    if(g_msg_qid < 0)
    {
        utils_print("create message queue error\n");
        return -1;
    }

    StudyInfo info;
    memset(&info, 0, sizeof(StudyInfo));
    info.info_type = STUDY_BEGIN;
    // send_study_report_type(&info);

    unsigned char *yuv_buf = NULL;
    utils_print("To process yuv data from vpss ....\n");
    while (g_keep_processing)
    {
        /* get yuv data from vpss */
        board_get_yuv_from_vpss_chn(&yuv_buf);
        if(NULL == yuv_buf)
        {
            utils_print("no vpss data...\n");
            continue;
        }
        /* recog posture */
        /* 0 : Normal */
        /* 1 : bad posture */
        /* 2 : away */
        one_check_result = run_sit_posture(g_recog_handle, yuv_buf, params->width, params->height, 5);
        utils_print("%s -----> %d\n", utils_get_current_time(), one_check_result);
        if (init_check_status(&check_status, one_check_result))
        {
            continue;
        }

        if(check_posture_changed(&check_status, one_check_result))
        {
            utils_print("POSTURE CHANGED\n");
            continue;
        }
        check_posture_alarm(&check_status, one_check_result, params->video_duration, params->alarm_interval);

        if (yuv_buf != NULL)
        {
            utils_free(yuv_buf);
            yuv_buf = NULL;
        }

        take_rest(100); 
    }
    /* prevent some fragmentary video*/
    delete_recorded();
    /**/
    if (g_recog_handle != NULL)
    {
        uninit_sit_posture(&g_recog_handle);
    }

    utils_free(params);

    /* send msg to notify ending */
    memset(&info, 0, sizeof(StudyInfo));
    info.info_type = STUDY_END;
    // send_study_report_type(&info);

    utils_send_local_voice(VOICE_CAMERA_SLEEP);

    utils_print("rocognize thread exit...\n");

    return NULL;
}

void start_posture_recognize()
{

    if (g_keep_processing)
    {
        return;
    }
    g_keep_processing = TRUE;


    PostureParams params;
    memset(&params, 0, sizeof(PostureParams));
    params.video_duration = hxt_get_video_length_cfg();
    params.alarm_interval = hxt_get_posture_judge_cfg();         
    params.width = hxt_get_video_height_cfg();
    params.height = hxt_get_video_width_cfg();
    params.snap_freq = hxt_get_attach_ratio_cfg();
    params.study_mode = hxt_get_study_mode_cfg();
    
    /* get posture pattern file */
    char *model_path1 = hxt_get_posture_detect_model_path_cfg();
    char *model_path2 = hxt_get_posture_class_model_path_cfg();
    if(NULL == model_path1 || NULL == model_path2)
    {
        return;
    }
    
    send_posture_start_cmd(params.height, params.width);
    utils_send_local_voice(VOICE_NORMAL_STATUS);

    // init 
    utils_print("To start recognize....\n");
    g_recog_handle = init_sit_posture(model_path1, model_path2);
    if (NULL == g_recog_handle)
    {
        utils_print("posture model init failed\n");
    }
    pthread_create(&g_proc_yuv_tid, NULL, thread_proc_yuv_data_cb, (void*)&params);
   
    return;
}

void stop_posture_recognize()
{
    if (g_keep_processing)
    {
        send_posture_stop_cmd();
        /* to tell mpp service stop video system */
        utils_print("To stop recognize....\n");
        g_keep_processing = FALSE;
    }
}