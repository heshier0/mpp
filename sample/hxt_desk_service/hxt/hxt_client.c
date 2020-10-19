#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cJSON.h>


#include "utils.h"
#include "hxt_defines.h"
#include "db.h"
#include "hxt_client.h"
#include "board_func.h"

#define PERSONAL_ALARM_VOICE_COUNT      (5)

typedef enum 
{
    MUTE = 0, 
    BEEP = 1, 
    ORIGNAL = 2,
    PERSONAL = 3
 };

static char* hxt_get_api_url(const char* api)
{
    if(NULL == api)
    {
        return NULL;
    }
    // char* server_url = hxt_get_server_url_cfg();
    // if(NULL == server_url)
    // {   
    //     return NULL;
    // }
    // char* ver = hxt_get_api_version_cfg();
    // if(NULL == ver)
    // {
    //     return NULL;
    // }

    /* should be optimization */
    char request_url[256] = {0};
    strcpy(request_url, HXT_SERVER_URL);
    // strcat(request_url, "/api/");
    // strcat(request_url, HXT_API_VER);
    strcat(request_url, api);

    char* ret_value = (char*)utils_calloc(strlen(request_url) + 1);
    if(NULL == ret_value)
    {
        return NULL;
    }
    strcpy(ret_value, request_url);
    //ret_value[strlen(ret_value)] = '\0';

    return ret_value;
}

static char* hxt_get_upload_url(const char* api)
{
    if(NULL == api)
    {
        return NULL;
    }

    char* upload_host = get_upload_url(); //hxt_get_upload_host_url_cfg();
    if(NULL == upload_host)
    {
        return NULL;
    }

    // char* ver = hxt_get_api_version_cfg();
    // if(NULL == ver)
    // {
    //     return NULL;
    // }
    
    char request_url[512] = {0};
    strcpy(request_url, upload_host);
    strcat(request_url, "/api/v1");
    // strcat(request_url, ver);
    strcat(request_url, api);

    char* ret_value = (char*)utils_calloc(strlen(request_url) + 1);
    if(NULL == ret_value)
    {
        return NULL;
    }
    strcpy(ret_value, request_url);

    utils_free(upload_host);

    return ret_value;
}

static int hxt_get_reponse_status_code(void* data)
{
    int status_code = 0;
    if(NULL == data || strlen(data) == 0)
    {
        return NULL;
    }
    utils_print("Respnse:[%s]\n", (char*)data);

    cJSON* root = cJSON_Parse(data);
    cJSON *item = cJSON_GetObjectItem(root, "statusCode");
    if (strcmp(item->valuestring, HXT_RES_STATUS_OK) == 0)
    {
        status_code = HXT_OK;
    }
    else if (strcmp(item->valuestring, HXT_RES_NO_REG) == 0)
    {
        status_code = HXT_NO_REGISTER;
    } 
    else if (strcmp(item->valuestring, HXT_RES_AUTH_FAIL) == 0)
    {
        status_code = HXT_AUTH_FAILED;
    }
    else if (strcmp(item->valuestring, HXT_RES_BIND_FAIL) == 0)
    {
        status_code = HXT_BIND_FAILED;
    }
    else if (strcmp(item->valuestring, HXT_UPLOAD_FILE_FAIL) == 0)
    {
        status_code = HXT_UPLOAD_FAIL;
    }

    if(root != NULL)
    {
        cJSON_Delete(root);
    }

    return status_code; 
}

static char* hxt_get_response_description(void *data)
{
    char* desc = NULL;
    if(NULL == data)
    {
        return NULL;
    }

    cJSON* root = cJSON_Parse(data);
    cJSON *item = cJSON_GetObjectItem(root, "desc");
    desc = (char*)utils_malloc(strlen(item->valuestring) + 1);
    strcpy(desc, item->valuestring);
    // desc[strlen(item->valuestring)] = '\0';

    if(root != NULL)
    {
        cJSON_Delete(root);
    }

    return desc; 
}

