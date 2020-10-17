#ifndef __IFLYOS_REQUEST_DEF_H__
#define __IFLYOS_REQUEST_DEF_H__


#define PLATFORM_NAME       "linux"
#define PLATFORM_VER        "4.9.37"

#define SYSTEM_VER          "1.3"

#define AUDIO_PLAYER_VER        "1.2"
#define AUDIO_PLAYER_STATE      "IDLE"

#define SPEAKER_VER             "1.0"
#define SPEAKER_VOL             10
#define SPEAKER_TYPE            "percent"

/****request define****/
//recognizer
#define recog_audion_in          "recognizer.audio_in"
#define recog_text_in            "recognizer.text_in" 
//system
#define sys_state_sync           "system.state_sync"
#define sys_exception            "system.exception"
#define sys_sw_update_result     "system.check_software_update_result"
#define sys_update_sw_state      "system.update_software_state_sync"
//audio player
#define aplayer_pl_prog_sync     "audio_player.playback.progress_sync"
#define aplayer_tts_prog_sync    "audio_player.tts.progress_sync"
#define aplayer_ring_prog_sync   "audio_player.ring.progress_sync"
#define aplayer_tts_text_in      "audio_player.tts.text_in"
//local alarm
#define alarm_state_sync         "alarm.state_sync"
//video player
#define vplayer_prog_sync        "vedio_player.progress_sync"
//playback controller
#define pc_ctrl_cmd              "playback_controller.control_command"
//app action
#define app_check_result         "app_action.check_result"
#define app_exec_success         "app_action.execute_succeed"
#define app_exec_fail            "app_action.execute_failed"
//template
#define tmpl_elem_sel            "template.element_selected"
//launcher
#define launch_sa_result         "launcher.start_activity_result"
#define launch_back_result       "launcher.back_result"
#define launch_sel_result        "launcher.select_result"
//wakeword
#define wakeword_result                "wakeword.set_wakeword_result"

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