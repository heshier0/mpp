#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <cJSON.h>

#include "utils.h"

#define MP3_FIFO ("/tmp/my_mp3_fifo")

static char* separate_filename(const char *url)
{
    char * filename = NULL;
    if (NULL == url)
    {
        return NULL;
    }

    char *start = NULL;
    char *end = NULL;
    int with_token= 0;

    if ((start = strrchr(url, '\/')) == NULL)
    {
        return NULL;
    }
    start ++;
    
    if ((end = strrchr(url, '?')) == NULL)
    {
        int length = (int)(url+strlen(url) - start);
        filename = (char*)utils_calloc(length+1);
        memcpy(filename, start, (url+strlen(url) - start));
    }
    else 
    {
        int length = (int)(end -start);
        filename = (char*)utils_calloc(length+1);
        memcpy(filename, start, (end - start));
    }
    int count = strlen(filename);
    filename[strlen(filename)] = '\0';

    return filename;
}

static BOOL check_system_cmd_result(const pid_t status)
{
    pid_t res = (pid_t) status;
    if(-1 == res)
    {
        utils_print("system cmd execute failed.\n");
        return FALSE;
    }
    else
    {
        if(WIFEXITED(status))
        {
            if(0 == WEXITSTATUS(status))      
            {
                return TRUE;
            }  
        }
    }

    return FALSE;
}

cJSON* utils_load_cfg(const char* cfg)
{
    FILE *file = NULL;
    cJSON *root = NULL;
    long length = 0;
    char* content = NULL;
    size_t read_chars = 0;

    if(NULL==cfg)
    {
        return NULL;
    }

    //以2进制方式打开
    file = fopen(cfg, "rb");
    if (NULL == file)
    {
        goto CLEANUP;
    }
    //获取长度
    if (fseek(file, 0, SEEK_END) != 0)
    {
        goto CLEANUP;
    }
    length = ftell(file);
    if (length < 0)
    {
        goto CLEANUP;
    }
    if (fseek(file, 0, SEEK_SET) != 0)
    {
        goto CLEANUP;
    }
    //分配内存
    //content = (char*)malloc((size_t)length + sizeof(""));
    content = (char*)utils_malloc(length+1);
    if (NULL == content)
    {
        goto CLEANUP;
    }
    //读取文件
    read_chars = fread(content, sizeof(char), (size_t)length, file);
    if ((long)read_chars != length)
    {
        utils_free(content);
        content = NULL;
        goto CLEANUP;
    }
    content[read_chars] = '\0';
    root = cJSON_Parse(content);
    if(content != NULL)
    {
        utils_free(content);
        content = NULL;
    }

CLEANUP:
    if (file != NULL)
    {
        fclose(file);
    }    

    return root;
}

void utils_unload_cfg(cJSON* root)
{
    if (root != NULL)
    {
        cJSON_Delete(root);
    }

    return;
}

BOOL utils_reload_cfg(const char* cfg, cJSON* root)
{
    if (NULL==root || NULL==cfg)
    {
        return FALSE;
    }

    char* content = cJSON_Print(root);
    if(NULL == content)
    {
        return FALSE;
    }

    FILE *fp = fopen(cfg, "w+");
    if (NULL == fp )
    {
        return FALSE;
    }
    
    fwrite(content, strlen(content), 1, fp);
    fclose(fp);
  
    utils_free(content);
    content = NULL;

    return TRUE;
}

char* utils_get_cfg_str_value(cJSON* root, const char* params_item, const char* prop_item)
{
    char* prop_item_val = NULL;
    if (NULL == root || 
        NULL == params_item || 
        NULL == prop_item)
    {
        return NULL;
    }
   
    cJSON *params_node = cJSON_GetObjectItem(root, params_item);
    if (!params_node)
    {
        return NULL;
    }
    cJSON *prop_node = cJSON_GetObjectItem(params_node, prop_item);
    if (!prop_node)
    {
        return NULL;
    }

    return prop_node->valuestring;
}