static BOOL hxt_get_response_pass_status(void *data)
{
    BOOL status;
    if(NULL == data)
    {
        return FALSE;
    }

    cJSON* root = cJSON_Parse(data);
    cJSON *item = cJSON_GetObjectItem(root, "isPass");
    if(strcmp(item->valuestring, "true") == 0)
    {
        status = TRUE;
    }
    else
    {
        status = FALSE;
    }
    
    if(root != NULL)
    {
        cJSON_Delete(root);
    }

    return status; 
}

static void hxt_get_token_response(void* data)
{
    if(NULL == data)
    {
        utils_print("no response data in\n");
        return;
    }

    cJSON* root = cJSON_Parse(data);
    
    //check return status,if not OK, get error msg
    cJSON *item1 = cJSON_GetObjectItem(root, "status");
    if (!item1)
    {
        return;            
    }
    int status = item1->valueint;
    if(status != HXT_OK)
    {
        cJSON *item2 = cJSON_GetObjectItem(root, "desc");
        utils_print("getToken request failed, err_code:%d, err_msg:%s\n", status, item2->valuestring);
        return;
    }

    if(root != NULL)
    {
        cJSON_Delete(root);
    }

    return;
}

static char* hxt_get_header_with_token()
{
    char* header = NULL;
    
    char* token = get_server_token();
    if(NULL == token)
    {
        return NULL;
    }
    header = (char*)utils_malloc(strlen(token) + strlen("Authorization:") + strlen("Bearer ") + 1);
    strcpy(header, "Authorization:");
    strcat(header, "Bearer ");
    strcat(header, token);
    
    utils_free(token);

    return header;
}

static void hxt_create_private_doc(int child_unid)
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
    // system("mkdir -p /user/sample");  

    return;
}

static void hxt_remove_private_doc(int child_unid)
{
    /* posture video */
    char rm_dir_cmd[256] = {0};
    sprintf(rm_dir_cmd, "rm -r /user/child_%d/video", child_unid);
    system(rm_dir_cmd);
    /* posture snap */
    memset(rm_dir_cmd, 0, 256);
    sprintf(rm_dir_cmd, "rm -r /user/child_%d/snap", child_unid);
    system(rm_dir_cmd);

    /* self define voice */
    memset(rm_dir_cmd, 0, 256);
    sprintf(rm_dir_cmd, "rm -r /user/child_%d/alarm", child_unid);
    system(rm_dir_cmd);
}

static void hxt_init_cfg(void* data)
{
    cJSON* item = NULL; 
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


    cJSON* item1 = cJSON_GetObjectItem(returnObject, "websocketUrl");
    cJSON* item2 = cJSON_GetObjectItem(returnObject, "uploadHostUrl");
    set_server_params(item1->valuestring, item2->valuestring);

    char *tmp1 = NULL, *tmp2 = NULL;
    item1 = cJSON_GetObjectItem(returnObject, "iflyosToken");
    item2 = cJSON_GetObjectItem(returnObject, "iflyosID");
    if(item1)
    {
        tmp1 = item1->valuestring;
    }
    if(item2)
    {
        tmp2 = item2->valuestring;
    }
    set_iflyos_params(tmp1, tmp2);
    
    item = cJSON_GetObjectItem(returnObject, "postureCountDuration");
    int judge_time = item->valueint;
    item = cJSON_GetObjectItem(returnObject, "videoRecordDuration");
    int video_length = item->valueint;
    item = cJSON_GetObjectItem(returnObject, "videoRecordRatio");
    int video_ratio = item->valueint;
    item = cJSON_GetObjectItem(returnObject, "videoRecordCount");
    int video_count = item->valueint;
    item = cJSON_GetObjectItem(returnObject, "photoRecordCount");
    int snap_count = item->valueint;
    item = cJSON_GetObjectItem(returnObject, "offlineStorage");
    int offline_count = item->valueint;
    item = cJSON_GetObjectItem(returnObject, "attachRatio");
    int attach_ratio = item->valueint;
    set_running_params(judge_time, video_length, video_ratio, video_count, snap_count, offline_count, attach_ratio);

    item = cJSON_GetObjectItem(returnObject, "newVersionId");
    item1 = cJSON_GetObjectItem(returnObject, "newVersionNo");
    item2 = cJSON_GetObjectItem(returnObject, "upgradePackUrl");
    set_update_params(item->valueint, item1->valuestring, item2->valuestring);

    hxt_parse_user_data((void*)returnObject);

    if (root != NULL)
    {
        cJSON_Delete(root);
        root = NULL;
    }

    return;
}

