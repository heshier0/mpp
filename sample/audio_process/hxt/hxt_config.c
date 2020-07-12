#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cJSON.h>

#include "utils.h"

#define HXT_CFG          "/user/config/hxt_config.json"


static cJSON* g_cfg_root  = NULL;     //指向配置文件的Object
static int g_children_unid = 0;
static char g_desk_sn_code[64] = {0};

void hxt_load_cfg()
{
    g_cfg_root = utils_load_cfg(HXT_CFG);
}

BOOL hxt_reload_cfg()
{
    return utils_reload_cfg(HXT_CFG, g_cfg_root);
}

void hxt_unload_cfg()
{
    utils_unload_cfg(g_cfg_root);
}

void hxt_set_children_unid(const int unid)
{
    g_children_unid = unid;
}

int hxt_get_children_unid()
{
    return g_children_unid;
}

void hxt_set_desk_sn_code(const char* desk_code)
{
    if(desk_code != NULL)
    {
        strcpy(g_desk_sn_code, desk_code);
    }
    
}

char* hxt_get_desk_sn_code()
{
    return g_desk_sn_code;
}

//data must json formatted data
void hxt_init_cfg(void* data)
{
    if (NULL == data)
    {
        return;
    }
    cJSON* root = cJSON_Parse(data);
    if(NULL == root)
    {
        return;
    }
    cJSON* returnObject = cJSON_GetObjectItem(root, "returnObject");

    cJSON* item = cJSON_GetObjectItem(returnObject, "token");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "token", item->valuestring);

    item = cJSON_GetObjectItem(returnObject, "websocketUrl");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "websocketUrl", item->valuestring);

    item = cJSON_GetObjectItem(returnObject, "uploadHostUrl");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "uploadHostUrl", item->valuestring);

    item = cJSON_GetObjectItem(returnObject, "upgradePackUrl");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "upgradePackUrl", item->valuestring);

    item = cJSON_GetObjectItem(returnObject, "alarmFileUrl");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "alarmFileUrl", item->valuestring);

    item = cJSON_GetObjectItem(returnObject, "postureCountDuration");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "judgeTime", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "videoRecordDuration");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "videoLength", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "videoRecordRatio");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "vidoeRatio", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "videoRecordCount");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "videoCount", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "photoRecordCount");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "photoCount", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "alarmUnid");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "alarmUnid", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "newVersionId");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "version", "versionId", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "newVersionNo");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "version", "versionNo", item->valuestring);

    item = cJSON_GetObjectItem(returnObject, "parentUnid");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "user", "parentId", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "childrenData");
    int item_count = cJSON_GetArraySize(item);
    for(int i = 0; i < item_count; i ++)
    {
        cJSON *node = cJSON_GetArrayItem(item, i);
        if (!node)
        {
            continue;
        }
        cJSON *node_item = cJSON_GetObjectItem(node, "childrenUnid");
        char children_unid[64] = {0};
        sprintf(children_unid, "child_%d", node_item->valueint);
        node_item = cJSON_GetObjectItem(node, "alarmType");
        utils_set_cfg_number_value(g_cfg_root, HXT_CFG, children_unid, "alarmType", node_item->valueint);
        node_item = cJSON_GetObjectItem(node, "studyMode");
        utils_set_cfg_number_value(g_cfg_root, HXT_CFG, children_unid, "studyMode", node_item->valueint);
    }

    hxt_reload_cfg();

    if (root != NULL)
    {
        cJSON_Delete(root);
        root = NULL;
    }

    return;
}

//get
char* hxt_get_desk_uuid_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "desk", "uuid");
}

char* hxt_get_server_url_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "server", "url");
}

char* hxt_get_api_version_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "server", "api_ver");
}

char* hxt_get_token_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "server", "token");
}

char* hxt_get_websocket_url_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "server", "websocketUrl");
}

char* hxt_get_upload_host_url_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "server", "uploadHostUrl");
}

char* hxt_get_upgrade_pack_url_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "server", "upgradePackUrl");
}

char* hxt_get_alarm_file_url_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "server", "alramFileUrl");
}

int hxt_get_posture_judge_cfg()
{
    return utils_get_cfg_number_value(g_cfg_root, "device", "judgeTime");
}

int hxt_get_video_length_cfg()
{
    return utils_get_cfg_number_value(g_cfg_root, "device", "videoLength");
}

int hxt_get_video_ratio_cfg()
{
    return utils_get_cfg_number_value(g_cfg_root, "device", "vidoeRatio");
}

int hxt_get_video_count_cfg()
{
    return utils_get_cfg_number_value(g_cfg_root, "device", "videoCount");
}

