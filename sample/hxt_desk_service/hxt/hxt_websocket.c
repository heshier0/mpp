#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <uwsc/uwsc.h>

#include "common.h"
#include "utils.h"
#include "hxt_defines.h"

typedef enum 
{
    POWER_ON = 1,
    POWER_OFF,
    HEARTBEAT
}REPORT_TYPE;



pthread_t study_info_tid;

static BOOL init_study_info(StudyInfo *study_info, int type)
{
    if(NULL == study_info)
    {
        return FALSE;
    }

    study_info->parent_unid = hxt_get_parent_unid_cfg();
    study_info->child_unid = hxt_get_children_unid();
    study_info->report_type = type;
    
    char* study_date = utils_date_to_string();
    if(study_info->study_date == NULL)
    {
        study_info->study_date = utils_malloc(strlen(study_date) + 1);
    }
    strcpy(study_info->study_date, study_date);
    study_info->study_date[strlen(study_date)] = '\0';
    
    char* report_time = utils_time_to_string();
    if (study_info->report_time == NULL)
    {
        study_info->report_type = utils_malloc(strlen(report_time) + 1);
    }
    strcpy(study_info->report_time, report_time);
    
    study_info->study_mode = hxt_get_study_mode_cfg(study_info->child_unid);

    if (study_info->report_type == 5)
    {
        study_info->duration = 10;

        if (study_info->video_url == NULL)
        {
            study_info->video_url = utils_malloc(strlen(g_video_url) + 1);
        }
        strcpy(study_info->video_url, g_video_url);
        study_info->video_url[strlen(g_video_url)] = '\0';

        if (study_info->photo_url == NULL)
        {
            study_info->photo_url = utils_malloc(strlen(g_photo_url) + 1);
        }
        strcpy(study_info->photo_url, g_photo_url);
        study_info->photo_url[strlen(g_photo_url)] = '\0';
    }
    study_info->camera_status = hxt_get_camera_status();

    return TRUE;
}

static void deinit_study_nifo(StudyInfo *study_info)
{
    if (NULL == study_info)
    {
        return;
    }
    if(study_info->video_url != NULL)
    {
        utils_free(study_info->video_url);
    }
    if(study_info->photo_url != NULL)
    {
        utils_free(study_info->photo_url);
    }
    if(study_info->study_date ！= NULL)
    {
        utils_free(study_info->study_date);
    }
    if(study_info->report_time != NULL)
    {
        utils_free(study_info->report_time);
    }

    return;
}

static void hxt_send_desk_status(struct uwsc_client *cl, REPORT_TYPE type)
{
    if(NULL == cl)
    {
        return;
    }

    //post json data to server
    cJSON *root = cJSON_CreateObject();    
    if (NULL == root)
    {
        return;
    }

    cJSON *data_item = NULL;
    cJSON_AddStringToObject(root, "senderId", "");
    cJSON_AddStringToObject(root, "targetId", "");
    cJSON_AddNumberToObject(root, "dataType", HXT_DESK_STATUS);
    cJSON_AddItemToObject(root, "data", data_item = cJSON_CreateObject());
    cJSON_AddNumberToObject(data_item, "reportType", type);
    cJSON_AddNumberToObject(data_item, "cameraStatus", hxt_get_camera_status());
    
    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        return;
    }
    utils_print("HEARTBEAT: %s\n", json_data);
    cl->send(cl, json_data, strlen(json_data), UWSC_OP_PING);

    if(root != NULL)
    {
        cJSON_Delete(root);
    }

    return;
}

