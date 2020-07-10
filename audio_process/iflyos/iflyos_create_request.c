#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

#include <cJSON.h>

#include "iflyos_common_def.h"

static FlyosHeader* inited_header;
static FlyosContext* inited_context;

static void iflyos_create_init_header()
{
    inited_header = (FlyosHeader *)utils_malloc(sizeof(FlyosHeader));

    char* token_type = iflyos_get_token_type();
    char* token = iflyos_get_token();
    strcpy(inited_header->authorization, token_type);
    strcat(inited_header->authorization, " ");
    strcat(inited_header->authorization, token);

    char* device_id = iflyos_get_device_id();
    strcpy(inited_header->device_id, device_id);

    char* platform_name = iflyos_get_platform_name();
    strcpy(inited_header->platform_name, platform_name);

    char* platform_version = iflyos_get_platform_version();
    strcpy(inited_header->platform_version, platform_version);
}

static void iflyos_create_init_context()
{
    inited_context = (FlyosContext *)utils_malloc(sizeof(FlyosContext));

    FlyosContextSystem context_system;
    memset(&context_system, 0, sizeof(FlyosContextSystem));
    char* sys_version = iflyos_get_system_version();
    strcpy(context_system.version, sys_version);

    FLyosContextAudioPlayer audio_player;
    memset(&audio_player, 0, sizeof(FLyosContextAudioPlayer));
    char* audio_version = iflyos_get_audio_version();
    char* audio_state = iflyos_get_audio_state();
    strcpy(audio_player.version, audio_version);
    strcpy(audio_player.state, audio_state);

    FlyosContextSpeaker speaker;
    memset(&speaker, 0, sizeof(FlyosContextSpeaker));
    char* speaker_version = iflyos_get_speaker_version();
    char* speaker_type = iflyos_get_speaker_type();
    int speaker_vol = iflyos_get_speaker_volume();
    strcpy(speaker.version, speaker_version);
    strcpy(speaker.type, speaker_type);
    speaker.volume = speaker_vol;

    inited_context->system = (FlyosContextSystem *)utils_malloc(sizeof(FlyosContextSystem));
    memcpy(inited_context->system, &context_system, sizeof(FlyosContextSystem));
    
    inited_context->audio_player = (FLyosContextAudioPlayer *)utils_malloc(sizeof(FLyosContextAudioPlayer));
    memcpy(inited_context->audio_player, &audio_player, sizeof(FLyosContextAudioPlayer));

    inited_context->speaker = (FlyosContextSpeaker *)utils_malloc(sizeof(FlyosContextSpeaker));
    memcpy(inited_context->speaker, &speaker, sizeof(FlyosContextSpeaker));

    return;
}

static void iflyos_destroy_header(FlyosHeader* header)
{
    if (NULL == header)
    {
        return;
    }
    utils_free(header);
    header = NULL;

    return;
}

static void iflyos_destroy_context(FlyosContext* context)
{
    if(NULL == context)
    {
        return;
    }
   
    if(context->system != NULL)
    {
        utils_free(context->system);
        context->system = NULL;
    }
    
    if(context->audio_player != NULL)
    {
        utils_free(context->audio_player);
        context->audio_player = NULL;
    }

    if(context->speaker != NULL)
    {
        utils_free(context->speaker);
        context->speaker = NULL;
    }

    utils_free(context);
    context = NULL;

    return;
}

static cJSON* iflyos_create_header(FlyosHeader* header)
{
    if (NULL == header)
    {
        return NULL;
    }

    cJSON *root = NULL;
    cJSON *device = NULL;
    cJSON *location = NULL;
    cJSON *platform = NULL;

    root = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "authorization", cJSON_CreateString(header->authorization));
    cJSON_AddItemToObject(root, "device", device = cJSON_CreateObject());

    cJSON_AddStringToObject(device, "device_id", header->device_id);
    cJSON_AddStringToObject(device, "ip", header->device_ip);
    cJSON_AddItemToObject(device, "location", location = cJSON_CreateObject());
    cJSON_AddItemToObject(device, "platform", platform = cJSON_CreateObject());

    cJSON_AddNumberToObject(location, "latitude", header->latitude);
    cJSON_AddNumberToObject(location, "longtitude", header->longitude);
    
    cJSON_AddStringToObject(platform, "name", header->platform_name);
    cJSON_AddStringToObject(platform, "version", header->platform_version);

    return root;
}

