#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cJSON.h>

#include "utils.h"
#include "hxt_defines.h"

typedef enum report_type
{
    START = 1,
    END,
    LEAVE,
    BACK,
    BAD
}ReportType;

typedef enum ext_type
{
    JPEG = 1,
    MP4
}ExtType;

static char* hxt_get_api_url(const char* api)
{
    if(NULL == api)
    {
        return NULL;
    }
    char* server_url = hxt_get_server_url_cfg();
    if(NULL == server_url)
    {   
        return NULL;
    }
    char* ver = hxt_get_api_version_cfg();
    if(NULL == ver)
    {
        return NULL;
    }

    /* should be optimization */
    char request_url[256] = {0};
    strcpy(request_url, server_url);
    strcat(request_url, "/api/");
    strcat(request_url, ver);
    strcat(request_url, api);

    char* ret_value = (char*)utils_calloc(strlen(request_url) + 1);
    memset(ret_value, 0, strlen(request_url) + 1);
    if(NULL == ret_value)
    {
        return NULL;
    }
    strcpy(ret_value, request_url);
    ret_value[strlen(ret_value)] = '\0';

    return ret_value;
}

static char* hxt_get_upload_url(const char* api)
{
    if(NULL == api)
    {
        return NULL;
    }
    char* upload_host = hxt_get_upload_host_url_cfg();
    if(NULL == upload_host)
    {
        return NULL;
    }
    utils_print("upload host %s\n", upload_host);

    char* ver = hxt_get_api_version_cfg();
    if(NULL == ver)
    {
        return NULL;
    }
    
    char request_url[512] = {0};
    strcpy(request_url, upload_host);
    strcat(request_url, "/api/");
    strcat(request_url, ver);
    strcat(request_url, api);
    utils_print("%s\n", request_url);

    char* ret_value = (char*)utils_calloc(strlen(request_url) + 1);
    if(NULL == ret_value)
    {
        return NULL;
    }
    strcpy(ret_value, request_url);

    return ret_value;
}

static int hxt_get_reponse_status_code(void* data)
{
    int status_code = 0;
    if(NULL == data)
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
    desc[strlen(item->valuestring)] = '\0';

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
    
    char* token = hxt_get_token_cfg();
    if(NULL == token)
    {
        return NULL;
    }
    header = (char*)utils_malloc(strlen(token) + strlen("Authorization:") + strlen("Bearer ") + 1);
    strcpy(header, "Authorization:");
    strcat(header, "Bearer ");
    strcat(header, token);
    
    return header;
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
    
    cJSON *ssid_node = cJSON_GetObjectItem(root, "ssid");
    if (NULL == ssid_node)
    {
        return FALSE;
    }
    hxt_set_wifi_ssid_cfg(ssid_node->valuestring);

    cJSON *pwd_node = cJSON_GetObjectItem(root, "pwd");
    if(NULL == pwd_node)
    {
        return FALSE;
    }
    hxt_set_wifi_pwd_cfg(pwd_node->valuestring);

    cJSON *checkCode_node = cJSON_GetObjectItem(root, "checkCode");
    if(NULL == checkCode_node)
    {
        return FALSE;
    }
    hxt_set_wifi_check_code_cfg(checkCode_node->valuestring);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
    
    hxt_reload_cfg();
    
    return TRUE;
}

BOOL hxt_get_token_request()
{
    BOOL reported = FALSE;
    char* api_url = hxt_get_api_url(HXT_GET_TOKEN);
    if(NULL == api_url)
    {
        goto CLEANUP4;
    }

    char* uuid = hxt_get_desk_uuid_cfg(); 
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
    char *out = (char*)utils_malloc(1024);
    if(!utils_post_json_data(api_url, NULL, json_data, out, 1024))
    {
        utils_print("post data send failed\n");
        goto CLEANUP1;
    } 

    int status_code = hxt_get_reponse_status_code((void*)out);
    if (status_code == HXT_OK)
    {
        /* write token into cfg */   
        hxt_init_token(out);
        reported = TRUE;
    }
    else if(status_code == HXT_NO_REGISTER)
    {
        //clear wifi info, to prevent user connect to server

    } 

CLEANUP1:    
    utils_free(out);
CLEANUP2:    
    cJSON_Delete(root);
CLEANUP3:
    utils_free(api_url);
CLEANUP4:
    return reported;
}

