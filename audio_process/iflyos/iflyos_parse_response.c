#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cJSON.h>

#include "utils.h"
#include "iflyos_defines.h"

#define IFLYOS_REPONSES             ("iflyos_responses")
#define IFLYOS_HEADER               ("header")
#define IFLYOS_HEADER_NAME          ("name")
#define IFLYOS_PAYLOAD              ("payload")
#define IFLYOS_METADATA             ("metadata")
#define IFLYOS_META_TEXT            ("text")
#define IFLYOS_MP3_SECURE_URL       ("secure_url")
#define IFLYOS_MP3_URL              ("url")


void iflyos_free(void* ptr)
{
    if(ptr == NULL)
    {
        return;
    }
    
    free(ptr);
    ptr = NULL;

    return;
}

static char* iflyos_get_response_value(const char* json_data, const char* item_name, const char* sub_name, const char* last_node)
{
    char* name = NULL;
    if(NULL == json_data || NULL == item_name || NULL == sub_name)
    {
        return NULL;
    }

    cJSON *root = cJSON_Parse(json_data);
    if(!root)
    {
        return NULL;
    }
    cJSON *responses = cJSON_GetObjectItem(root, IFLYOS_REPONSES);
    if (!responses)
    {
        goto release;
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

release:
    if(root)
    {
        cJSON_Delete(root);
    }

    return name;
}

char* iflyos_get_response_name(const char* json_data)
{
    return iflyos_get_response_value(json_data, IFLYOS_HEADER, IFLYOS_HEADER_NAME, NULL);
}

char* iflyos_get_audio_url(const char* json_data)
{
    return iflyos_get_response_value(json_data, IFLYOS_PAYLOAD, IFLYOS_MP3_URL, NULL);
}

char* iflyos_get_audio_secure_url(const char* json_data)
{
    return iflyos_get_response_value(json_data, IFLYOS_PAYLOAD, IFLYOS_MP3_SECURE_URL, NULL);
}

char* iflyos_get_payload_metadata_text(const char* json_data)
{
    return iflyos_get_response_value(json_data, IFLYOS_PAYLOAD, IFLYOS_METADATA, IFLYOS_META_TEXT);
}

void iflyos_play_response_audio(void *data)
{
    // char* url = iflyos_get_audio_url(data);
    char* url = iflyos_get_audio_secure_url(data);
    if(NULL == url)
    {
        return;
    }
    
    utils_send_mp3_voice(url); 

    iflyos_free(url);

    return;
}