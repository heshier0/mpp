#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <sitting_posture.h>

#include "databuffer.h"
#include "utils.h"
#include "common.h"
#include "yuv2mp4.h"

#define NORMAL_POSTURE_STATUS         0
#define BAD_POSTURE_STATUS     1
#define AWAY_STATUS             2

#define DEPART_ALARM_TIMEVAL       (60) //(3*60)         
#define ALARM_TIMEVAL              (10)//(3*60)          
#define BAD_ALARM_TIMEVAL           (10)
#define CORRECT_JUDGE_TIMEVAL       (5)             
#define MIN_DURATION_TIME         (3)

typedef struct check_status_t
{
    time_t _start_time;
    time_t _last_time;
    time_t _last_alarm_time;
    int _start_posture;
    int _last_posture;
};

// static void* g_recog_handle = NULL;
static pthread_t g_proc_yuv_tid = NULL;
static BOOL g_keep_processing = FALSE;

static BOOL g_is_recording = FALSE;
static BOOL g_is_inited = FALSE;
static BOOL g_first_alarm = TRUE;
static BOOL g_first_away = TRUE;

static char g_mp4_file[128] = {0};
static char g_snap_file[128] = {0};

static int g_msg_qid;

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

static void generate_video_file_name()
{
	struct tm *tm;
	time_t now = time(0);
	tm = localtime(&now);

	int child_unid = hxt_get_child_unid();
	snprintf(g_mp4_file, 128, "/user/child_%d/video/%04d%02d%02d-%02d%02d%02d.mp4",
								child_unid,
								tm->tm_year + 1900,
								tm->tm_mon + 1, 
								tm->tm_mday,
								tm->tm_hour,
								tm->tm_min,
								tm->tm_sec);

    snprintf(g_snap_file, 128, "/user/child_%d/snap/%04d%02d%02d-%02d%02d%02d.jpg",
                            child_unid,
                            tm->tm_year + 1900,
                            tm->tm_mon + 1, 
                            tm->tm_mday,
                            tm->tm_hour,
                            tm->tm_min,
                            tm->tm_sec);

	return;
}

static BOOL begin_recording()
{
    if(!g_is_recording)
    {
        generate_video_file_name();
        start_video_recording(g_mp4_file);
        g_is_recording = TRUE;
        utils_print("START To record: %s.......\n", g_mp4_file);
    }
    return TRUE;
}

static void delete_recorded()
{
    if(g_is_recording)
    {
        utils_print("Deleting video.....\n");
        delete_posture_video();
        unlink(g_snap_file);
        g_is_recording = FALSE;
    }
}

static void stop_record()
{
    if(g_is_recording)
    {
        stop_video_recording();
        board_get_snap_from_venc_chn(g_snap_file);
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

static BOOL send_study_report_type(StudyInfo *info)
{
    
    info->msg_type = 1;
    if (msgsnd(g_msg_qid, (void*)info, sizeof(StudyInfo) - sizeof(long), 0) < 0)
    {
        utils_print("send study info msg failed, %d\n", errno);
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

        //防止中间有错误姿势晃过而开始录制的视频残留
        if (check_status->_start_posture != BAD_POSTURE_STATUS)
        {
            delete_recorded(); 
        }
        // 连续的错误姿势
        if (check_status->_start_posture == BAD_POSTURE_STATUS)
        {
            begin_recording();
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
                }
                mark_first_away_alarm();

                StudyInfo info;
                memset(&info, 0, sizeof(StudyInfo));
                info.info_type = CHILD_BACK;
                send_study_report_type(&info);
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
    case NORMAL_POSTURE_STATUS:
        break;
    case BAD_POSTURE_STATUS:
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

            StudyInfo info;
            memset(&info, 0, sizeof(StudyInfo));
            info.info_type = BAD_POSTURE;
            strcpy(info.file, g_mp4_file);
            strcpy(info.snap, g_snap_file);
            send_study_report_type(&info);
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
            StudyInfo info;
            memset(&info, 0, sizeof(StudyInfo));
            info.info_type = CHILD_AWAY;
            send_study_report_type(&info);
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

    void *recog_handle = (void*)param;

    struct check_status_t check_status;
    memset(&check_status, 0, sizeof(struct check_status_t));

    int width = hxt_get_video_height_cfg();
    int height = hxt_get_video_width_cfg();
    int alarm_time = 10;// hxt_get_posture_judge_cfg();

    g_msg_qid = msgget(STUDY_INFO_MQ_KEY, 0);
    if(g_msg_qid < 0)
    {
        utils_print("create message queue error\n");
        return -1;
    }
    StudyInfo info;
    memset(&info, 0, sizeof(StudyInfo));
    info.info_type = STUDY_BEGIN;
    send_study_report_type(&info);

    // utils_send_local_voice(VOICE_NORMAL_STATUS);

    char *yuv_buf = NULL;
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
        // one_check_result = run_sit_posture(g_recog_handle, yuv_buf, width, height, 5);
        one_check_result = run_sit_posture(recog_handle, yuv_buf, width, height, 3);
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

        check_posture_alarm(&check_status, one_check_result);

        if (yuv_buf != NULL)
        {
            free(yuv_buf);
            yuv_buf = NULL;
        }
        
        take_rest(150); 
    }
    /* prevent some fragmentary video*/
    delete_recorded();
    /**/
    if (recog_handle != NULL)
    {
        uninit_sit_posture(&recog_handle);
        recog_handle = NULL;
    }

    /* send msg to notify ending */
    memset(&info, 0, sizeof(StudyInfo));
    info.info_type = STUDY_END;
    send_study_report_type(&info);

    utils_send_local_voice(VOICE_CAMERA_SLEEP);

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
    if (!g_keep_processing)
    {
        g_keep_processing = TRUE;

        // g_recog_handle = init_sit_posture(model_path1, model_path2);
        void* recog_handle = init_sit_posture(model_path1, model_path2);
        if (recog_handle == NULL)
        {
            utils_print("posture model init failed\n");
        }
        pthread_create(&g_proc_yuv_tid, NULL, thread_proc_yuv_data_cb, recog_handle);
    }
    

    return;
}

void stop_posture_recognize()
{
    g_keep_processing = FALSE;
}