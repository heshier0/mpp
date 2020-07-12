#ifndef __IFLYOS_REQUEST_DEF_H__
#define __IFLYOS_REQUEST_DEF_H__

#include "iflyos_defines.h"

#pragma pack(push, 1)
//recognizer
/******
 * proifle: CLOSE_TALK, FAR_FIELD, EVALUATE
 * format: AUDIO_L16_RATE_16000_CHANELS_1
 * category: read_chapter, read_sentence, read_word, read_syllable
 *****/
typedef struct iflyos_recog_ai_request
{
    char request_name[64];
    char request_id[64];
    char reply_key[64];
    BOOL enable_vad;
    int vad_eos;
    char profile[10];
    char format[32];
    char wake_up_word[32];
    int wake_up_score;
    int start_index;
    int end_index;
    char prompt[32];
    char language[8];
    char category[16];
    char* text;
}FlyosRecogAiRequest;

typedef struct iflyos_recog_ti_request
{
    char request_name[64];
    char request_id[64];
    char* query;
    BOOL with_tts;
    char reply_key[64];
}FlyosRecogTiRequest;

//system
typedef struct iflyos_sys_update_request
{
    char request_name[64];
    char request_id[64];
    char result[8];
    BOOL need_update;
    char ver_name[6];
    char* description;
}FlyosSysUpdateRequest;

typedef struct iflyos_sys_update_sync_request
{
    char request_name[64];
    char request_id[64];
    char state[10];
    char* ver_name;
    char* description;
    char type[16];
    char* message;
}FlyosSysUpdateSyncRequest;

typedef struct iflyos_sys_exception_request
{
    char request_name[64];
    char request_id[64];
    char* type;
    char* code;
    char* message;
}FlyosSysExceptionRequest;

//audio player
typedef struct iflyos_audio_play_sync_request
{
    char request_name[64];
    char request_id[64];
    char type[16];
    char resource_id[64];
    char* offset;
    long fail_code;
}FlyosAudioPlaySyncRequest;

typedef struct iflyos_audio_other_sync_request
{
    char request_name[64];
    char request_id[64];
    char type[8];
    char resource_id[64];
}FlyosAudioOtherSyncRequest;

typedef struct iflyos_audio_tts_in_request
{
    char request_name[64];
    char request_id[64];
    char* text;
    int number;
    int volume;
    char* vcn;
}FlyosAudioTtsInRequest;
#pragma pack(pop)


#endif //__IFLYOS_REQUEST_DEF_H__