BOOL hxt_query_wifi_info(void *data)
{
    if (NULL == data)
    {
        return FALSE;
    }

    cJSON *root = cJSON_Parse(data);
    if(NULL == root)
    {
        return FALSE;
    }
    
    BOOL old_code = FALSE;
    char *ssid = NULL, *pwd = NULL, *code = NULL;

    cJSON *ssid_node = cJSON_GetObjectItem(root, "ssid");
    if (ssid_node != NULL)
    {
        ssid = ssid_node->valuestring;
    }
    cJSON *pwd_node = cJSON_GetObjectItem(root, "pwd");
    if (pwd_node != NULL)
    {
        pwd = pwd_node->valuestring;
    }
    cJSON *checkCode_node = cJSON_GetObjectItem(root, "checkCode");
    if (checkCode_node != NULL)
    {
        code = checkCode_node->valuestring;
    }

    if (code == NULL)
    {
        code = get_wifi_check_code();
        old_code = TRUE;
    }

    BOOL result = set_wifi_params(ssid, pwd, code);

    if (old_code && code != NORMAL)
    {
        utils_free(code);
    }

    cJSON_Delete(root);

    return result;
}

BOOL hxt_bind_desk_with_wifi_request()
{
    BOOL uuid_exist = FALSE;
    BOOL reported = FALSE;
    char* api_url = hxt_get_api_url(HXT_BIND_DESK_WIFI);
    if(NULL == api_url)
    {
        goto CLEANUP4;
    }

    //post json data to server
    cJSON *root = cJSON_CreateObject();    
    if (NULL == root)
    {
        goto CLEANUP3;
    }
    char* desk_uunid_tmp = get_device_id(); 
    char* check_code = get_wifi_check_code();
    cJSON_AddStringToObject(root, "checkCode", check_code);
    cJSON_AddStringToObject(root, "snCode", board_get_sn());
    if(desk_uunid_tmp != NULL && strlen(desk_uunid_tmp) != 0)
    {
        cJSON_AddStringToObject(root, "deskCode", desk_uunid_tmp);
        uuid_exist = TRUE;
    }
    else
    {
        cJSON_AddStringToObject(root, "deskCode", "");
        uuid_exist = FALSE;
    }
    
    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        goto CLEANUP2;
    }
    utils_print("%s\n", json_data);
    //save response data
    char* out = (char*)utils_malloc(2048);
    if(!utils_post_json_data(api_url, "", json_data, out, 2048))
    {
        utils_print("post data send failed\n");
        goto CLEANUP1;
    } 
    utils_print("response length is [%s]\n", out);
    int status_code = hxt_get_reponse_status_code(out);
    if (status_code == HXT_OK)
    {
        /* store desk unid */
        if (!uuid_exist)
        {
            char *uuid = hxt_get_response_description(out);
            utils_print("desk uuid is %s\n", uuid);
            set_device_id(uuid);
            utils_free(uuid);
        }
        
        reported = TRUE;
    } 
    else if (status_code == HXT_BIND_FAILED)
    {
        utils_print("device is already binded\n");
        reported = FALSE;
    }
    else if (status_code == HXT_NO_REGISTER)
    {
        /* deinit */
        utils_system_reset();
        sleep(3);
        utils_system_reboot();
    }

CLEANUP1:  
    utils_free(out);
CLEANUP2:   
    utils_free(check_code);
    utils_free(desk_uunid_tmp);
    utils_free(json_data);
CLEANUP3:
    cJSON_Delete(root);
    utils_free(api_url);
CLEANUP4:
    return reported;
}