double utils_get_cfg_number_value(cJSON* root, const char* params_item, const char* prop_item)
{
    if (NULL == params_item || NULL == prop_item)
    {
        return 0;
    }
   
    cJSON *params_node = cJSON_GetObjectItem(root, params_item);
    if (!params_node)
    {
        return 0;
    }
    cJSON *prop_node = cJSON_GetObjectItem(params_node, prop_item);
    if (!prop_node)
    {
        return 0;
    }
    
    return prop_node->valueint;
}

BOOL utils_set_cfg_str_value(cJSON* root, const char* cfg, const char* params_item, const char* prop_item, const char* value)
{
    char* prop_item_val = NULL;

    if (NULL == cfg || 
        NULL == root || 
        NULL == params_item || 
        NULL == prop_item)
    {
        utils_print("params null\n");
        return FALSE;
    }
   
    cJSON *params_node = cJSON_GetObjectItem(root, params_item);
    if (!params_node)
    {
        int ret = cJSON_AddItemToObject(root, params_item, params_node = cJSON_CreateObject());
        if (ret == 0)
        {
            return FALSE;
        }
    }

    cJSON *prop_node = cJSON_GetObjectItem(params_node, prop_item);
    if(!prop_node)
    {
        if(NULL == value)
        {
            prop_node = cJSON_AddNullToObject(params_node, prop_item);
            return TRUE;
        }
        else
        {
            prop_node = cJSON_AddStringToObject(params_node, prop_item, value);
        }
        
        if(prop_node == NULL)
        {
            return FALSE;
        }
    }
    else
    {
        return (cJSON_SetValuestring(prop_node, value) == NULL);
    }

    return TRUE;
}

BOOL utils_set_cfg_number_value(cJSON* root, const char* cfg, const char* params_item, const char* prop_item, const double value)
{
    if (NULL == cfg ||
        NULL == root ||
        NULL == params_item || 
        NULL == prop_item)
    {
        return FALSE;
    }
   
    cJSON *params_node = cJSON_GetObjectItem(root, params_item);
    if (!params_node)
    {
        int ret = cJSON_AddItemToObject(root, params_item, params_node = cJSON_CreateObject());
        if (ret == 0)
        {
            return FALSE;
        }
    }

    cJSON *prop_node = cJSON_GetObjectItem(params_node, prop_item);
    if (!prop_node)
    {
        prop_node = cJSON_AddNumberToObject(params_node, prop_item, value);
        if(prop_node == NULL)
        {
            return FALSE;
        }
    }
    else
    {
        cJSON_SetNumberValue(prop_node, value);
    }

    return TRUE;
}

BOOL utils_send_mp3_voice(const char *url)
{
    char cmd[256] = {0};
#ifdef DEBUG    
    sprintf(cmd, "curl --insecure -o %s %s", MP3_FIFO, url);
#else
    sprintf(cmd, "curl --insecure -s -o %s %s", MP3_FIFO, url);
#endif   
    pid_t status = system(cmd);
    
    return check_system_cmd_result(status);
}

BOOL utils_download_file(const char *url, char *out_buffer, int buffer_length)
{
    if (NULL == url || NULL == out_buffer)
    {
        return FALSE;
    }

    char * filename = separate_filename(url);
    if(filename == NULL)
    {
        return FALSE;
    }

    char CMD_DOWNLOAD_FILE[512] = {0};
#ifdef DEBUG
    sprintf(CMD_DOWNLOAD_FILE, "curl --insecure -o %s %s", filename, url);
#else
    sprintf(CMD_DOWNLOAD_FILE, "curl --insecure -s -o %s %s", filename, url);
#endif    

    FILE *fp = NULL;
    fp = popen(CMD_DOWNLOAD_FILE, "r");
    if(NULL != fp)
    {
        if(fgets(out_buffer, buffer_length, fp) == NULL)
        {
            pclose(fp);
            return FALSE;
        }
        out_buffer[buffer_length-1] = '\0';
    }
    pclose(fp);

    if(filename != NULL)
    {
        utils_free(filename);
        filename = NULL;
    }

    return TRUE;
}

