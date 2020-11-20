#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <uwsc/uwsc.h>

#include "utils.h"
#include "hxt_defines.h"
#include "databuffer.h"
#include "db.h"
#include "hxt_client.h"
#include "posture_check.h"
#include "board_func.h"

typedef enum 
{
    POWER_ON = 1,
    POWER_OFF,
    HEARTBEAT
}REPORT_TYPE;

extern BOOL g_hxt_wbsc_running;
extern BOOL g_hxt_first_login;
extern DATABUFFER g_msg_buffer;
extern int g_camera_status;
extern BOOL g_deploying_net;
extern BOOL g_device_sleeping;
extern sem_t g_hxt_run_flag;

static struct uwsc_client *hxt_wsc = NULL;
static struct ev_loop *g_hxt_loop = NULL;
static ev_async g_async_watcher;
static BOOL g_recv_running = TRUE;
static BOOL g_sem_posted = FALSE;


static BOOL init_study_info(ReportInfo *report_info, StudyInfo *study_info, AliossOptions *opts)
{
    BOOL video_uploaded = FALSE, snap_uploaded = FALSE;

    if(NULL == report_info || NULL == study_info || NULL == opts)
    {
        return FALSE;
    }

    if(study_info->info_type < 1 || study_info->info_type > 5)
    {
        utils_print("Study info type ERROR\n");
        return FALSE;
    }

    utils_print("type:%d, video:%s, snap:%s\n", study_info->info_type, study_info->file, study_info->snap);

    report_info->camera_status = g_camera_status;
    int parent_unid = get_parent_id();
    if (parent_unid <= 0)
    {
        return FALSE;
    }
    report_info->parent_unid = parent_unid;
    int child_unid = get_select_child_id();
    if (child_unid <= 0)
    {
        return FALSE;
    }
    report_info->child_unid = child_unid;
    report_info->report_type = study_info->info_type;
    strcpy(report_info->study_date, utils_date_to_string());
    strcpy(report_info->report_time, utils_time_to_string());
    report_info->study_mode = get_study_mode(report_info->child_unid);
    if (report_info->report_type == BAD_POSTURE)
    {
        report_info->duration = 10;

        /*query upload sum today*/
        int count = get_upload_count_of_day(child_unid);
        if (count >= 50)
        {
            strcpy(report_info->video_url, "");
            strcpy(report_info->snap_url, "");
            return TRUE;
        }

        /*waiting for mp4 file saved, better way is to recive a signal to notify*/
        int retry_count = 0;
        while (utils_get_file_size(study_info->file) < 1024 && retry_count < 5)
        {
            utils_print("video %s size is error\n", study_info->file);
            sleep(1);
            retry_count ++;
        }
        retry_count = 0;

        char* mp4_object = hxt_upload_file(study_info->file, (void*)opts);
        if (mp4_object != NULL)
        {
            utils_print("Upload video %s OK\n", study_info->file);
            strcpy(report_info->video_url, mp4_object);
            utils_free(mp4_object);
            video_uploaded = TRUE;
        }
        else
        {
            utils_print("Upload video %s Failed\n", study_info->file);
        }
        //remove(study_info->file);

        while (utils_get_file_size(study_info->snap) < 1024 && retry_count < 5)
        {
            utils_print("snap %s size is error\n", study_info->snap);
            sleep(1);
            retry_count ++;
        }
   
        char *snap_object = hxt_upload_file(study_info->snap, (void*)opts);
        if (snap_object != NULL)
        {
            utils_print("Upload snap %s OK\n", study_info->snap);
            strcpy(report_info->snap_url, snap_object);
            utils_free(snap_object);
            snap_uploaded = TRUE;
        }
        else
        {            
            utils_print("Upload snap %s Failed\n", study_info->snap);
        }
        //remove(study_info->snap);

        /*if uploaded, increased in db*/
        if (video_uploaded && snap_uploaded)
        {
            inc_upload_count(child_unid);
        }
    }

    return TRUE;
}