BOOL hxt_get_desk_cfg_request()
{
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
    if(!utils_post_json_data(api_url, header, NULL, out, 1024*2))
    {
        utils_print("post data send failed\n");
        utils_free(header);
        utils_free(api_url);
        return FALSE;
    } 
    utils_print("response length is [%s]\n", out);

    int status_code = hxt_get_reponse_status_code((void *)out);
    if (status_code == HXT_OK)
    {
        /* init hxt config */
        hxt_init_cfg((void*)out);
        reported = TRUE;
    }

    utils_free(out);
    utils_free(header);
    utils_free(api_url);
    
    return reported;
}

BOOL hxt_report_info_request(int reportType, int cameraStatus)
{
    BOOL reported = FALSE;
    char* api_url = hxt_get_api_url(HXT_STATUS_REPORT);
    if(NULL == api_url)
    {
        goto CLEANUP5;
    }

    char* header = hxt_get_header_with_token();

    //post json data to server
    cJSON *root = cJSON_CreateObject();    
    if (NULL == root)
    {
        goto CLEANUP4;
    }
    cJSON_AddNumberToObject(root, "reportType", reportType);
    cJSON_AddNumberToObject(root, "cameraStatus", cameraStatus);
    
    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        goto CLEANUP3;
    }
    utils_print("%s\n", json_data);
    //save response data
    char *out = (char*)utils_malloc(1024);
    if(!utils_post_json_data(api_url, header, json_data, out, 1024))
    {
        utils_print("post data send failed\n");
        goto CLEANUP1;
    } 
    utils_print("response length is [%s]\n", out);
    int status_code = hxt_get_reponse_status_code((void *)out);
    if (status_code == HXT_OK)
    {
        reported = TRUE;
    }
    else if(status_code == HXT_AUTH_FAILED)
    {
        hxt_get_token_request();
    }    

CLEANUP1:  
    utils_free(out);
    //utils_free(json_data);
CLEANUP3:   
    cJSON_Delete(root);
CLEANUP4:
    utils_free(header);
CLEANUP5: 
    utils_free(api_url);

    return reported;
}

