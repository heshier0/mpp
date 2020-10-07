#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>


#include <uwsc/uwsc.h>

#include "utils.h"
#include "hxt_defines.h"
#include "databuffer.h"
#include "db.h"
#include "hxt_client.h"

typedef enum 
{
    POWER_ON = 1,
    POWER_OFF,
    HEARTBEAT
}REPORT_TYPE;

extern BOOL g_hxt_wbsc_running;
extern BOOL g_hxt_first_login;
extern DATABUFFER g_msg_buffer;

static BOOL g_recv_running = TRUE;
pthread_t study_info_tid;

static BOOL init_study_info(ReportInfo *report_info, StudyInfo *study_info)
{
    if(NULL == report_info || NULL == study_info)
    {
        return FALSE;
    }
    
    if(study_info->info_type < 1 || study_info->info_type > 5)
    {
        utils_print("Study info type ERROR\n");
        return FALSE;
    }
    report_info->camera_status = 1;//hxt_get_camera_status();
    report_info->parent_unid = get_parent_id(); //hxt_get_parent_unid_cfg();
    report_info->child_unid = get_select_child_id(); //hxt_get_child_unid();
    report_info->report_type = study_info->info_type;
    strcpy(report_info->study_date, utils_date_to_string());
    strcpy(report_info->report_time, utils_time_to_string());
    report_info->study_mode = get_study_mode(report_info->child_unid);//hxt_get_study_mode_cfg(report_info->child_unid);
    if (report_info->report_type == BAD_POSTURE)
    {
        report_info->duration = 10;
        if(hxt_file_upload_request(study_info->file, report_info->study_date, report_info->video_url))
        {
            remove(study_info->file);
        }
        if (hxt_file_upload_request(study_info->snap, report_info->study_date, report_info->snap_url))
        {
            remove(study_info->snap);
        }
    }

    
    return TRUE;
}

static void hxt_send_desk_status(struct uwsc_client *cl, REPORT_TYPE type, int info_type)
{
    if(NULL == cl)
    {
        return;
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
    cJSON_AddNumberToObject(data_item, "cameraStatus", 1);
    
    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        goto END;
    }
    utils_print("HXT_DESK_STATUS: %s\n", json_data);
    cl->send(cl, json_data, strlen(json_data), info_type);

    utils_free(json_data);
END:
    cJSON_Delete(root);
    return;
}

