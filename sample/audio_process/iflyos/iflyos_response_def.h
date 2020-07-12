#ifndef __IFLYOS_RESPONSE_DEF_H__
#define __IFLYOS_RESPONSE_DEF_H__

#include "iflyos_defines.h"

#pragma pack(push,1)
//intermediate text
typedef struct iflyos_recog_it_response
{
    char header[64];
    char* text;
    BOOL is_last;
}FlyosRecogITResponse;

//expect reply
typedef struct iflyos_recog_er_response
{
    char header[64];
    char reply_key[64];
    BOOL bg_recognize;
    long timeout;
}FlyosRecogERResponse;

//evaluate reply
typedef struct iflyos_recog_ev_response
{
    char header[64];
    int code;
    char description[32];
    char sid[64];
    char* data;
}FlyosRecogEVRespons;

//ping
typedef struct iflyos_sys_ping_response
{
    char header[64];
    long timestam;
}FlyosSysPingResponse;

//error
typedef struct iflyos_sys_error_response
{
    char header[64];
    int code;      
    char* message;
}FlyosSysErrorResponse;

//udpate device modes
typedef struct iflyos_sys_modes_response
{
    char* header;
    BOOL kid;
    BOOL interaction;
}FlyosSysModesResponse;

//audio player
typedef struct iflyos_audio_out_response
{
    char header[64];
    char* type;
    char* control;
    char* behavior;
    char* url;
    char* secure_url;
    char* resource_id;
    long offset;
    long duration;
    char* text; 
}FlyosAudioOutResponse;


#pragma pack(pop)


#endif //__IFLYOS_RESPONSE_DEF_H__