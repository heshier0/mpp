#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

#include <cJSON.h>
#include "db.h"
#include "iflyos_func.h"

static cJSON* iflyos_create_header()
{

    cJSON *root = NULL;
    cJSON *device = NULL;
    cJSON *location = NULL;
    cJSON *platform = NULL;

    /*for test*/
    // char device_id[255] = {0};
    // char token[255] = {0};
    // char line[255] = {0};
    // int line_idx = 0;
    // FILE* fp = fopen("/userdata/data/iflyos_token.txt", "r");
    // if (fp != NULL)
    // {
    //     memset(line, 0, 255);
    //     while(fgets(line, sizeof(line), fp) != NULL)
    //     {
    //         if (line_idx == 0)
    //         {
    //             strcpy(device_id, line);
    //             device_id[strlen(device_id)-1] = '\0';
    //             line_idx ++;
    //         }
    //         else
    //         {
    //             strcpy(token, line);
    //             token[strlen(token)-1] = '\0';
    //         }
            
    //     }
    //     line_idx = 0;
    // }
    // fclose(fp);
    /*test end*/

    /* auth */
    char auth[128] = {0};
    char* token = get_iflyos_token(); 
    strcpy(auth, "Bearer ");
    strcat(auth, token);
    utils_print("token is %s\n", auth);
    /* device id */
    char* device_id = get_device_id();
    utils_print("device id is %s\n", device_id);

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "authorization", cJSON_CreateString(auth));
    cJSON_AddItemToObject(root, "device", device = cJSON_CreateObject());

    cJSON_AddStringToObject(device, "device_id", device_id);
    cJSON_AddStringToObject(device, "ip", "");
    cJSON_AddItemToObject(device, "location", location = cJSON_CreateObject());
    cJSON_AddItemToObject(device, "platform", platform = cJSON_CreateObject());

    cJSON_AddNumberToObject(location, "latitude", 0);
    cJSON_AddNumberToObject(location, "longitude", 0);
    
    cJSON_AddStringToObject(platform, "name", PLATFORM_NAME);
    cJSON_AddStringToObject(platform, "version", PLATFORM_VER);

    char* data = cJSON_PrintUnformatted(root);
    utils_print("iflyos request header: %s\n", data);
    utils_free(data);

    return root;
}

static cJSON* iflyos_create_context()
{
    cJSON* root = NULL;
    cJSON* system = NULL;
    cJSON* audio_player = NULL;
    cJSON* playback = NULL;
    cJSON* speaker = NULL;
    cJSON* wakeword = NULL;

    root = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "system", system = cJSON_CreateObject());
    cJSON_AddItemToObject(root, "audio_player", audio_player = cJSON_CreateObject());
    cJSON_AddItemToObject(root, "speaker", speaker = cJSON_CreateObject());
    // cJSON_AddItemToObject(root, "wakeword", wakeword = cJSON_CreateObject());

    cJSON_AddStringToObject(system, "version", SYSTEM_VER);
    cJSON_AddBoolToObject(system, "software_updater", cJSON_False);
    cJSON_AddBoolToObject(system, "power_controller", cJSON_False);
    cJSON_AddBoolToObject(system, "device_modes", cJSON_False);
    cJSON_AddBoolToObject(system, "factory_reset", cJSON_False);
    cJSON_AddBoolToObject(system, "reboot", cJSON_False);

    cJSON_AddStringToObject(audio_player, "version", AUDIO_PLAYER_VER);
    cJSON_AddItemToObject(audio_player, "playback", playback = cJSON_CreateObject());
    cJSON_AddStringToObject(playback, "state", AUDIO_PLAYER_STATE);
    cJSON_AddStringToObject(playback, "resource_id", "");
    cJSON_AddNumberToObject(playback, "offset", 0);

    cJSON_AddStringToObject(speaker, "version", SPEAKER_VER);
    cJSON_AddNumberToObject(speaker, "volume", SPEAKER_VOL);
    cJSON_AddStringToObject(speaker, "type", SPEAKER_TYPE);

    return root;
}