BOOL utils_upload_file(const char* url, const char* header, const char* local_file_path, char* out_buffer, int buffer_length)
{
    if(NULL == url || NULL == local_file_path || NULL == out_buffer)
    {
        return FALSE;
    }

    utils_print("upload file name is %s\n", local_file_path);

    char CMD_UPLOAD_FILE[512] = {0};
#ifdef DEBUG
    sprintf(CMD_UPLOAD_FILE, "curl --insecure -H \"%s\" -F \"file=@%s\" %s", header, local_file_path, url);
#else
    sprintf(CMD_UPLOAD_FILE, "curl --insecure -s -H \"%s\" -F \"file=@%s\" %s", header, local_file_path, url);
#endif // DEBUG

    FILE *fp = NULL;
    fp = popen(CMD_UPLOAD_FILE, "r");
    if(NULL != fp)
    {
        if(fgets(out_buffer, buffer_length, fp) == NULL)
        {
            pclose(fp);
            return FALSE;
        }
        out_buffer[buffer_length-1] = '\0';
    }
    pclose(fp);

        
    return TRUE;
}

BOOL utils_post_json_data(const char *url, const char* header_content, const char* json_data, char* out, int out_length)
{

    if (NULL == url || NULL == out)
    {
        return FALSE;
    }

    char CMD_POST_JSON[512] = {0};
#ifdef DEBUG
    sprintf(CMD_POST_JSON, "curl --insecure -X POST -H \"Content-Type:application/json;charset=UTF-8\" -H \"%s\" -d \"%s\" %s", 
                                header_content, json_data, url);
#else 
    sprintf(CMD_POST_JSON, "curl --insecure -s -X POST -H \"Content-Type:application/json;charset=UTF-8\" -H \"%s\" -d \"%s\" %s", 
                                header_content, json_data, url);
#endif // DEBUG

    FILE *fp = NULL;
    fp = popen(CMD_POST_JSON, "r");
    if(NULL != fp)
    {
        if(fgets(out, out_length, fp) == NULL)
        {
            pclose(fp);
            return FALSE;
        }
        out[out_length-1] = '\0';
    }
    pclose(fp);

    return TRUE;
}

char* utils_get_response_value(const char* json_data, const char* root_name, const char* item_name, const char* sub_name, const char* last_node)
{
    char* name = NULL;
    if(NULL == json_data || NULL == root_name || NULL == item_name || NULL == sub_name)
    {
        return NULL;
    }

    cJSON *root = cJSON_Parse(json_data);
    if(!root)
    {
        return NULL;
    }
    cJSON *responses = cJSON_GetObjectItem(root, root_name);
    if (!responses)
    {
        goto END;
    }

    int item_count = cJSON_GetArraySize(responses);
    for(int i = 0; i < item_count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(responses, i);
        if (!item)
        {
            continue;
        }
        cJSON *header = cJSON_GetObjectItem(item, item_name);
        cJSON *value= NULL;
        if(NULL == last_node)
        {
            value = cJSON_GetObjectItem(header, sub_name);
        }
        else
        {
            cJSON* sub_value = cJSON_GetObjectItem(header, sub_name);
            value = cJSON_GetObjectItem(sub_value, last_node);
        }
        
        if (value)
        {
            int len = strlen(value->valuestring);
            name = malloc(len + 1);
            memcpy(name, value->valuestring, len);
            name[len] = '\0';
            break;
        }
    }

END:
    if(root)
    {
        cJSON_Delete(root);
    }

    return name;
}

char* utils_date_to_string()
{
    const static char* CMD_GET_DATE = "date +\"%Y-%m-%d\"";
    static char str_date[16] = {0};
    char line[16] = {0};
    FILE *fp = NULL;
    fp = popen(CMD_GET_DATE, "r");
    if(NULL != fp)
    {
        if(fgets(line, sizeof(line), fp) == NULL)
        {
            pclose(fp);
            return NULL;
        }
        line[strlen(line)-1] = '\0';
    }

    pclose(fp);
    strcpy(str_date, line);

    return str_date;
}

