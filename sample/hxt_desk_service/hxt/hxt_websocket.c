#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <uwsc/uwsc.h>

#include "utils.h"
#include "hxt_defines.h"

typedef enum 
{
    POWER_ON = 1,
    POWER_OFF,
    HEARTBEAT
}REPORT_TYPE;

typedef enum
{
    STUDY_BEGIN = 1,
    STUDY_END,
    CHILD_AWAY, 
    CHILD_BACK,
    BAD_POSTURE
}STUDY_REPORT;

pthread_t study_info_tid;

static void* send_study_info_cb(void *params)
{
    if (NULL == params)
    {
        return NULL;
    }    
    struct uwsc_client *cl = (struct uwsc_client *)data;

    
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

static void hxt_send_heartbeat_info(struct uwsc_client *cl)
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
    cJSON_AddNumberToObject(data_item, "reportType", HEARTBEAT);
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

static void hxt_send_study_info(struct uwsc_client *cl)
{
    int child_id = 0;
    int parent_id = 0;
    int report_type = STUDY_BEGIN;
    int study_mode = 0; //hxt_get_study_mode_cfg();
    char study_date[32] = {0};
    char report_time[32] = {0};
    int duration = 0;
    char video_url[256] = {0};
    char photo_url[256] = {0};
    int camera_status = 1;

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
    cJSON_AddNumberToObject(root, "dataType", HXT_STUDY_INFO);
    cJSON_AddItemToObject(root, "data", data_item = cJSON_CreateObject());
    cJSON_AddNumberToObject(data_item, "childrenUnid", child_id);
    cJSON_AddNumberToObject(data_item, "parentUnid", parent_id;
    cJSON_AddStringToObject(data_item, "studyDate", study_date);
    cJSON_AddStringToObject(data_item, "reportTime", report_time);
    cJSON_AddNumberToObject(data_item, "studyMode", study_mode);
    if(study_mode == BAD_POSTURE)
    {
        cJSON_AddNumberToObject(data_item, "duration", duration);
        cJSON_AddStringoObject(data_item, "videoUrl", video_url);
        cJSON_AddStringoObject(data_item, "photoUrl", photo_url);
    }
    cJSON_AddNumberToObject(data_item, "cameraStatus", camera_status);
    
    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        return;
    }
    utils_print("Study info: %s\n", json_data);
    cl->send(cl, json_data, strlen(json_data), UWSC_OP_TEXT);

    if(root != NULL)
    {
        cJSON_Delete(root);
    }

    return;
}

static void hxt_wsc_onopen(struct uwsc_client *cl)
{
    utils_print("hxt onopen\n");

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
    hxt_send_heartbeat_info(cl);
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

    free(cl);
       
    return 0;
}