char*  iflyos_create_audio_in_request()
{
    cJSON *root = NULL;

    // iflyos_create_init_header();
    // iflyos_create_init_context();

    cJSON* header_node = iflyos_create_header();
    cJSON* context_node = iflyos_create_context();
    cJSON* request_node = NULL;

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "iflyos_header", header_node);
    cJSON_AddItemToObject(root, "iflyos_context", context_node);
    cJSON_AddItemToObject(root, "iflyos_request", request_node = cJSON_CreateObject());
    //audio-in request
    cJSON *request_header = NULL;
    cJSON *request_payload = NULL;

    cJSON_AddItemToObject(request_node, "header", request_header = cJSON_CreateObject());
    cJSON_AddItemToObject(request_node, "payload", request_payload = cJSON_CreateObject());

    cJSON_AddStringToObject(request_header, "name", recog_audion_in);
    cJSON_AddStringToObject(request_header, "request_id", "");

    cJSON *payload_wakeup = NULL;
    cJSON_AddStringToObject(request_payload, "profile", "CLOSE_TALK");
    cJSON_AddStringToObject(request_payload, "format", "AUDIO_L16_RATE_16000_CHANNELS_1");
    cJSON_AddItemToObject(request_payload, "iflyos_wake_up", payload_wakeup = cJSON_CreateObject());

    cJSON_AddNumberToObject(payload_wakeup, "score", 666);
    // cJSON_AddNumberToObject(payload_wakeup, "start_index_in_samples", 50);
    // cJSON_AddNumberToObject(payload_wakeup, "end_index_in_samples", 150);
    cJSON_AddStringToObject(payload_wakeup, "word", "小童小童");
    cJSON_AddStringToObject(payload_wakeup, "prompt", "我在");

    char* request = cJSON_Print(root);

    // cJSON_free(header_node);
    // cJSON_free(context_node);
    cJSON_free(root);
    
    return request;
}

char* iflyos_create_txt_in_request(const char* txt_buffer)
{
    if(NULL == txt_buffer)
    {
        return NULL;
    }

    cJSON *root = NULL;
    // iflyos_init_request();
    cJSON* header_node = iflyos_create_header();
    cJSON* context_node = iflyos_create_context();
    cJSON* request_node = NULL;

    

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "iflyos_header", header_node);
    cJSON_AddItemToObject(root, "iflyos_context", context_node);
    cJSON_AddItemToObject(root, "iflyos_request", request_node = cJSON_CreateObject());

    //audio-in request
    cJSON *request_header = NULL;
    cJSON *request_payload = NULL;
    cJSON_AddItemToObject(request_node, "header", request_header = cJSON_CreateObject());
    cJSON_AddItemToObject(request_node, "payload", request_payload = cJSON_CreateObject());

    cJSON_AddStringToObject(request_header, "name", recog_text_in);
    cJSON_AddStringToObject(request_header, "request_id", "");

    cJSON_AddStringToObject(request_payload, "query", txt_buffer);
    cJSON_AddBoolToObject(request_payload, "with_tts", TRUE);
    
    char* request = cJSON_Print(root);

    //for test
    // cJSON_free(header_node);
    // cJSON_free(context_node);
    cJSON_free(root);
    //end test

    return request;
}

char* iflyos_create_set_wakeword_request()
{
    cJSON *root = NULL;

    // iflyos_init_request();

    cJSON* header_node = iflyos_create_header();
    cJSON* context_node = iflyos_create_context();
    
    
    cJSON* request_node = NULL;

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "iflyos_header", header_node);
    cJSON_AddItemToObject(root, "iflyos_context", context_node);

    cJSON_AddItemToObject(root, "iflyos_request", request_node = cJSON_CreateObject());

    //set wakeword request
    cJSON *request_header = NULL;
    cJSON *request_payload = NULL;
    cJSON_AddItemToObject(request_node, "header", request_header = cJSON_CreateObject());
    cJSON_AddItemToObject(request_node, "payload", request_payload = cJSON_CreateObject());

    cJSON_AddStringToObject(request_header, "name", wakeword_result);
    cJSON_AddStringToObject(request_header, "request_id", "");

    
    char* request = cJSON_Print(root);

    //for test
    // cJSON_free(header_node);
    // cJSON_free(context_node);
    cJSON_free(root);

    //end test

    return request;
}