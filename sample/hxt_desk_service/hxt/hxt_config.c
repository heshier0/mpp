#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <cJSON.h>

#include "utils.h"
#include "common.h"
#include "hxt_defines.h"


#define PERSONAL_ALARM_VOICE_COUNT      (5)

typedef enum 
{
    MUTE = 0, 
    BEEP = 1, 
    ORIGNAL = 2,
    PERSONAL = 3
 };

 typedef enum
 {
     NORMAL = 1, 
     MEDIUM = 2,
     STRICT = 3
 };

const int g_video_width[3] = {1280, 960, 640};
const int g_video_height[3] = {720, 540, 360};

static cJSON* g_cfg_root  = NULL;     //指向配置文件的Object
pthread_mutex_t g_hxt_cfg_lock =  PTHREAD_MUTEX_INITIALIZER;
static int g_children_unid = 1;
static int g_camera_status = 1;

static void hxt_mk_private_doc(int child_unid)
{
    /* posture video */
    char mk_dir_cmd[256] = {0};
    sprintf(mk_dir_cmd, "mkdir -p /user/child_%d/video", child_unid);
    system(mk_dir_cmd);
    /* posture snap */
    memset(mk_dir_cmd, 0, 256);
    sprintf(mk_dir_cmd, "mkdir -p /user/child_%d/snap", child_unid);
    system(mk_dir_cmd);

    /* self define voice */
    memset(mk_dir_cmd, 0, 256);
    sprintf(mk_dir_cmd, "mkdir -p /user/child_%d/alarm", child_unid);
    system(mk_dir_cmd);

    /* sample frame */
    memset(mk_dir_cmd, 0, 256);
    sprintf(mk_dir_cmd, "mkdir -p /user/sample", child_unid);
    system(mk_dir_cmd);  

    return;
}

/* init config file */
BOOL hxt_store_desk_uuid(const char* uuid)
{
    cJSON *root = utils_load_cfg(HXT_INIT_CFG);
    if (!root)
    {
        return FALSE;
    }
    if (uuid == NULL)
    {
        utils_set_cfg_str_value(root, HXT_INIT_CFG, "desk", "deskCode", "");
    }
    utils_set_cfg_str_value(root, HXT_INIT_CFG, "desk", "deskCode", uuid);

    utils_reload_cfg(HXT_INIT_CFG, root);

    cJSON_Delete(root);

    return TRUE;
}

char* hxt_get_init_desk_uuid()
{
    static char uuid[64] = {0};
    cJSON *root = utils_load_cfg(HXT_INIT_CFG);
    if (!root)
    {
        return NULL;
    }
    char* tmp = utils_get_cfg_str_value(root, "desk", "deskCode");
    if (tmp == NULL)
    {
        return NULL;
    }
    strcpy(uuid, tmp);
    uuid[strlen(uuid)] = '\0';

    cJSON_Delete(root);

    return uuid;
}

void hxt_load_cfg()
{
    pthread_mutex_lock(&g_hxt_cfg_lock);
    g_cfg_root = utils_load_cfg(HXT_CFG);
    pthread_mutex_unlock(&g_hxt_cfg_lock);
}

BOOL hxt_reload_cfg()
{
    int ret = FALSE;
    pthread_mutex_lock(&g_hxt_cfg_lock);
    ret =  utils_reload_cfg(HXT_CFG, g_cfg_root);
    pthread_mutex_unlock(&g_hxt_cfg_lock);

    return ret;
}

void hxt_unload_cfg()
{
    pthread_mutex_lock(&g_hxt_cfg_lock);
    utils_unload_cfg(g_cfg_root);
    pthread_mutex_unlock(&g_hxt_cfg_lock);
}

void hxt_set_child_unid(const int unid)
{
    g_children_unid = unid;
}