static int hxt_send_desk_status(struct uwsc_client *cl, REPORT_TYPE type, int info_type)
{
    if(NULL == cl)
    {
        return -1;
    }

    //post json data to server
    cJSON *root = cJSON_CreateObject();    
    if (NULL == root)
    {
        goto END;
    }

    cJSON *data_item = NULL;
    cJSON_AddStringToObject(root, "senderId", "");
    cJSON_AddStringToObject(root, "targetId", "");
    cJSON_AddNumberToObject(root, "dataType", HXT_DESK_STATUS);
    cJSON_AddItemToObject(root, "data", data_item = cJSON_CreateObject());
    cJSON_AddNumberToObject(data_item, "reportType", type);
    cJSON_AddNumberToObject(data_item, "cameraStatus", g_camera_status);
    
    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        goto END;
    }
    utils_print("HXT_DESK_STATUS: %s %s\n", utils_get_current_time(), json_data);
    int send_count = cl->send(cl, json_data, strlen(json_data), info_type);

    utils_free(json_data);
END:
    cJSON_Delete(root);

    return send_count;
}

static void* send_study_info_cb(void *params)
{
    ReportInfo report_info;
    StudyInfo info;
    if (NULL == params)
    {
        return NULL;
    }    

    prctl(PR_SET_NAME, "hxt_send_study_info");
    pthread_detach(pthread_self());

    struct uwsc_client *cl = (struct uwsc_client *)params;

    /*get alioss token*/
    AliossOptions *opts = NULL;
    if (hxt_get_aliyun_config((void **)&opts))
    {
        utils_print("get alioss token ok\n");
    }

    while (g_recv_running)
    {
        char* ptr = get_buffer(&g_msg_buffer, sizeof(StudyInfo));
        if (NULL == ptr)
        {
            sleep(5);
            continue;
        }
        bzero(&report_info, sizeof(report_info));

        if (opts != NULL)
        {   
            utils_print("alioss expired time is %d\n", opts->expired_time);
            if (time(NULL) - opts->expired_time > 0)
            {
                deinit_upload_options((void*)opts);
                opts = NULL;
                hxt_get_aliyun_config((void **)&opts);
            }
        }

        if (!init_study_info(&report_info, (StudyInfo*)ptr, opts))
        {
            release_buffer(&g_msg_buffer, sizeof(StudyInfo));
            ptr = NULL;
            continue;
        }

        //post json data to server
        cJSON *root = cJSON_CreateObject();    
        if (NULL == root)
        {
            release_buffer(&g_msg_buffer, sizeof(StudyInfo));
            ptr = NULL;
            continue;
        }

        cJSON *data_item = NULL;
        cJSON_AddStringToObject(root, "senderId", "");
        cJSON_AddStringToObject(root, "targetId", "");
        cJSON_AddNumberToObject(root, "dataType", HXT_STUDY_INFO);
        cJSON_AddItemToObject(root, "data", data_item = cJSON_CreateObject());
        cJSON_AddNumberToObject(data_item, "childrenUnid", report_info.child_unid);
        cJSON_AddNumberToObject(data_item, "parentUnid", report_info.parent_unid);
        cJSON_AddNumberToObject(data_item, "reportType", report_info.report_type);
        cJSON_AddStringToObject(data_item, "studyDate", report_info.study_date);
        cJSON_AddStringToObject(data_item, "reportTime", report_info.report_time);
        cJSON_AddNumberToObject(data_item, "studyMode", report_info.study_mode);
        if (report_info.report_type == BAD_POSTURE)
        {
            cJSON_AddNumberToObject(data_item, "duration", report_info.duration);
            cJSON_AddStringToObject(data_item, "videoUrl", report_info.video_url);
            cJSON_AddStringToObject(data_item, "photoUrl", report_info.snap_url);
        }
        cJSON_AddNumberToObject(data_item, "cameraStatus", report_info.camera_status);

        char* json_data = cJSON_PrintUnformatted(root);
        if (NULL == json_data)
        {
            goto CLEAR;
        }
        
        cl->send(cl, json_data, strlen(json_data) + 1, UWSC_OP_TEXT);
        printf("STUDY-INFO: %s\n", json_data);

        utils_free(json_data);
CLEAR:
        cJSON_Delete(root);
        
        release_buffer(&g_msg_buffer, sizeof(StudyInfo));
        ptr = NULL;
    }

    if (opts != NULL)
    {
        deinit_upload_options((void*)opts);
        opts = NULL;
    }

    printf("Hxt send_study_info thread exit ....\n");

    return NULL;
}

