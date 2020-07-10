#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <uwsc/uwsc.h>

#include "utils.h"
#include "hxt_defines.h"

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
        sub_item = cJSON_GetObjectItem(item, "postureCountDuration");
        hxt_set_posture_judge_cfg(sub_item->valueint);
        sub_item = cJSON_GetObjectItem(item, "videoRecordDuration");
        hxt_set_video_length_cfg(sub_item->valueint);
        sub_item = cJSON_GetObjectItem(item, "videoRecordRatio");
        hxt_set_video_ratio_cfg(sub_item->valueint);
        sub_item = cJSON_GetObjectItem(item, "videoRecordCount");
        hxt_set_video_count_cfg(sub_item->valueint);
        sub_item = cJSON_GetObjectItem(item, "photoRecordCount");
        hxt_set_photo_count_cfg(sub_item->valueint);
        hxt_reload_cfg();
    break;
    case 2:
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "newVersionId");
        hxt_set_version_id_cfg(sub_item->valueint);
        sub_item = cJSON_GetObjectItem(item, "newVersionNo");
        hxt_set_version_cfg(sub_item->valuestring);
        sub_item = cJSON_GetObjectItem(item, "upgradepackUrl");
        hxt_set_upgrade_pack_url_cfg(sub_item->valuestring);
        hxt_reload_cfg();
        //to upgrade

    break;    
    case 3:
        //to wake camera
    break;
    case 4:
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "alarmUnid");
        hxt_set_alarm_unid_cfg(sub_item->valueint);
        sub_item = cJSON_GetObjectItem(item, "alarmFileUrl");
        hxt_set_alarm_file_url_cfg(sub_item->valuestring);
        hxt_reload_cfg();
    break;       
    case 5:
        item = cJSON_GetObjectItem(root, "data");
        cJSON *sub_item1 = cJSON_GetObjectItem(item, "childrenUnid");
        cJSON *sub_item2 = cJSON_GetObjectItem(item, "alarmType");
        hxt_set_alarm_type_cfg(sub_item1->valueint, sub_item2->valueint);
        hxt_reload_cfg();
    break;
    case 6:
        //check code to sound
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "checkCode");
        // play_sound_check_code(sub_item->valuestring);
    break;
    case 7:
        // child unid
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "childrenUnid");
        hxt_set_children_unid(sub_item->valueint);
    break;
    case 8:
        //
        item = cJSON_GetObjectItem(root, "data");
        sub_item = cJSON_GetObjectItem(item, "childrenUnid");
        hxt_set_children_unid(sub_item->valueint);
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


static void uwsc_onopen(struct uwsc_client *cl)
{
    uwsc_log_info("onopen\n");

    //added by hekai
    //end added
}

static void uwsc_onmessage(struct uwsc_client *cl,
	void *data, size_t len, bool binary)
{
    printf("Recv:\n");

    if (binary) {
        //文件
    } 
    else 
    {
        printf("[%.*s]\n", (int)len, (char *)data);
        parse_server_config_data(data);

    }
}

static void uwsc_onerror(struct uwsc_client *cl, int err, const char *msg)
{
    utils_print("onerror:%d: %s\n", err, msg);
    ev_break(cl->loop, EVBREAK_ALL);
}

static void uwsc_onclose(struct uwsc_client *cl, int code, const char *reason)
{
    utils_print("onclose:%d: %s\n", code, reason);
    ev_break(cl->loop, EVBREAK_ALL);
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
	int ping_interval = 10;	/* second */
    struct uwsc_client *cl;

    char* hxt_url = hxt_get_websocket_url_cfg();
    char* token = hxt_get_token_cfg();
    char extra_header[1024] = {0};
    strcpy(extra_header, "Sec-WebSocket-Protocol: ");
    strcat(extra_header, "Bearer ");
    strcat(extra_header, token);
    strcat(extra_header, "\r\n");
    utils_print("extra_header:[%s]\n", extra_header);

    cl = uwsc_new(loop, hxt_url, ping_interval, extra_header);
    if (!cl)
    {
        utils_print("hxt webosocket init failed\n");
        return -1;
    }
        
	utils_print("Hxt webosocket start connect...\n");

    cl->onopen = uwsc_onopen;
    cl->onmessage = uwsc_onmessage;
    cl->onerror = uwsc_onerror;
    cl->onclose = uwsc_onclose;

    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &signal_watcher);

    ev_run(loop, 0);

    free(cl);
       
    return 0;
}