BOOL hxt_confirm_desk_bind_request()
{
    BOOL reported = FALSE;
    char* api_url = hxt_get_api_url(HXT_CONFIRM_DESK_BIND);
    if(NULL == api_url)
    {
        goto CLEANUP4;
    }

    //post json data to server
    cJSON *root = cJSON_CreateObject();    
    if (NULL == root)
    {
        goto CLEANUP3;
    }
    char* desk_code = get_device_id();
    char* check_code = get_wifi_check_code();
    cJSON_AddStringToObject(root, "deskCode", desk_code);
    cJSON_AddStringToObject(root, "snCode", board_get_sn());
    cJSON_AddStringToObject(root, "checkCode", check_code);
    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        goto CLEANUP2;
    }
    utils_print("%s\n", json_data);
    //save response data
    char* out = (char*)utils_malloc(2048);
    if(!utils_post_json_data(api_url, "", json_data, out, 2048))
    {
        utils_print("post data send failed\n");
        goto CLEANUP1;
    } 
    utils_print("response length is [%s]\n", out);
    int status_code = hxt_get_reponse_status_code(out);
    if (status_code == HXT_OK)
    {
        reported = TRUE;
    } 

CLEANUP1:  
    utils_free(out);
    utils_free(json_data);
CLEANUP2:   
    cJSON_Delete(root);
    utils_free(desk_code);
    utils_free(check_code);
CLEANUP3:
    utils_free(api_url);
CLEANUP4:
    return reported;
}

BOOL hxt_get_token_request()
{
    BOOL reported = FALSE;
    char* api_url = hxt_get_api_url(HXT_GET_TOKEN);
    if(NULL == api_url)
    {
        goto CLEANUP4;
    }

    char* uuid = get_device_id(); 
    if(NULL == uuid)
    {
        goto CLEANUP4;
    }

    //post json data to server
    cJSON *root = cJSON_CreateObject();    
    if (NULL == root)
    {
        goto CLEANUP3;
    }
    cJSON_AddStringToObject(root, "deskCode", uuid);
    cJSON_AddStringToObject(root, "snCode", board_get_sn());
    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        goto CLEANUP2;
    }

    //save response data
    char *out = (char*)utils_malloc(2048);
    if(!utils_post_json_data(api_url, NULL, json_data, out, 2048))
    {
        utils_print("post data send failed\n");
        goto CLEANUP1;
    } 

    int status_code = hxt_get_reponse_status_code((void*)out);
    if (status_code == HXT_OK)
    {
        /* write token into cfg */   
        cJSON* token_root = cJSON_Parse(out);
        if(NULL == token_root)
        {
            goto CLEANUP1;
        }
        cJSON* returnObject = cJSON_GetObjectItem(token_root, "returnObject");
        if (NULL == returnObject)
        {
            utils_print("No return objects\n");
            cJSON_Delete(token_root);
            goto CLEANUP1;
        }
        cJSON* token_item = cJSON_GetObjectItem(returnObject, "token");  
        cJSON* time_item = cJSON_GetObjectItem(returnObject, "tokenExpireTime");
        utils_print("token time is %lld\n", time_item->valuedouble);
        set_connect_params(token_item->valuestring, time_item->valuedouble/1000);

        cJSON_Delete(token_root);
        reported = TRUE;
    }
    else if(status_code == HXT_NO_REGISTER)
    {
        utils_print("No register device...\n");
        utils_system_reset();
        sleep(3);
        utils_system_reboot();
    } 

CLEANUP1:    
    utils_free(out);
    utils_free(json_data);
CLEANUP2:    
    cJSON_Delete(root);
    utils_free(uuid);
CLEANUP3:
    utils_free(api_url);
CLEANUP4:
    return reported;
}

BOOL hxt_refresh_token_request()
{
    BOOL token_required = TRUE;

    long expire_time = get_server_token_expired_time();
    time_t now = time(0);
    
    long valid_time = now - expire_time;
    utils_print("now %ld - expire_time %ld = %ld\n", now, expire_time, valid_time);
    
    char* server_token = get_server_token();
    if ( (NULL == server_token) || (valid_time >= 0))
    {
        utils_print("require token....\n");
        token_required = hxt_get_token_request();
    }

    utils_free(server_token);
    return token_required;
}