static void parse_server_config_data(void *data)
{
    if(NULL == data)
    {
        return;
    }

    cJSON *root = cJSON_Parse(data);
    if(!root)
    {
        return;
    }
    
    int judge_time, video_length, video_ratio, video_count, snap_count, offline_storage, attach_ratio;
    int old_ver = 0;
    int version_id = 0;
    char* version_no;
    char* pack_url;
    int child_unid = -1, alarm_type = 2;
    
    cJSON *sub_item = NULL,  *sub_item1 = NULL, *sub_item2=NULL;
    cJSON *item = cJSON_GetObjectItem(root, "dataType");   
    int data_type = item->valueint; 
    switch(data_type)
    {
     case HXT_BASIC_CFG:
        utils_print("To process basic config...\n");
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "postureCountDuration");   //不良坐姿时长判定值
        judge_time = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "videoRecordDuration");    //视频记录时长
        video_length = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "videoRecordRatio");       //视频分辨率
        video_ratio = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "videoRecordCount");       //视频记录个数
        video_count = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "photoRecordCount");       //照片记录张数
        snap_count = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "offlineStorage");         //离线存储时长
        offline_storage = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "attachRatio");            //抽帧频次
        attach_ratio = sub_item->valueint;
        set_running_params(judge_time, video_length, video_ratio, video_count, snap_count, offline_storage, attach_ratio);
    break;
    case HXT_UPDATE_REMIND:
        utils_print("To process update...\n");
        if (!g_device_sleeping && !g_deploying_net)
        {
            item = cJSON_GetObjectItem(root, "data");
            if (item != NULL)
            {
                old_ver = get_update_version_id();
                utils_print("old version is %d\n", old_ver);
                sub_item = cJSON_GetObjectItem(item, "newVersionId");   
                if(sub_item != NULL)
                {
                    version_id = sub_item->valueint;
                }        
                utils_print("new version is %d\n", version_id);
                if (old_ver < version_id)
                {
                    sub_item1 = cJSON_GetObjectItem(item, "newVersionNo");           //新版本号
                    version_no = sub_item1->valuestring;
                    sub_item2 = cJSON_GetObjectItem(item, "upgradepackUrl");         //新版本文件地址
                    pack_url = sub_item2->valuestring;
                    set_update_params(version_id, version_no, pack_url);
                    //to upgrade
                    hxt_get_new_version_request(pack_url);
                }
            }
        }
    break;    
    case HXT_WAKE_CAMERA:
        //to wake camera
        utils_print("To wake camera...\n");
    break;
    case HXT_USER_DATA:
        utils_print("To process user data...\n");
        item = cJSON_GetObjectItem(root, "data");
        hxt_parse_user_data((void*)item);
    break;       
    case HXT_ALARM_VARRY:
        utils_print("To varry alarm file...\n");
        item = cJSON_GetObjectItem(root, "data");
        if(item != NULL)
        {
            sub_item1 = cJSON_GetObjectItem(item, "childrenUnid");
            sub_item2 = cJSON_GetObjectItem(item, "alarmType");
            if (sub_item1 != NULL && sub_item2 != NULL)
            {
                update_alarm_type(sub_item1->valueint, sub_item2->valueint);
            }
        }   
    break;
    case HXT_STUDY_MODE_VARRRY:
        //new studyMode
        utils_print("To varray study mode...\n");
        item = cJSON_GetObjectItem(root, "data");   
        if (item != NULL)
        {
            sub_item1 = cJSON_GetObjectItem(item, "childrenUnid");
            sub_item2 = cJSON_GetObjectItem(item, "studyMode");
            update_study_mode(sub_item1->valueint, sub_item2->valueint);   
        }                            
    break;
    case HXT_BIND_CHILD_ID:
        // child unid
        utils_print("To bind child id...\n");
        item = cJSON_GetObjectItem(root, "data");
        hxt_update_children_alarm_files((void*)item);
    break;
    case HXT_VARY_CHILD_ID:
        //
        utils_print("To varry child id...\n");
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "childrenUnid"); 
        // stop_posture_recognize();              //设置/变更上报数据的孩子ID
        if (sub_item != NULL)
        {
            if (sub_item->valueint > 0)
            {
                update_select_child(sub_item->valueint);
            }
        }
        // sleep(1);
        // start_posture_recognize();
    break;
    case HXT_GET_IFLYOS_TOKEN:
        utils_print("To update iflyos token or sn...\n");
        item = cJSON_GetObjectItem(root, "data");
        sub_item1 = cJSON_GetObjectItem(item, "iflyosToken");
        sub_item2 = cJSON_GetObjectItem(item, "iflyosID");
        if (sub_item1 && sub_item2)
        {
            set_iflyos_params(sub_item1->valuestring, sub_item2->valuestring);
        }  
        else if (sub_item1 && !sub_item2)
        {
            set_iflyos_params(sub_item1->valuestring, NULL);
        }
        else if (!sub_item1 && sub_item2)
        {
            set_iflyos_params(NULL, sub_item2->valuestring);
        }
    break;
    case HXT_STOP_STUDY:
        /* stop studying */
        utils_print("To stop study...\n");
        stop_posture_recognize();
    break;
    case HXT_UNBIND_DESK:
        /* device deregister */
        utils_print("To disconnect...\n");
        utils_system_reset();
        sleep(3);
        utils_system_reboot();
    break;
    case HXT_POWEROFF:
        /* power off */
        utils_print("To poweroff...\n");
    break;
    case HXT_RESTART:
        /* restart */
        utils_print("To restart...\n");
        utils_system_reboot();
    break;
    case HXT_UPDATE_URL:
        utils_print("To update url ...\n");
        item = cJSON_GetObjectItem(root, "data");
        if (item != NULL)
        {
            sub_item1 = cJSON_GetObjectItem(item, "websocketUrl");
            if (sub_item1 != NULL)
            {
                set_websocket_url(sub_item1->valuestring);
            }
            sub_item2 = cJSON_GetObjectItem(item, "uploadHostUrl");
            if (sub_item2 != NULL)
            {
                set_upload_url(sub_item2->valuestring);
            }
            // set_server_params(sub_item1->valuestring, sub_item2->valuestring);
        }
    break;
    case HXT_UPDATE_SELF_ALARM:
        utils_print("To update selfdef alarm...\n");
        item = cJSON_GetObjectItem(root, "data");
        hxt_update_children_alarm_file((void*)item);
    break;
    case HXT_UNBIND_CHILD:
        utils_print("To unbind child...\n");
        item = cJSON_GetObjectItem(root, "data");
        if (item != NULL)
        {
            sub_item1 = cJSON_GetObjectItem(item, "childrenUnid");   
            hxt_unbind_child(sub_item1->valueint);
        }
    break;
    case 0:
    default:
        //connect OK, do nothing
        utils_print("websocket connect ok\n");
    break;  
    }
    
    if(root != NULL)
    {
        cJSON_Delete(root);
    }

    utils_print("end\n");
    return;
}