static cJSON* iflyos_create_context(FlyosContext* context)
{
    if (NULL == context)
    {
        return;
    }

    cJSON* root = NULL;
    cJSON* system = NULL;
    cJSON* audio_player = NULL;
    cJSON* playback = NULL;
    cJSON* speaker = NULL;

    root = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "system", system = cJSON_CreateObject());
    cJSON_AddItemToObject(root, "audio_player", audio_player = cJSON_CreateObject());
    cJSON_AddItemToObject(root, "speaker", speaker = cJSON_CreateObject());

    cJSON_AddStringToObject(system, "version", context->system->version);
    cJSON_AddBoolToObject(system, "software_updater", context->system->software_updater);
    cJSON_AddBoolToObject(system, "power_controller", context->system->power_controller);
    cJSON_AddBoolToObject(system, "device_modes", context->system->device_modes);
    cJSON_AddBoolToObject(system, "factory_reset", context->system->factory_reset);
    cJSON_AddBoolToObject(system, "reboot", context->system->reboot);

    cJSON_AddStringToObject(audio_player, "version", context->audio_player->version);
    cJSON_AddItemToObject(audio_player, "playback", playback = cJSON_CreateObject());
    cJSON_AddStringToObject(playback, "state", context->audio_player->state);
    cJSON_AddStringToObject(playback, "resource_id", context->audio_player->resource_id);
    cJSON_AddNumberToObject(playback, "offset", context->audio_player->offset);

    cJSON_AddStringToObject(speaker, "version", context->speaker->version);
    cJSON_AddNumberToObject(speaker, "volume", context->speaker->volume);
    cJSON_AddStringToObject(speaker, "type", context->speaker->type);

    return root;
}

static void iflyos_init_request()
{
    iflyos_create_init_header();
    iflyos_create_init_context();
}

void iflyos_deinit_request()
{
    iflyos_destroy_header(inited_header);
    iflyos_destroy_context(inited_context);
}

char*  iflyos_create_audio_in_request()
{
    cJSON *root = NULL;

    iflyos_init_request();

    cJSON* header_node = iflyos_create_header(inited_header);
    cJSON* context_node = iflyos_create_context(inited_context);
    
    
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

    cJSON_AddStringToObject(request_payload, "profile", "CLOSE_TALK");
    cJSON_AddStringToObject(request_payload, "format", "AUDIO_L16_RATE_16000_CHANNELS_1");
    
    char* request = cJSON_Print(root);

    //for test
    cJSON_free(header_node);
    cJSON_free(context_node);
    cJSON_free(root);

    iflyos_deinit_request();
    //end test

    return request;
}

char* iflyos_create_txt_in_request()
{
    cJSON *root = NULL;

    cJSON* header_node = iflyos_create_header(inited_header);
    cJSON* context_node = iflyos_create_context(inited_context);
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

    cJSON_AddStringToObject(request_payload, "query", "今天天气怎么样");
    cJSON_AddBoolToObject(request_payload, "with_tts", TRUE);
    
    char* request = cJSON_Print(root);

    //for test
    cJSON_free(header_node);
    cJSON_free(context_node);
    cJSON_free(root);
    //end test

    return request;
}

int iflyos_get_audio_data_handle()
{
    int fd = -1;
    const char* fifo_pcm = "/tmp/my_pcm_fifo";
    if (access(fifo_pcm, F_OK) == -1)
    {
        int res = mkfifo(fifo_pcm, 0777);
        if(res != 0)
        {
            utils_print("could not create fifo %s\n", fifo_pcm);
            return -1;
        }
    }
    fd = open(fifo_pcm, O_RDONLY | O_NONBLOCK);
    if (fd == -1)
    {
        utils_print("pcm fifo open error\n");
        return -1;
    }

    return fd; 
}