BOOL hxt_update_children_alarm_files(void* data)
{
    if (NULL == data)
    {
        return FALSE;
    }
    cJSON *node = (cJSON*)data;
    cJSON *node_item = cJSON_GetObjectItem(node, "childrenUnid");
    /* get child personal config */
    char children_unid[64] = {0};
    sprintf(children_unid, "child_%d", node_item->valueint);
    int child_unid = node_item->valueint;
    /* create documents for save mp4 and snap file */
    hxt_mk_private_doc(child_unid);

    node_item = cJSON_GetObjectItem(node, "studyMode");
    if (node_item)
    {
        utils_set_cfg_number_value(g_cfg_root, HXT_CFG, children_unid, "studyMode", node_item->valueint);       
    }

    node_item = cJSON_GetObjectItem(node, "alarmType");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, children_unid, "alarmType", node_item->valueint);
    int alarm_type = node_item->valueint;
    
    /* self defines alarm voice */
    if (alarm_type == PERSONAL)
    {
        char alarm_file[128] = {0};
        char old_alarm_file[128] = {0};
        char replace_cmd[256] = {0};

        cJSON *alarm_file_node = cJSON_GetObjectItem(node, "filePaths");
        int alarm_file_count = cJSON_GetArraySize(alarm_file_node);
        for (int count = 0; count < PERSONAL_ALARM_VOICE_COUNT; count ++)
        {
            cJSON *file_item = cJSON_GetArrayItem(alarm_file_node, count);
            if (!file_item)
            {
                continue;
            }
            node_item = cJSON_GetObjectItem(file_item, "priority");
            int idx = node_item->valueint;
            bzero(alarm_file, 128);
            sprintf(alarm_file, HXT_CHILD_ALARM_FILE_TMP, child_unid, idx);
            node_item = cJSON_GetObjectItem(file_item, "filePath");
            utils_download_file(node_item->valuestring, alarm_file);
            
            /*replace old file*/
            bzero(old_alarm_file, 128);
            sprintf(old_alarm_file, HXT_CHILD_ALARM_FILE, child_unid, idx);
            sprintf(replace_cmd, "mv %s %s", alarm_file, old_alarm_file);
            system(replace_cmd);    
        }
    }   
    return TRUE;
}

BOOL hxt_parse_user_data(void* data)
{
    BOOL result = FALSE;
    if(NULL == data)
    {
        return FALSE;
    }
    cJSON* returnObject = (cJSON*)data;

    cJSON* item = cJSON_GetObjectItem(returnObject, "parentUnid");
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
        hxt_update_children_alarm_files((void*)node);
    }
  
    return result;
}

int hxt_get_child_unid()
{
    return g_children_unid;
}

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
    if (NULL == returnObject)
    {
        utils_print("No return objects\n");
        return;
    }

    pthread_mutex_lock(&g_hxt_cfg_lock);
 
    cJSON* item = cJSON_GetObjectItem(returnObject, "websocketUrl");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "websocketUrl", item->valuestring);

    item = cJSON_GetObjectItem(returnObject, "uploadHostUrl");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "uploadHostUrl", item->valuestring);

    item = cJSON_GetObjectItem(returnObject, "iflyosToken");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "iflyosToken", item->valuestring);

    // item = cJSON_GetObjectItem(returnObject, "iflyosSN");
    // utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "iflyosSN", item->valuestring);

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

    // item = cJSON_GetObjectItem(returnObject, "alarmFileUrl");
    // utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "alarmFileUrl", item->valuestring);

    // item = cJSON_GetObjectItem(returnObject, "alarmUnid");
    // utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "alarmUnid", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "offlineStorage");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "offlineStorage", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "attachRatio");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "device", "attachRatio", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "newVersionId");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "version", "versionId", item->valueint);

    item = cJSON_GetObjectItem(returnObject, "newVersionNo");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "version", "versionNo", item->valuestring);

    item = cJSON_GetObjectItem(returnObject, "upgradePackUrl");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "version", "upgradePackUrl", item->valuestring);

    hxt_parse_user_data((void*)returnObject);

    pthread_mutex_unlock(&g_hxt_cfg_lock);

    hxt_reload_cfg();

    if (root != NULL)
    {
        cJSON_Delete(root);
        root = NULL;
    }

    return;
}

void hxt_init_token(void* data)
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
    if (NULL == returnObject)
    {
        utils_print("No return objects\n");
        return;
    }
    cJSON* item = cJSON_GetObjectItem(returnObject, "token");
    utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "token", item->valuestring);
    
    item = cJSON_GetObjectItem(returnObject, "tokenExpireTime");
    utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "server", "tokenExpireTime", item->valuedouble);

    hxt_reload_cfg();

    if (root != NULL)
    {
        cJSON_Delete(root);
        root = NULL;
    }
}

char* hxt_get_posture_detect_model_path_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "model", "detect_model");
}

char* hxt_get_posture_class_model_path_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "model", "class_model");
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

char* hxt_get_iflyos_token_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "server", "iflyosToken");
}

char* hxt_get_iflyos_cae_sn()
{
    return utils_get_cfg_str_value(g_cfg_root, "server", "iflyosSN");
}

char* hxt_get_token_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "server", "token");
}

int hxt_get_token_expire_time_cfg()
{
    long long int expire_time = utils_get_cfg_number_value(g_cfg_root, "server", "tokenExpireTime");
    return expire_time / 1000;
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
    return utils_get_cfg_str_value(g_cfg_root, "version", "upgradePackUrl");
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

int hxt_get_video_width_cfg()
{
    int video_width = 0;
    int video_ratio = hxt_get_video_ratio_cfg();
    switch (video_ratio)
    {
    case 1:
        video_width = 1280;
        break;
    case 2:
        // video_width = 960;
        // break;
    case 3:
        video_width = 640;
        break;    
    default:
        video_width = 640;
        break;
    }

    return video_width;
}

int hxt_get_video_height_cfg()
{
    int video_height =0;
    int video_ratio = hxt_get_video_ratio_cfg();
    switch (video_ratio)
    {
    case 1:
        video_height = 720;
    break;
    case 2:
    //     video_height = 540;
    // break;
    case 3:
        video_height = 360;
    break;
    default:
        video_height = 360;
    break;
    }

    return video_height;
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

char* hxt_get_offline_storage_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "device", "offlineStorage");
}