BOOL hxt_get_desk_cfg_request()
{
    int retry_count = 0;
    BOOL reported = FALSE;
    char* api_url = hxt_get_api_url(HXT_GET_DESK_CONFIG);
    if(NULL == api_url)
    {
        return FALSE;
    }

    char* header = hxt_get_header_with_token();
    if(NULL == header)
    {
        utils_free(api_url);
        return FALSE;
    }

    //save response data
    char *out = (char*)utils_malloc(1024*2);
 RETRY_GET:   
    if(!utils_post_json_data(api_url, header, NULL, out, 1024*2))
    {
        utils_print("post data send failed\n");
        utils_free(out);
        utils_free(header);
        utils_free(api_url);
        return FALSE;
    } 
    // utils_print("response is [%s]\n", out);

    int status_code = hxt_get_reponse_status_code((void *)out);
    if (status_code == HXT_OK)
    {
        /* init hxt config */
        hxt_init_cfg((void*)out);
        reported = TRUE;
    }
    else
    {
        hxt_refresh_token_request();
        if (retry_count < 3)
        {
            retry_count ++;
            bzero((void*)out, 1024*2);
            goto RETRY_GET;
        }
    }
    

    utils_free(out);
    utils_free(header);
    utils_free(api_url);
    
    return reported;
}

BOOL hxt_get_new_version_request(const char* update_url)
{
    if (NULL == update_url)
    {
        return FALSE;
    }

    /*download file*/
    utils_download_file(update_url, UPDATE_FILES);

    return TRUE;
}

int hxt_file_upload_request(const char* filename, const char* study_date, char* server_file_path)
{
    BOOL uploaded = FALSE;
    int retry_count = 0;
    char* upload_url = NULL;
    char* header = NULL;
    char* out = NULL;
    int status_code = 0;

    if(NULL == filename || 0 == strlen(filename) || NULL == server_file_path || NULL == study_date)
    {
        return FALSE;
    }
    
    /* create json data*/
    int child_unid = get_select_child_id();

    char url[256] = {0};
    sprintf(url, HXT_UPLOAD_FILE, child_unid, study_date);
    
    while((!uploaded) && (retry_count < 3))
    {
        upload_url = hxt_get_upload_url(url);
        if(NULL == upload_url)
        {
            return FALSE;
        }
        header = hxt_get_header_with_token();

        if (out != NULL)
        {
            utils_free(out);
            out = NULL;
        }
        out = (char*)utils_malloc(2048);
        utils_upload_file(upload_url, header, filename, out, 2048);
        if (strlen(out) == 0)
        {
            retry_count ++;
            utils_print("retry count is %d\n", retry_count);
            continue;
        }
        status_code = hxt_get_reponse_status_code((void*)out);
        if (status_code == HXT_OK)
        {
            char* file_path = hxt_get_response_description((void*)out);
            if( file_path != NULL)
            {
                strcpy(server_file_path, file_path);
                uploaded = TRUE;
                utils_free(file_path);
            }
        }
        else if (status_code == HXT_UPLOAD_FAIL)
        {
            break;
        }
        else if (status_code == HXT_AUTH_FAILED)
        {
            uploaded = FALSE;
            retry_count ++;
            hxt_get_token_request();
        }
    }

    if (out != NULL)
    {
        utils_free(out);
    }
    
    utils_free(header);
    utils_free(upload_url);
    
    return status_code;
}

