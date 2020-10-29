#include <time.h>
#include <libgen.h>

#include <oss_api.h>
#include <aos_http_io.h>
#include <aos_buf.h>

#include "utils.h"
#include "hxt_defines.h"
#include "report_info_db.h"

void init_upload_options(void **opts, void *data)
{
    if (NULL == data)
    {
        return;
    }

    *opts = utils_malloc(sizeof(AliossOptions));
    if (*opts == NULL)
    {
        utils_print("malloc AliossOptions failed\n");
        return;
    }
    AliossOptions *opt = *opts;

    cJSON* returnObject = cJSON_Parse((char *)data);
    if(NULL == returnObject)
    {
        return;
    }

    cJSON *item = cJSON_GetObjectItem(returnObject, "securityToken");
    if (item != NULL)
    {
        opt->sts_token = (char*)utils_malloc(strlen(item->valuestring) + 1);
        strcpy(opt->sts_token, item->valuestring);
        opt->sts_token[strlen(opt->sts_token)] = '\0';
    }

    item = cJSON_GetObjectItem(returnObject, "endpoint");
    if (item != NULL)
    {
        opt->endpoint = (char*)utils_malloc(strlen(item->valuestring) + 1);
        strcpy(opt->endpoint, item->valuestring);
        opt->endpoint[strlen(opt->endpoint)] = '\0';
    }

    item = cJSON_GetObjectItem(returnObject, "accessKeyId");
    if (item != NULL)
    {
        opt->access_key_id = (char*)utils_malloc(strlen(item->valuestring) + 1);
        strcpy(opt->access_key_id, item->valuestring);
        opt->access_key_id[strlen(opt->access_key_id)] = '\0';
    }

    item = cJSON_GetObjectItem(returnObject, "accessKeySecret");
    if (item != NULL)
    {
        opt->access_key_secret = (char*)utils_malloc(strlen(item->valuestring) + 1);
        strcpy(opt->access_key_secret, item->valuestring);
        opt->access_key_secret[strlen(opt->access_key_secret)] = '\0';
    }

    item = cJSON_GetObjectItem(returnObject, "bucketName");
    if (item != NULL)
    {
        opt->bucket_name = (char*)utils_malloc(strlen(item->valuestring) + 1);
        strcpy(opt->bucket_name, item->valuestring);
        opt->bucket_name[strlen(opt->bucket_name)] = '\0';
    }

    item = cJSON_GetObjectItem(returnObject, "path");
    if (item != NULL)
    {
        opt->path = (char*)utils_malloc(strlen(item->valuestring) + 1);
        strcpy(opt->path, item->valuestring);
        opt->path[strlen(opt->path)] = '\0';
    }

    item = cJSON_GetObjectItem(returnObject, "timeout");
    if (item != NULL)
    {
        opt->expired_time = item->valueint;
    }

    return;
}

void deinit_upload_options(void *opts)
{
    if (NULL == opts)
    {
        return;
    }
    AliossOptions *opt = opts;
    if (opt->endpoint)
    {
        utils_free(opt->endpoint);
    }
    if (opt->access_key_id)
    {
        utils_free(opt->access_key_id);
    }
    if (opt->access_key_secret)
    {
        utils_free(opt->access_key_secret);
    }
    if (opt->sts_token)
    {
        utils_free(opt->sts_token);
    }
    if (opt->path)
    {
        utils_free(opt->path);
    }

    utils_free(opt);

    return;
}

static void init_aliyun_sts_options(oss_request_options_t *options, AliossOptions *opts)
{
    options->config = oss_config_create(options->pool);
    aos_str_set(&options->config->endpoint, opts->endpoint);
    aos_str_set(&options->config->access_key_id, opts->access_key_id);
    aos_str_set(&options->config->access_key_secret, opts->access_key_secret);
    aos_str_set(&options->config->sts_token, opts->sts_token);

    options->config->is_cname = 0;
    options->ctl = aos_http_controller_create(options->pool, 0);

    return;
}

BOOL hxt_upload_file(const char* path, void *opts)
{
    if (NULL == path)
    {
        return; 
    }

    // char *object_content = NULL;
    // unsigned long object_size = utils_get_file_size(path);
    // object_content = utils_malloc(object_size);

    // int fd = open(path, O_RDONLY);
    // if (-1 == fd)
    // {
    //     return;
    // }
    // int size = read(fd, object_content, object_size);
    // utils_print("file size is %d\n", size);
    // if (size != object_size)
    // {
    //     utils_print("read file error,size not equal\n");
    //     return;
    // }
    // close(fd);
    BOOL upload_status = FALSE;

    aos_pool_t *pool;
    aos_pool_create(&pool, NULL);
    oss_request_options_t *oss_client_options;
    oss_client_options = oss_request_options_create(pool);
    
    init_aliyun_sts_options(oss_client_options, (AliossOptions*)opts);

    aos_string_t bucket;
    aos_string_t object;
    aos_string_t local_file;
    // aos_list_t buffer;
    // aos_buf_t *content = NULL;
    aos_table_t *headers = NULL;
    aos_table_t *resp_headers = NULL; 
    aos_status_t *resp_status = NULL; 

    time_t now;
	struct tm *timenow;
	time(&now);      
    timenow = gmtime(&now);
    char time_path[16] = {0};
    sprintf(time_path, "%04d/%02d/%02d/", timenow->tm_year+1900, timenow->tm_mon+1, timenow->tm_mday);
    utils_print("%s\n", time_path);
    
    char file[32] = {0};
    strcpy(file, basename(path));
    utils_print("%s\n", file);

    char object_name[256] = {0};
    strcpy(object_name, opts->path);
    strcat(object_name, time_path);
    strcat(object_name, file);
    utils_print("object_name is %s\n", object_name);

    aos_str_set(&object, object_name); //{path}/{年}/{月}/{日}/{时间戳_孩子id.格式}
    aos_str_set(&bucket, opts->bucket_name);
    aos_str_set(&local_file, path);
    
    // aos_list_init(&buffer);
    // content = aos_buf_pack(oss_client_options->pool, object_content, object_size);
    // aos_list_add_tail(&content->node, &buffer);

    resp_status = oss_put_object_from_file(oss_client_options, &bucket, &object, &local_file, headers, &resp_headers);
    // resp_status = oss_put_object_from_buffer(oss_client_options, &bucket, &object, &buffer, headers, &resp_headers);
    if (aos_status_is_ok(resp_status))
    {
        printf("put object from buffer succeeded\n");
        upload_status = TRUE;
    } 
    else 
    {
        printf("put object from buffer failed\n");  
        upload_status = FALSE;    
    }    

    aos_pool_destroy(pool);
    // utils_free(object_content);

    return upload_status;
}

BOOL hxt_init_aliyun_env()
{
    if (aos_http_io_initialize(NULL, 0) != AOSE_OK)
    {
        return FALSE;
    }

    return TRUE;
}

void hxt_deinit_aliyun_env()
{
    aos_http_io_deinitialize();
}



BOOL hxt_del_offline_expired_info()
{
    return del_report_info_expired();
}

int hxt_stored_offline_study_info(void *data)
{
    if (NULL == data)
    {
        return -1;
    }

    /* */
    
}
 