static void* send_study_info_cb(void *params)
{
    ReportInfo report_info;
    StudyInfo info;
    if (NULL == params)
    {
        return NULL;
    }    
    struct uwsc_client *cl = (struct uwsc_client *)params;

    while (g_recv_running)
    {
        char* ptr = get_buffer(&g_msg_buffer, sizeof(StudyInfo));
        if (NULL == ptr)
        {
            sleep(5);
            continue;
        }
        bzero(&report_info, sizeof(report_info));
        init_study_info(&report_info, (StudyInfo*)ptr);
        //post json data to server
        cJSON *root = cJSON_CreateObject();    
        if (NULL == root)
        {
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
        utils_print("STUDY-INFO: %s\n", json_data);
        cl->send(cl, json_data, strlen(json_data), UWSC_OP_TEXT);

        utils_free(json_data);
CLEAR:
        cJSON_Delete(root);
        release_buffer(&g_msg_buffer, sizeof(StudyInfo));
        ptr = NULL;
    }

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
    
    int old_ver = 0;
    cJSON *sub_item = NULL,  *sub_item1 = NULL, *sub_item2=NULL;
    cJSON *item = cJSON_GetObjectItem(root, "dataType");
    int data_type = item->valueint;
    int judge_time, video_length, video_ratio, video_count, snap_count, offline_storage, attach_ratio;
    int version_id;
    char* version_no;
    char* pack_url;
    switch(data_type)
    {
     case HXT_BASIC_CFG:
        utils_print("To process basic config...\n");
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "postureCountDuration");   //不良坐姿时长判定值
        // hxt_set_posture_judge_cfg(sub_item->valueint);
        judge_time = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "videoRecordDuration");    //视频记录时长
        // hxt_set_video_length_cfg(sub_item->valueint);
        video_length = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "videoRecordRatio");       //视频分辨率
        // hxt_set_video_ratio_cfg(sub_item->valueint);
        video_ratio = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "videoRecordCount");       //视频记录个数
        // hxt_set_video_count_cfg(sub_item->valueint);
        video_count = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "photoRecordCount");       //照片记录张数
        // hxt_set_photo_count_cfg(sub_item->valueint);
        snap_count = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "offlineStorage");         //离线存储时长
        // hxt_set_offline_storage_cfg(sub_item->valueint);
        offline_storage = sub_item->valueint;
        sub_item = cJSON_GetObjectItem(item, "attachRatio");            //抽帧频次
        attach_ratio = sub_item->valueint;
        // hxt_set_attach_ratio_cfg(sub_item->valueint);
        // hxt_reload_cfg();
        set_running_params(judge_time, video_length, video_ratio, video_count, snap_count, offline_storage, attach_ratio);
    break;
    case HXT_UPDATE_REMIND:
        utils_print("To process update...\n");
        item = cJSON_GetObjectItem(root, "data");
        if (item != NULL)
        {
            old_ver = get_update_version_id();//hxt_get_version_id_cfg();
            sub_item = cJSON_GetObjectItem(item, "newVersionId");           //新版本id，大于0时有效
            version_id = sub_item->valueint;
            if (old_ver < version_id)
            {
                //hxt_set_version_id_cfg(sub_item->valueint);
                sub_item1 = cJSON_GetObjectItem(item, "newVersionNo");           //新版本号
                // hxt_set_version_cfg(sub_item->valuestring);
                version_no = sub_item1->valuestring;
                sub_item2 = cJSON_GetObjectItem(item, "upgradepackUrl");         //新版本文件地址
                // hxt_set_upgrade_pack_url_cfg(sub_item->valuestring);
                pack_url = sub_item2->valuestring;
                // hxt_reload_cfg();
                set_update_params(version_id, version_no, pack_url);
                //to upgrade
                hxt_get_new_version_request(pack_url);
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
        // hxt_reload_cfg();
    break;       
    case HXT_ALARM_VARRY:
        utils_print("To varry alarm file...\n");
        item = cJSON_GetObjectItem(root, "data");
        hxt_update_children_alarm_files((void*)item);
        // hxt_reload_cfg();
    break;
    case HXT_STUDY_MODE_VARRRY:
        //new studyMode
        utils_print("To varray study mode...\n");
        item = cJSON_GetObjectItem(root, "data");                           
        sub_item = cJSON_GetObjectItem(item, "studyMode");
        update_study_mode(get_select_child_id(), sub_item->valueint);               
        // hxt_set_study_mode_cfg(hxt_get_child_unid(), sub_item->valueint);
        // hxt_reload_cfg();
    break;
    case HXT_BIND_CHILD_ID:
        // child unid
        utils_print("To bind child id...\n");
        item = cJSON_GetObjectItem(root, "data");
        hxt_update_children_alarm_files((void*)item);
        // hxt_reload_cfg();
    break;
    case HXT_VARY_CHILD_ID:
        //
        utils_print("To varry child id...\n");
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "childrenUnid");               //设置/变更上报数据的孩子ID
        stop_posture_recognize();
        // hxt_set_child_unid(sub_item->valueint);
        update_select_child(sub_item->valueint);
        sleep(1);
        start_posture_recognize();
    break;
    case HXT_GET_IFLYOS_TOKEN:
        utils_print("To update iflyos token or sn...\n");
        item = cJSON_GetObjectItem(root, "data");
        sub_item1 = cJSON_GetObjectItem(item, "iflyosToken");
        // if (sub_item1)
        // {
            // hxt_set_iflyos_token_cfg(sub_item->valuestring);
        // }
        sub_item2 = cJSON_GetObjectItem(item, "iflyosID");
        // if (sub_item2)
        // {
            // utils_print("to update iflyos id");
            // hxt_set_iflyos_sn_cfg(sub_item->valuestring);
        // }
        // hxt_reload_cfg();
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
    case HXT_DISCONNECT:
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

    hxt_send_desk_status(cl, POWER_ON, UWSC_OP_TEXT);
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

static void hxt_wsc_ping(struct uwsc_client *cl)
{
    hxt_send_desk_status(cl, HEARTBEAT, UWSC_OP_PING);
}

int hxt_websocket_start()
{
	int ping_interval = 120;	        /* second */
    struct uwsc_client *cl;
    struct ev_loop *loop;

    char* hxt_url = get_websocket_url();
    char* token = get_server_token();
    char extra_header[1024] = {0};
    strcpy(extra_header, "Sec-WebSocket-Protocol: ");
    strcat(extra_header, "Bearer ");
    strcat(extra_header, token);
    strcat(extra_header, "\r\n");

    loop = ev_loop_new(EVFLAG_AUTO);

    cl = uwsc_new(loop, hxt_url, ping_interval, extra_header);
    if (!cl)
    {
        utils_print("hxt init failed\n");
        return -1;
    }
         
    utils_free(token);     
    utils_free(hxt_url);     
	utils_print("%s -- Hxt websocket start ...\n", utils_get_current_time());

    cl->onopen = hxt_wsc_onopen;
    cl->onmessage = hxt_wsc_onmessage;
    cl->onerror = hxt_wsc_onerror;
    cl->onclose = hxt_wsc_onclose;
    cl->ping = hxt_wsc_ping;

    ev_run(loop, 0);

    free(cl);
    
    g_hxt_wbsc_running = FALSE;
    utils_print("Hxt websocket exit...\n");   

    return 0;  
}