BOOL hxt_sample_snap_upload_request(const char* filename, const char* study_date, char* server_file_path)
{
    BOOL uploaded = FALSE;
    int retry_count = 0;
    char* upload_url = NULL;
    char* header = NULL;
    char* out = NULL;

    if(NULL == filename || NULL == server_file_path || NULL == study_date)
    {
        return FALSE;
    }
    
    /* create json data*/
    // int child_unid = hxt_get_child_unid();
    int child_unid = get_select_child_id();

    char url[256] = {0};
    sprintf(url, HXT_UPLOAD_SAMPLE_SNAP, child_unid, study_date);

    utils_print("To upload %s ...\n", filename);
    while((!uploaded) && (retry_count < 3))
    {
        upload_url = hxt_get_upload_url(url);
        if(NULL == upload_url)
        {
            return FALSE;
        }

        header = hxt_get_header_with_token();
        utils_print("[%s]\n", header);

        out = (char*)utils_malloc(2048);
        utils_upload_file(upload_url, header, filename, out, 2048);
        utils_print("[%s]\n", out);

        int status_code = hxt_get_reponse_status_code((void*)out);
        if (status_code == HXT_OK)
        {
            uploaded = TRUE;
            server_file_path = hxt_get_response_description((void*)out);
        }
        else if (status_code == HXT_AUTH_FAILED)
        {
            uploaded = FALSE;
            retry_count ++;
            hxt_get_token_request();
        }
    }

    utils_free(out);
    utils_free(header);
    utils_free(upload_url);

    return uploaded;
}