static void hxt_wsc_onopen(struct uwsc_client *cl)
{
    utils_print("hxt onopen\n");

    g_hxt_wbsc_running = TRUE;
    board_set_led_status(NORMAL);
    if (g_hxt_first_login)
    {
        utils_send_local_voice(VOICE_SERVER_CONNECT_OK);//联网成功
        g_hxt_first_login = FALSE;
    }
    
    sem_post(&g_hxt_run_flag);
    g_sem_posted = TRUE;

    hxt_send_desk_status(cl, POWER_ON, UWSC_OP_TEXT);

    g_recv_running = TRUE;
    pthread_t study_info_tid;
    pthread_create(&study_info_tid, NULL, send_study_info_cb, (void *)cl);
}

static void hxt_wsc_onmessage(struct uwsc_client *cl,void *data, size_t len, bool binary)
{
    if (binary) {
        //文件
    } 
    else 
    {
        printf("[%.*s]\n", (int)len, (char *)data);
        parse_server_config_data(data);
    }
}

static void hxt_wsc_onerror(struct uwsc_client *cl, int err, const char *msg)
{
    utils_print("hxt onerror:%d: %s\n", err, msg);
    g_recv_running = FALSE;
    ev_break(cl->loop, EVBREAK_ALL);
}

static void hxt_wsc_onclose(struct uwsc_client *cl, int code, const char *reason)
{
    utils_print("hxt onclose:%d: %s\n", code, reason);
    g_recv_running = FALSE;
    ev_break(cl->loop, EVBREAK_ALL);
}