int hxt_get_photo_count_cfg()
{
    return utils_get_cfg_number_value(g_cfg_root, "device", "photoCount");
}

int hxt_get_alarm_unid_cfg()
{
    return utils_get_cfg_number_value(g_cfg_root, "device", "alarmUnid");
}

int hxt_get_version_id_cfg()
{
    return utils_get_cfg_number_value(g_cfg_root, "version", "versionId");
}

char* hxt_get_version_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "version", "versionNo");
}

int hxt_get_parent_unid_cfg()
{
    return utils_get_cfg_number_value(g_cfg_root, "user", "parentId");
}

int hxt_get_study_mode_cfg(const int unid)
{
    char child_index[64] = {0};
    sprintf(child_index, "child_%d", unid);

    return utils_get_cfg_number_value(g_cfg_root, child_index, "studyMode");
}

int hxt_get_alarm_type_cfg(const int unid)
{
    char child_index[64] = {0};
    sprintf(child_index, "child_%d", unid);

    return utils_get_cfg_number_value(g_cfg_root, child_index, "alarmType");
}

char* hxt_get_wifi_ssid_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "wifi", "ssid");
}

char* hxt_get_wifi_bssid_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "wifi", "bssid");
}

char* hxt_get_wifi_pwd_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "wifi", "pwd");
}

char* hxt_get_wifi_check_code_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "wifi", "checkCode");
}

char* hxt_get_desk_code_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "desk", "deskCode");
}
//set 
BOOL hxt_set_desk_uuid_cfg(const char* value)
{
    if (NULL == value)
    {
        return FALSE;
    }
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "desk", "uuid", value);
     
}

BOOL hxt_set_server_url_cfg(const char* value)
{
    if (NULL == value)
    {
        return FALSE;
    }
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "url", value);
}

BOOL hxt_set_api_version_cfg(const char* value)
{
    if (NULL == value)
    {
        return FALSE;
    }    
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "api_ver", value);
}

BOOL hxt_set_token_cfg(const char* value)
{
    if (NULL == value)
    {
        return FALSE;
    }       
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "token", value);
}

BOOL hxt_set_websocket_url_cfg(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }

    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "websocketUrl", value);
}

BOOL hxt_set_upload_host_url_cfg(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }
    
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "uploadHostUrl", value);
}

BOOL hxt_set_upgrade_pack_url_cfg(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }

    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "upgradePackUrl", value);
}

BOOL hxt_set_alarm_file_url_cfg(const  char* value)
{
    if (NULL == value)
    {
        return FALSE;
    }
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "alramFileUrl", value);
}

BOOL hxt_set_posture_judge_cfg(int value)
{
    return utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "judgeTime", value);
}

BOOL hxt_set_video_length_cfg(int value)
{
    return utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "videoLength", value);
}

BOOL hxt_set_video_ratio_cfg(int value)
{
    return utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "vidoeRatio", value);
}

BOOL hxt_set_video_count_cfg(int value)
{
    return utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "videoCount", value);
}

BOOL hxt_set_photo_count_cfg(int value)
{
    return utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "photoCount", value);
}

BOOL hxt_set_alarm_unid_cfg(int value)
{
    return utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "alarmUnid", value);
}

BOOL hxt_set_version_id_cfg(int value)
{
    return utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "version", "versionId", value);
}

BOOL hxt_set_version_cfg(const char* value)
{
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "version", "versionNo", value);
}

BOOL hxt_set_parent_unid_cfg(int value)
{
    return utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "user", "parentId", value);
}

BOOL hxt_set_study_mode_cfg(const int unid, const int value)
{
    char child_index[64] = {0};
    sprintf(child_index, "child_%d", unid);

    return utils_set_cfg_number_value(g_cfg_root, HXT_CFG, child_index, "studyMode", value);
}

BOOL hxt_set_alarm_type_cfg(const int unid, const int value)
{
    char child_index[64] = {0};
    sprintf(child_index, "child_%d", unid);

    return utils_set_cfg_number_value(g_cfg_root, HXT_CFG, child_index, "alarmType", value);
}

//wifi
BOOL hxt_set_wifi_ssid_cfg(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }

    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "wifi", "ssid", value);
}

BOOL hxt_set_wifi_bssid_cfg(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "wifi", "bssid", value);
}

BOOL hxt_set_wifi_pwd_cfg(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "wifi", "pwd", value);
}

BOOL hxt_set_wifi_check_code_cfg(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "wifi", "checkCode", value);
}

BOOL hxt_set_desk_code_cfg(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }

    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "desk", "deskCode", value);
}