BOOL hxt_update_children_alarm_files(void* data)
{
    int child_unid, study_mode, alarm_type; 
    if (NULL == data)
    {
        return FALSE;
    }
 
    cJSON *node = (cJSON*)data;
    cJSON *node_item = cJSON_GetObjectItem(node, "childrenUnid");
    if (node_item == NULL)
    {   
        utils_print("NO child !!!!\n");
        return;
    }
    child_unid = node_item->valueint;
    
    node_item = cJSON_GetObjectItem(node, "studyMode");
    if (node_item == NULL)
    {
        study_mode = get_study_mode(child_unid);
        if (study_mode == -1)
        {
            study_mode = 3;
        }
    }
    else
    {
        study_mode = node_item->valueint;
    }

    node_item = cJSON_GetObjectItem(node, "alarmType");
    if (node_item == NULL)
    {
        alarm_type = get_alarm_type(child_unid);
        if (alarm_type == -1)
        {
            alarm_type = 2;
        }
    }
    else
    {
        alarm_type = node_item->valueint;
    }

    if (get_select_child_id() == -1)
    {
        set_user_params(child_unid, study_mode, alarm_type, TRUE);
    }

    if (child_unid != get_select_child_id())
    {
        set_user_params(child_unid, study_mode, alarm_type, FALSE);
    }
    
    /* create documents for save mp4 and snap file */
    hxt_create_private_doc(child_unid);

    /* self defines alarm voice */
    char alarm_file[128] = {0};
    char old_alarm_file[128] = {0};
    char replace_cmd[512] = {0};
    char del_cmd[256] = {0};

    cJSON *alarm_file_node = cJSON_GetObjectItem(node, "filePaths");
    if (alarm_file_node != NULL)
    {
        int alarm_file_count = cJSON_GetArraySize(alarm_file_node);
        for (int count = 0; count < alarm_file_count; count++)
        {
            cJSON *file_item = cJSON_GetArrayItem(alarm_file_node, count);
            if (!file_item)
            {
                continue;
            }
            node_item = cJSON_GetObjectItem(file_item, "operation");
            int operate = node_item->valueint; //1:replace 2:delete
            node_item = cJSON_GetObjectItem(file_item, "priority");
            int idx = node_item->valueint;

            if (operate == 1)   //replace
            {
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
            else if (operate == 2) //delete 
            {
                bzero(old_alarm_file, 128);
                sprintf(old_alarm_file, HXT_CHILD_ALARM_FILE, child_unid, idx);
                sprintf(del_cmd, "rm %s", old_alarm_file);
                system(del_cmd);
            }
        }
    }

    return TRUE;
}

BOOL hxt_update_children_alarm_file(void* data)
{
    if (NULL == data)
    {
        return FALSE;
    }

    /* self defines alarm voice */
    int child_unid = -1; 
    char alarm_file[128] = {0};
    char old_alarm_file[128] = {0};
    char replace_cmd[512] = {0};
    char del_cmd[256] = {0};
    cJSON *node_item = NULL;

    cJSON *node = (cJSON*)data;
    if (node != NULL)
    {
        int alarm_file_count = 0;
        BOOL is_array = cJSON_IsArray(node);
        if (is_array)
        {
            alarm_file_count = cJSON_GetArraySize(node);
        }
        else
        {
            alarm_file_count = 1;
        }
        
        for (int count = 0; count < alarm_file_count; count++)
        {
            cJSON *data_item = NULL;
            if (is_array)
            {
                data_item = cJSON_GetArrayItem(node, count);
            }
            else
            {
                data_item = node;
            }
            if (!data_item)
            {
                continue;
            }
            node_item = cJSON_GetObjectItem(data_item, "childrenUnid");
            child_unid = node_item->valueint;
            node_item = cJSON_GetObjectItem(data_item, "operation");
            int operate = node_item->valueint; //1:replace 2:delete
            node_item = cJSON_GetObjectItem(data_item, "priority");
            int idx = node_item->valueint;
            
            if (operate == 1)   //replace
            {
                bzero(alarm_file, 128);
                sprintf(alarm_file, HXT_CHILD_ALARM_FILE_TMP, child_unid, idx);
                node_item = cJSON_GetObjectItem(data_item, "filePath");
                utils_download_file(node_item->valuestring, alarm_file);
                
                /*replace old file*/
                bzero(old_alarm_file, 128);
                sprintf(old_alarm_file, HXT_CHILD_ALARM_FILE, child_unid, idx);
                sprintf(replace_cmd, "mv %s %s", alarm_file, old_alarm_file);
                utils_print("%s\n", replace_cmd);
                system(replace_cmd);  
            } 
            else if (operate == 2) //delete 
            {
                bzero(old_alarm_file, 128);
                sprintf(old_alarm_file, HXT_CHILD_ALARM_FILE, child_unid, idx);
                sprintf(del_cmd, "rm %s", old_alarm_file);
                utils_print("%s\n", del_cmd);
                system(del_cmd);
            }
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
    if (item == NULL)
    {
        return FALSE;
    }
    set_parent_id(item->valueint);

    item = cJSON_GetObjectItem(returnObject, "childrenData");
    int item_count = cJSON_GetArraySize(item);
    utils_print("child data count is %d\n", item_count);
    /* if children data is empty,remove it from config*/
    if (item_count == 0)
    {
        deinit_user_params();
        /* remove data ???? */
    }
    else
    {
        for(int i = 0; i < item_count; i ++)
        {
            cJSON *node = cJSON_GetArrayItem(item, i);
            if (!node)
            {
                continue;
            }
            hxt_update_children_alarm_files((void*)node);
        }
    }
    
    return result;
}

BOOL hxt_unbind_child(int child_unid)
{
    if (child_unid == -1)
    {
        return FALSE;
    }
    /*rm private files*/
    hxt_remove_private_doc(child_unid);

    return delete_child(child_unid);

}

char* hxt_get_posture_detect_model_path(int study_mode)
{
    return DETECT_PATTERN;
}

char* hxt_get_posture_class_model_path(int study_mode)
{
    switch (study_mode)
    {
    case LOW:
        return LOW_CLASS_SAMPLE_FILE;
    case MEDIUM:
        return MEDIUM_CLASS_SAMPLE_FILE;
    case STRICT:
    default:
        return HIGH_CLASS_SAMPLE_FILE;
    }
}

int hxt_get_video_width()
{
    int video_width = 0;
    int video_ratio = get_video_ratio();
    switch (video_ratio)
    {
    case 1:
        video_width = 640;
        break;
    case 2:
        video_width = 960;
        break;
    case 3:
        video_width = 1280;
        break;    
    default:
        video_width = 640;
        break;
    }

    return video_width;
}

int hxt_get_video_height()
{
    int video_height =0;
    int video_ratio = get_video_ratio();
    switch (video_ratio)
    {
    case 1:
        video_height = 360;
    break;
    case 2:
        video_height = 540;
    break;
    case 3:
        video_height = 720;
    break;
    default:
        video_height = 360;
    break;
    }

    return video_height;
}

#if 0
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
#endif