static void* send_study_info_cb(void *params)
{
    StudyInfo study_info;

    if (NULL == params)
    {
        return NULL;
    }    
    struct uwsc_client *cl = (struct uwsc_client *)params;

    STUDY_REPORT_TYPE type;

    if (-1 == utils_recv_msg(&type, sizeof(STUDY_REPORT_TYPE)))
    {
        utils_print("recv study info failed\n");
        return NULL;
    }
    init_study_info(&study_info, type);
    //post json data to server
    cJSON *root = cJSON_CreateObject();    
    if (NULL == root)
    {
        return;
    }

    cJSON *data_item = NULL;
    cJSON_AddStringToObject(root, "senderId", "");
    cJSON_AddStringToObject(root, "targetId", "");
    cJSON_AddNumberToObject(root, "dataType", HXT_STUDY_INFO);
    cJSON_AddItemToObject(root, "data", data_item = cJSON_CreateObject());
    cJSON_AddNumberToObject(data_item, "childrenUnid", study_info.child_unid);
    cJSON_AddNumberToObject(data_item, "parentUnid", study_info.parent_unid);
    cJSON_AddNumberToObject(data_item, "reportType", study_info.report_type);
    cJSON_AddStringoObject(data_item, "studyDate", study_info.study_date);
    cJSON_AddStringoObject(data_item, "reportTime", study_info.report_time);
    cJSON_AddNumberToObject(data_item, "studyMode", study_info.study_mode);
    if (study_info.study_mode == BAD_POSTURE)
    {
        cJSON_AddNumberToObject(data_item, "duration", study_info.duration);
        cJSON_AddStringoObject(data_item, "videoUrl", study_info.video_url);
        cJSON_AddStringoObject(data_item, "photoUrl", study_info.photo_url);
    }
    cJSON_AddNumberToObject(data_item, "cameraStatus", study_info.camera_status);

    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        return;
    }
    utils_print("STUDY-INFO: %s\n", json_data);
    cl->send(cl, json_data, strlen(json_data), UWSC_OP_TEXT);

    if(root != NULL)
    {
        cJSON_Delete(root);
    }

    deinit_study_nifo(&study_info);
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
    
    cJSON *sub_item = NULL;
    cJSON *item = cJSON_GetObjectItem(root, "senderId");
    item = cJSON_GetObjectItem(root, "targetId");
    item = cJSON_GetObjectItem(root, "dataType");
    int data_type = item->valueint;
    switch(data_type)
    {
     case 1:
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "postureCountDuration");   //不良坐姿时长判定值
        hxt_set_posture_judge_cfg(sub_item->valueint);
        sub_item = cJSON_GetObjectItem(item, "videoRecordDuration");    //视频记录时长
        hxt_set_video_length_cfg(sub_item->valueint);
        sub_item = cJSON_GetObjectItem(item, "videoRecordRatio");       //视频记录时长
        hxt_set_video_ratio_cfg(sub_item->valueint);
        sub_item = cJSON_GetObjectItem(item, "videoRecordCount");       //视频记录个数
        hxt_set_video_count_cfg(sub_item->valueint);
        sub_item = cJSON_GetObjectItem(item, "photoRecordCount");       //照片记录张数
        hxt_set_photo_count_cfg(sub_item->valueint);
        hxt_reload_cfg();
    break;
    case 2:
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "newVersionId");           //新版本id，大于0时有效
        hxt_set_version_id_cfg(sub_item->valueint);
        sub_item = cJSON_GetObjectItem(item, "newVersionNo");           //新版本号
        hxt_set_version_cfg(sub_item->valuestring);
        sub_item = cJSON_GetObjectItem(item, "upgradepackUrl");         //新版本文件地址
        hxt_set_upgrade_pack_url_cfg(sub_item->valuestring);
        hxt_reload_cfg();
        //to upgrade

    break;    
    case 3:
        //to wake camera
    break;
    case 4:
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "alarmUnid");          //书桌提示音ID
        hxt_set_alarm_unid_cfg(sub_item->valueint); 
        sub_item = cJSON_GetObjectItem(item, "alarmFileUrl");       //自定义语音时文件地址
        hxt_set_alarm_file_url_cfg(sub_item->valuestring);
        hxt_reload_cfg();
    break;       
    case 5:
        item = cJSON_GetObjectItem(root, "data");
        cJSON *sub_item1 = cJSON_GetObjectItem(item, "childrenUnid");       //孩子ID
        cJSON *sub_item2 = cJSON_GetObjectItem(item, "alarmType");          //提醒方式 ：0-静音 1-蜂鸣 2-语音
        hxt_set_alarm_type_cfg(sub_item1->valueint, sub_item2->valueint);
        hxt_reload_cfg();
    break;
    case 6:
        //check code to sound
        item = cJSON_GetObjectItem(root, "data");                           
        sub_item = cJSON_GetObjectItem(item, "checkCode");                  //验证码内容
        /* play check code */
    break;
    case 7:
        // child unid
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "childrenUnid");              //书桌新关联孩子
        hxt_set_children_unid(sub_item->valueint);
    break;
    case 8:
        //
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "childrenUnid");               //设置/变更上报数据的孩子ID
        hxt_set_children_unid(sub_item->valueint);
    break;
    case 10:
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "iflyosToken");
        iflyos_set_token(sub_item->valuestring);
    break;
    case 14:
        /* stop studying */
    break;
    case 15:
        /* disconnect */
    break;
    case 16:
        /* power off */
    break;
    case 17:
        /* restart */
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

    return;
}

static void hxt_wsc_onopen(struct uwsc_client *cl)
{
    utils_print("hxt onopen\n");

    hxt_send_desk_status(cl, POWER_ON);
    pthread_create(&study_info_tid, NULL, send_study_info_cb, (void *)cl);
}

static void hxt_wsc_onmessage(struct uwsc_client *cl,void *data, size_t len, bool binary)
{
    utils_print("hxt recv:\n");

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
    ev_break(cl->loop, EVBREAK_ALL);
}

static void hxt_wsc_onclose(struct uwsc_client *cl, int code, const char *reason)
{
    utils_print("hxt onclose:%d: %s\n", code, reason);
    ev_break(cl->loop, EVBREAK_ALL);
}

static void hxt_wsc_ping(struct uwsc_client *cl)
{
    hxt_send_desk_status(cl, HEARTBEAT);
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    if (w->signum == SIGINT) {
        ev_break(loop, EVBREAK_ALL);
        utils_print("Normal quit\n");
    }
}

int hxt_websocket_start()
{
    struct ev_loop *loop = EV_DEFAULT;
    struct ev_signal signal_watcher;
	int ping_interval = 120;	        /* second */
    struct uwsc_client *cl;

    char* hxt_url = hxt_get_websocket_url_cfg();
    char* token = hxt_get_token_cfg();
    char extra_header[1024] = {0};
    strcpy(extra_header, "Sec-WebSocket-Protocol: ");
    strcat(extra_header, "Bearer ");
    strcat(extra_header, token);
    strcat(extra_header, "\r\n");

    cl = uwsc_new(loop, hxt_url, ping_interval, extra_header);
    if (!cl)
    {
        utils_print("hxt init failed\n");
        return -1;
    }
        
	utils_print("Hxt start connect...\n");

    cl->onopen = hxt_wsc_onopen;
    cl->onmessage = hxt_wsc_onmessage;
    cl->onerror = hxt_wsc_onerror;
    cl->onclose = hxt_wsc_onclose;
    cl->ping = hxt_wsc_ping;

    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &signal_watcher);

    ev_run(loop, 0);

    hxt_send_desk_status(cl, POWER_OFF);
    free(cl);
       
    return 0;
}