char* hxt_get_attach_ratio_cfg()
{
    return utils_get_cfg_str_value(g_cfg_root, "device", "attachRatio");
}

//set 

BOOL hxt_set_posture_detect_model_path(const char* value)
{
    if (NULL == value)
    {
        return FALSE;
    }
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "model", "detect_model", value);
}

BOOL hxt_set_posture_class_model_path(const char* value)
{
    if (NULL == value)
    {
        return FALSE;
    }
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "model", "class_model", value);
}

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

BOOL hxt_set_iflyos_token_cfg(const char* value)
{
    if (NULL == value)
    {
        return FALSE;
    }       
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "iflyosToken", value);  
}

BOOL hxt_set_iflyos_sn_cfg(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }
    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "ilfyosSN", value);
}

BOOL hxt_set_token_cfg(const char* value)
{
    if (NULL == value)
    {
        return FALSE;
    }       
    utils_print("To set token ...\n");
    BOOL ret_val = utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "server", "token", value);
    utils_print("To reload config...\n");
    hxt_reload_cfg();

    return ret_val;
}

BOOL hxt_set_token_expire_time_cfg(const double value)
{
    if (value <= 0)
    {
        return FALSE;
    }       
    return utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "server", "tokenExpireTime", value);
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

    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "version", "upgradePackUrl", value);
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

BOOL hxt_set_camera_status(int status)
{
    if(status < 1 || status > 3)
    {
        return FALSE;
    }
    g_camera_status = status;

    return TRUE;
}

int hxt_get_camera_status()
{
    return g_camera_status;
}

int hxt_get_desk_bind_status_cfg()
{
    return utils_get_cfg_number_value(g_cfg_root, "desk", "bind");
}

BOOL hxt_set_desk_bind_status_cfg(int status)
{
    int ret_val = utils_set_cfg_number_value(g_cfg_root, HXT_CFG, "desk", "bind", status);
    hxt_reload_cfg();
}

BOOL hxt_set_offline_storage_cfg(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }

    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "device", "offlineStorage", value);
}

BOOL hxt_set_attach_ratio_cfg(const char* value)
{
    if(NULL == value)
    {
        return FALSE;
    }

    return utils_set_cfg_str_value(g_cfg_root, HXT_CFG, "device", "attachRatio", value);
}

int hxt_get_selfdef_voice_count()
{
    int file_count = 0;
    int child_unid = hxt_get_child_unid();
    char CMD_GET_VOICE_COUNT[256] = {0};
    sprintf(CMD_GET_VOICE_COUNT, "ls /user/child_%d/alarm/ | wc -l", child_unid);
    system(CMD_GET_VOICE_COUNT);

    char line[64] = {0};
    FILE *fp = NULL;
    fp = popen(CMD_GET_VOICE_COUNT, "r");
    if(NULL != fp)
    {
        if(fgets(line, sizeof(line), fp) == NULL)
        {
            pclose(fp);
            return 0;
        }
        file_count = atoi(line);
    }
    pclose(fp);

    return file_count;
}

BOOL hxt_get_snap_count()
{
    int file_count = 0;
    int child_unid = hxt_get_child_unid();
    char CMD_GET_SNAP_COUNT[256] = {0};
    sprintf(CMD_GET_SNAP_COUNT, "ls /user/child_%d/snap/ | wc -l", child_unid);
    system(CMD_GET_SNAP_COUNT);

    char line[64] = {0};
    FILE *fp = NULL;
    fp = popen(CMD_GET_SNAP_COUNT, "r");
    if(NULL != fp)
    {
        if(fgets(line, sizeof(line), fp) == NULL)
        {
            pclose(fp);
            return 0;
        }
        file_count = atoi(line);
    }
    pclose(fp);

    return file_count;
}

BOOL hxt_get_video_count()
{
    int file_count = 0;
    int child_unid = hxt_get_child_unid();
    char CMD_GET_VIDEO_COUNT[256] = {0};
    sprintf(CMD_GET_VIDEO_COUNT, "ls /user/child_%d/video/ | wc -l", child_unid);
    system(CMD_GET_VIDEO_COUNT);

    char line[64] = {0};
    FILE *fp = NULL;
    fp = popen(CMD_GET_VIDEO_COUNT, "r");
    if(NULL != fp)
    {
        if(fgets(line, sizeof(line), fp) == NULL)
        {
            pclose(fp);
            return 0;
        }
        file_count = atoi(line);
    }
    pclose(fp);

    return file_count;
}