char* utils_time_to_string()
{
    const static char* CMD_GET_TIME = "date +\"%Y-%m-%d %H:%M:%S\"";
    static char str_time[24] = {0};
    char line[24] = {0};
    FILE *fp = NULL;
    fp = popen(CMD_GET_TIME, "r");
    if(NULL != fp)
    {
        if(fgets(line, sizeof(line), fp) == NULL)
        {
            pclose(fp);
            return NULL;
        }
        line[strlen(line)-1] = '\0';
    }

    pclose(fp);
    strcpy(str_time, line);

    return str_time;
}

char* utils_get_file_md5sum(const char* file_name)
{
    // md5sum demo_config.sh | awk -F " " '{print $1}'
    if(NULL == file_name)
    {
        return NULL;
    }
    char CMD_GET_MD5[256] = {0};
    sprintf(CMD_GET_MD5, "md5sum %s|awk -F \" \" \'{print $1}\'", file_name);
    static char str_md5[64] = {0};
    char line[64] = {0};
    FILE *fp = NULL;
    fp = popen(CMD_GET_MD5, "r");
    if(NULL != fp)
    {
        if(fgets(line, sizeof(line), fp) == NULL)
        {
            pclose(fp);
            return NULL;
        }
        line[strlen(line)-1] = '\0';
    }

    pclose(fp);
    strcpy(str_md5, line);

    return str_md5;
}

unsigned long utils_get_file_size(const char* path)
{
    unsigned long file_size = 0;
    FILE *fp = NULL;
    
    if(NULL == path)
    {
        return 0;
    }

    fp = fopen(path, "r");
    if(NULL == fp)
    {
        return file_size;
    }
    fseek(fp, 0L, SEEK_END);
    file_size = ftell(fp);
    fclose(fp);

    return file_size;
}

int utils_split_file_to_chunk(const char* path)
{
    int chunk_count = 0;
    if(NULL == path)
    {
        return 0;;
    }
    //separate file name with suffix
    char* full_name = separate_filename(path);
    if(NULL == full_name)
    {
        return 0;
    }
    printf("full name is %s\n", full_name);
    char* p = strchr(full_name, '.');
    char file_name[64] = {0};
    memcpy(file_name, full_name, strlen(p));
    printf("file name is %s\n", file_name);
    char* suffix = strrchr(full_name, '.');
    if(NULL == suffix)
    {
        return 0;
    }
    suffix ++;
    printf("suffix name is %s\n", suffix);

    char CMD_CHUNK_DIR[256] = {0};
    sprintf(CMD_CHUNK_DIR, "mkdir -p /userdata/chunk/%s_%s", file_name, suffix);
    char CMD_SPLIT_FILE[256] = {0};
    sprintf(CMD_SPLIT_FILE, "split -b 5m %s -a 3 /userdata/chunk/%s_%s/%s_%s_", path, file_name, suffix, file_name, suffix);
    char CMD_COUNT_CHUNKS[256] = {0};
    sprintf(CMD_COUNT_CHUNKS, "ls -l /userdata/chunk/%s_%s|wc -l", file_name, suffix);

    system(CMD_CHUNK_DIR);
    system(CMD_SPLIT_FILE);
    
    char line[64] = {0};
    FILE *fp = NULL;
    fp = popen(CMD_COUNT_CHUNKS, "r");
    if(NULL != fp)
    {
        if(fgets(line, sizeof(line), fp) == NULL)
        {
            pclose(fp);
            return 0;
        }
        chunk_count = atoi(line);
    }
    pclose(fp);
    
    //modify chunk file index 
    char CMD_CHUNK_SERIAL[256] = {0};
    sprintf(CMD_CHUNK_SERIAL, "sh /user/bin/modify_chunk_names.sh %s_%s", file_name, suffix);
    system(CMD_CHUNK_SERIAL);

    return chunk_count;
}