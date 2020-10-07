#ifndef __IFLYOS_RESPONSE_DEF_H__
#define __IFLYOS_RESPONSE_DEF_H__


/*****response define*****/
//recognizer
#define recog_intermediate_text      "recognizer.intermediate_text"
#define recog_stop_capture           "recognizer.stop_capture"
#define recog_expect_reply           "recognizer.expect_reply"
#define recog_evaluate_result        "evaluate_result"
//system
#define sys_ping                     "system.ping"
#define sys_error                    "system.error"
#define sys_chk_sw_update            "system.check_software_update"
#define sys_update_sw                "system.update_software"
#define sys_power_off                "system.power_off"
#define sys_update_dev_modes         "system.update_device_modes"
#define sys_factory_rst              "system.factory_reset"
#define sys_reboot                   "system.reboot"
#define sys_revoke_response          "system.revoke_response"
#define sys_update_alarm_list        "system.update_cloud_alarm_list"
//audio player
#define aplayer_audio_out            "audio_player.audio_out"
//local alarm
#define alarm_set_alarm              "alarm.set_alarm"
#define alarm_delete_alarm           "alarm.delete_alarm"
//speaker
#define spk_set_volume               "speaker.set_volume"
//video player
#define vplayer_video_out            "video_player.video_out"
//app action
#define app_excute                   "app_action.excute"
#define app_check                    "app_action.check"
//screen control
#define screen_set_state             "screen.set_state"
#define screen_set_bright            "screen.set_brightness"
//template
#define tmpl_static                  "template.static_template"
#define tmpl_playing                 "template.playing_template"
#define tmpl_custom                  "template.custom_template"
#define tmpl_exit                    "template.exit"
//launcher
#define launch_sa                    "launcher.start_activity"
#define launch_back                  "launcher.back"
#define launch_sel                   "launcher.select"
//wake word
#define wakeword_set_wakeword        "wakeword.set_wakeword"
//interceptor
#define itcpt_custom                 "interceptor.custom"
#define itcpt_trans_sema             "interceptor.transfer_semantic"

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