BOOL hxt_file_upload_request(const char* filename, char* server_file_path)
{
    BOOL uploaded = FALSE;
    int retry_count = 0;
    char* upload_url = NULL;
    char* header = NULL;
    char* out = NULL;

    if(NULL == filename || NULL == server_file_path)
    {
        return FALSE;
    }
    
    int child_unid = hxt_get_child_unid();
    char url[256] = {0};
    sprintf(url, HXT_UPLOAD_FILE, child_unid);

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

        if (out != NULL)
        {
            utils_free(out);
        }
        out = (char*)utils_malloc(1024);
        utils_upload_file(upload_url, header, filename, out, 1024);
        utils_print("[%s]\n", out);
        if (strlen(out) == 0)
        {
            retry_count ++;
            continue;
        }

        int status_code = hxt_get_reponse_status_code((void*)out);
        if (status_code == HXT_OK)
        {
            char* file_path = hxt_get_response_description((void*)out);
            if( file_path != NULL)
            {
                strcpy(server_file_path, file_path);
                uploaded = TRUE;
                free(file_path);
            }
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

BOOL hxt_sample_snap_upload_request(const char* filename, char* server_file_path)
{
    BOOL uploaded = FALSE;
    int retry_count = 0;
    char* upload_url = NULL;
    char* header = NULL;
    char* out = NULL;

    if(NULL == filename || NULL == server_file_path)
    {
        return FALSE;
    }
    
    int child_unid = hxt_get_child_unid();
    char url[256] = {0};
    sprintf(url, HXT_UPLOAD_SAMPLE_SNAP, child_unid);

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

        out = (char*)utils_malloc(1024);
        utils_upload_file(upload_url, header, filename, out, 1024);
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

BOOL hxt_study_report_request(int unid, ReportType type, int duration, const char* video_path, const char* photo_path)
{
    BOOL reported = FALSE;
    char* api_url = hxt_get_api_url(HXT_STUDY_REPORT);
    if(NULL == api_url)
    {
        goto CLEANUP5;
    }

    char* header = hxt_get_header_with_token();

    //post json data to server
    cJSON *root = cJSON_CreateObject();    
    if (NULL == root)
    {
        goto CLEANUP4;
    }
    //post data in json
    cJSON_AddNumberToObject(root, "childrenUnid", unid);
    cJSON_AddNumberToObject(root, "parentUnid", hxt_get_parent_unid_cfg());
    cJSON_AddNumberToObject(root, "reportType", type);
    cJSON_AddStringToObject(root, "studyDate", utils_date_to_string);
    cJSON_AddStringToObject(root, "reportTime", utils_time_to_string);
    cJSON_AddNumberToObject(root, "studyMode", hxt_get_study_mode_cfg(unid));
    if(type == BAD)
    {
        cJSON_AddNumberToObject(root, "duration", duration);
        cJSON_AddStringToObject(root, "videoUrl", video_path);
        cJSON_AddStringToObject(root, "videoUrl", photo_path);
    }
    
    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        goto CLEANUP3;
    }
    utils_print("%s\n", json_data);
    //save response data
    char *out = (char*)utils_malloc(1024);
    if(!utils_post_json_data(api_url, header, json_data, out, 1024))
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
    else if(status_code == HXT_AUTH_FAILED)
    {
        hxt_get_token_request();
    }    

CLEANUP1:  
    utils_free(out);
    // utils_free(json_data);
CLEANUP3:   
    cJSON_Delete(root);
CLEANUP4:
    utils_free(header);
CLEANUP5: 
    utils_free(api_url);
    return reported;
}

BOOL hxt_study_batch_report_request()
{
    BOOL reported = FALSE;

    return reported;
}

int hxt_get_max_chunk_request(const char* file_md5, const ExtType type)
{
    int max_chunk = 0;
    char file_type[8] = {0};
    char uri[256] = {0};
    
    if(NULL == file_md5 || NULL == file_type)
    {
        return max_chunk;
    }

    switch (type)
    {
    case JPEG:
        strcpy(file_type, "jpeg");
    break;
    case MP4:
        strcpy(file_type, "mp4");
    break;    
    default:
        strcpy(file_type, "mp4");
    break;
    }
    sprintf(uri, HXT_GETMAX_CHUNK, file_md5, file_type);
    char* api_url = hxt_get_upload_url(uri);
    if(NULL == api_url)
    {
        return max_chunk;
    }
    char* header = hxt_get_header_with_token();

    //save response data
    char *out = (char*)utils_malloc(1024);
    if(!utils_post_json_data(api_url, header, NULL, out, 1024))
    {
        utils_print("post data send failed\n");
        goto EXIT;
    } 
    utils_print("response length is [%s]\n", out);
    int status_code = hxt_get_reponse_status_code(out);
    BOOL passed = hxt_get_response_pass_status(out);
    if (status_code == HXT_OK && passed)
    {
        char* desc = hxt_get_response_description(out);
        if(NULL == desc)
        {
            max_chunk = 0;
        }
        max_chunk = atoi(desc);

        utils_free(desc);
    }
    else if(status_code == HXT_AUTH_FAILED)
    {
        hxt_get_token_request();
    }    

EXIT:  
    utils_free(out);
    utils_free(api_url);
    utils_free(header);

    return max_chunk;
}

char* hxt_send_chunk_request(const char* file_md5, int chunk_idx, int max_chunk)
{
    char* server_file_path = NULL;

    if(NULL == file_md5)
    {
        return FALSE;
    }

    char uri[256] = {0};
    sprintf(uri, HXT_UPLOAD_CHUNK, file_md5, chunk_idx, max_chunk);    
    char* upload_url = hxt_get_upload_url(uri);
    if(NULL == upload_url)
    {
        return FALSE;
    }
    utils_print("auth:%s\n", upload_url);   

    char* header = hxt_get_header_with_token();
    utils_print("auth:%s\n", header);

    char *out = (char*)utils_malloc(1024);

    //TODO get file name 
    char chunk_file[256] = {0};
    //???
    utils_upload_file(chunk_file, header, chunk_file, out, 1024);
    utils_print("[%s]\n", out);

    int status_code = hxt_get_reponse_status_code((void*)out);
    if (status_code == HXT_OK)
    {
        server_file_path = hxt_get_response_description((void*)out);
    }

    utils_free(out);
    utils_free(header);
    utils_free(upload_url);

    return server_file_path;
}

BOOL hxt_merge_chunks_request(const char* file_path, ExtType type)
{
    BOOL reported =FALSE;
    char uri[256] = {0};
    char file_type[8] = {0};
    char type_name[16] = {0};   

    if(NULL == file_path)
    {
        return FALSE;
    }
    char* md5 = utils_get_file_md5sum(file_path);
    unsigned long size = utils_get_file_size(file_path);

    switch (type)
    {
    case JPEG:
        strcpy(file_type, "jpeg");
        strcpy(type_name, "photo");
    break;
    case MP4:
    default:
        strcpy(file_type, "mp4");
        strcpy(type_name, "video");
    break;    
    }

    sprintf(uri, HXT_MERGE_FILES, md5, file_type, size, type_name);
    char* api_url = hxt_get_upload_url(uri);
    if(NULL == api_url)
    {
        return FALSE;
    }
    char* header = hxt_get_header_with_token();

    //save response data
    char *out = (char*)utils_malloc(1024);
    if(!utils_post_json_data(api_url, header, "", out, 1024))
    {
        utils_print("post data send failed\n");
        goto EXIT;
    } 
    utils_print("response length is [%s]\n", out);
    int status_code = hxt_get_reponse_status_code(out);
    hxt_get_response_pass_status(out);
    if (status_code == HXT_OK)
    {
        //
        reported = TRUE;
    }
    else if(status_code == HXT_AUTH_FAILED)
    {
        hxt_get_token_request();
    }    

EXIT:  
    utils_free(out);
    utils_free(api_url);
    utils_free(header);

    return reported;
}

BOOL hxt_check_wifi_data_request()
{
    BOOL reported = FALSE;
    char* api_url = hxt_get_api_url(HXT_CHECK_WIFI_DATA);
    if(NULL == api_url)
    {
        goto CLEANUP5;
    }

    //post json data to server
    cJSON *root = cJSON_CreateObject();    
    if (NULL == root)
    {
        goto CLEANUP4;
    }
    cJSON_AddNumberToObject(root, "ssid", hxt_get_wifi_ssid_cfg());
    cJSON_AddNumberToObject(root, "bssid", hxt_get_wifi_bssid_cfg());
    cJSON_AddNumberToObject(root, "pwd", hxt_get_wifi_pwd_cfg());
    cJSON_AddNumberToObject(root, "checkCode", hxt_get_wifi_check_code_cfg());
    
    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        goto CLEANUP3;
    }
    utils_print("%s\n", json_data);
    //save response data
    char* out = (char*)utils_malloc(1024);
    if(!utils_post_json_data(api_url, "", json_data, out, 1024))
    {
        utils_print("post data send failed\n");
        goto CLEANUP1;
    } 
    utils_print("response length is [%s]\n", out);
    int status_code = hxt_get_reponse_status_code(out);
    if (status_code == HXT_OK)
    {
        reported = TRUE;
        
        // char* desc = hxt_get_response_description(out);
        // hxt_set_desk_sn_code(desc);
    } 

CLEANUP1:  
    utils_free(out);
    // utils_free(json_data);
CLEANUP3:   
    cJSON_Delete(root);
CLEANUP4:
    utils_free(api_url);
CLEANUP5:
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
    cJSON_AddStringToObject(root, "deskCode", hxt_get_desk_uuid_cfg());
    cJSON_AddStringToObject(root, "snCode", board_get_sn());
    
    char* json_data = cJSON_PrintUnformatted(root);
    if (NULL == json_data)
    {
        goto CLEANUP2;
    }
    utils_print("%s\n", json_data);
    //save response data
    char* out = (char*)utils_malloc(1024);
    if(!utils_post_json_data(api_url, "", json_data, out, 1024))
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
CLEANUP2:   
    cJSON_Delete(root);
CLEANUP3:
    utils_free(api_url);
CLEANUP4:
    return reported;
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
    cJSON_AddStringToObject(root, "checkCode", hxt_get_wifi_check_code_cfg());
    cJSON_AddStringToObject(root, "snCode", board_get_sn());
    char* desk_uunid_tmp = hxt_get_init_desk_uuid();
    if(desk_uunid_tmp != NULL)
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
    char* out = (char*)utils_malloc(1024);
    if(!utils_post_json_data(api_url, "", json_data, out, 1024))
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
            hxt_set_desk_uuid_cfg(uuid);
            /* write into init config,and never erase it*/
            hxt_store_desk_uuid(uuid);
            free(uuid);
        }
        
        reported = TRUE;
    } 

CLEANUP1:  
    utils_free(out);
CLEANUP2:   
    cJSON_Delete(root);
CLEANUP3:
    utils_free(api_url);
CLEANUP4:
    return reported;
}

BOOL hxt_refresh_token_request()
{
    BOOL token_required = TRUE;

    long expire_time = hxt_get_token_expire_time_cfg();
    time_t now = time(0);
    
    long valid_time = now - expire_time;
    utils_print("now %ld - expire_time %ld = %ld\n", now, expire_time, valid_time);
    
    if ( (hxt_get_token_cfg() == NULL) || (valid_time >=0) )
    {
        utils_print("require token....\n");
        token_required = hxt_get_token_request();
    }

    return token_required;
}

BOOL hxt_get_new_version_request(int new_ver_id, const char* new_ver_no, const char* update_url)
{
    if (NULL == update_url || new_ver_id <= 0)
    {
        return FALSE;
    }

    /*download file*/
    utils_download_file(update_url, UPDATE_BIN_FILE);

    return TRUE;
}