static void hxt_send_heartbeat(struct ev_loop* loop, ev_timer* timer, int e)
{
    if (NULL == g_hxt_loop)
    {
        return;
    }

    hxt_send_desk_status(hxt_wsc, HEARTBEAT, UWSC_OP_TEXT);
}

static void async_callback(EV_P_ ev_async* w, int revents)
{
    ev_break(g_hxt_loop, EVBREAK_ALL);
}

void hxt_websocket_stop()
{
    g_recv_running = FALSE;
    ev_async_init(&g_async_watcher, async_callback);
    ev_async_start(g_hxt_loop, &g_async_watcher);
    ev_async_send(g_hxt_loop, &g_async_watcher);
}

int hxt_websocket_start()
{
	int ping_interval = 120;	        /* second */
    // struct ev_loop *loop;
    struct ev_timer heartbeat_timer;

    prctl(PR_SET_NAME, "hxt_websocket");
    pthread_detach(pthread_self());

    /* init aliyun env */
    hxt_init_aliyun_env();

    char* hxt_url = get_websocket_url();
    if (hxt_url != NULL)
    {
        hxt_get_token_request();
    }

    hxt_url = get_websocket_url();
    char* token = get_server_token();
    char extra_header[1024] = {0};
    strcpy(extra_header, "Sec-WebSocket-Protocol: ");
    strcat(extra_header, "Bearer ");
    strcat(extra_header, token);
    strcat(extra_header, "\r\n");

    g_hxt_loop = ev_loop_new(EVFLAG_AUTO);

    hxt_wsc = uwsc_new(g_hxt_loop, hxt_url, -1, extra_header);
    if (!hxt_wsc)
    {
        utils_print("hxt init failed\n");
        return -1;
    }
         
    utils_free(token);     
    utils_free(hxt_url);     
	printf("%s -- Hxt websocket start ...\n", utils_get_current_time());

    hxt_wsc->onopen = hxt_wsc_onopen;
    hxt_wsc->onmessage = hxt_wsc_onmessage;
    hxt_wsc->onerror = hxt_wsc_onerror;
    hxt_wsc->onclose = hxt_wsc_onclose;

    ev_timer_init(&heartbeat_timer, hxt_send_heartbeat, 0.0, ping_interval);
    ev_timer_start(g_hxt_loop, &heartbeat_timer);

    ev_run(g_hxt_loop, 0);

    free(hxt_wsc);
    
    if (!g_sem_posted)
    {
        sem_post(&g_hxt_run_flag);
    }

    g_sem_posted = FALSE;
    hxt_deinit_aliyun_env();
    g_hxt_wbsc_running = FALSE;
    if (!g_device_sleeping)
    {
        board_set_led_status(NET_ERR);
    }
        
    printf("Hxt websocket exit...\n");   

    return 0;  
}

static void* hxt_websocket_cb(void* data)
{
    hxt_websocket_start();
    return NULL;
}

