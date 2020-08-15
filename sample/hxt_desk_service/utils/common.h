#ifndef __COMMON_H__
#define __COMMON_H__



#define MP3_FIFO                "/tmp/mp3_fifo"
#define PCM_FIFO                "/tmp/pcm_fifo"
#define VIDEO_FIFO              "/tmp/video_fifo"

//wifi config file
#define WIFI_CFG                "/userdata/config/wifi.conf"

#define STUDY_INFO_MQ_KEY       (232323L)

//local voice
#define VOICE_DEVICE_OPEN                       "/userdata/media/voice/V001.mp3"
#define VOICE_CONNECT_ERROR                     "/userdata/media/voice/V002.mp3"  
#define VOICE_WIFI_BIND_OK                      "/userdata/media/voice/V003.mp3"  
#define VOICE_WIFI_BIND_ERROR                   "/userdata/media/voice/V004.mp3"
#define VOICE_FIND_NETWORK                      "/userdata/media/voice/V005.mp3"
#define VOICE_NORMAL_STATUS                     "/userdata/media/voice/V006.mp3"
#define VOICE_WAKE_CAMERA                       "/userdata/media/voice/V007.mp3"
#define VOICE_VOLUME_10                         "/userdata/media/voice/V008.mp3"
#define VOICE_VOLUME_20                         "/userdata/media/voice/V009.mp3"
#define VOICE_VOLUME_30                         "/userdata/media/voice/V010.mp3"
#define VOICE_VOLUME_40                         "/userdata/media/voice/V011.mp3"
#define VOICE_VOLUME_50                         "/userdata/media/voice/V012.mp3"
#define VOICE_VOLUME_60                         "/userdata/media/voice/V013.mp3"
#define VOICE_VOLUME_70                         "/userdata/media/voice/V014.mp3"
#define VOICE_VOLUME_80                         "/userdata/media/voice/V015.mp3"
#define VOICE_VOLUME_90                         "/userdata/media/voice/V016.mp3"
#define VOICE_VOLUME_MAX                        "/userdata/media/voice/V017.mp3"
#define VOICE_BRIGHTNESS_MIN                    "/userdata/media/voice/V018.mp3"
#define VOICE_BRIGHTNESS_MID                    "/userdata/media/voice/V019.mp3"
#define VOICE_BRIGHTNESS_MAX                    "/userdata/media/voice/V020.mp3"
#define VOICE_SITTING_WARM1                     "/userdata/media/voice/V021.mp3"
#define VOICE_SITTING_WARM2                     "/userdata/media/voice/V022.mp3"
#define VOICE_SITTING_WARM3                     "/userdata/media/voice/V023.mp3"
#define VOICE_SITTING_WARM4                     "/userdata/media/voice/V024.mp3"
#define VOICE_SITTING_WARM5                     "/userdata/media/voice/V025.mp3"
#define VOICE_CHILD_AWAY                        "/userdata/media/voice/V026.mp3"
#define VOICE_CHILD_REAPPEAR                    "/userdata/media/voice/V027.mp3"
#define VOICE_CAMERA_SLEEP                      "/userdata/media/voice/V028.mp3"
#define VOICE_MUTE_CLOSE                        "/userdata/media/voice/V029.mp3"  
#define VOICE_WARING_TYPE                       "/userdata/media/voice/V030.mp3"  
#define VOICE_WARNING_BUZZ                      "/userdata/media/voice/V031.mp3"  
#define VOICE_WARNING_MUTE                      "/userdata/media/voice/V032.mp3"  
#define VOICE_DESK_DEREGISTER                   "/userdata/media/voice/V033.mp3"  
#define VOICE_SITTING_PRAISE1                   "/userdata/media/voice/V034.mp3" 
#define VOICE_SITTING_PRAISE2                   "/userdata/media/voice/V035.mp3"
#define VOICE_SITTING_PRAISE3                   "/userdata/media/voice/V036.mp3"
#define VOICE_SITTING_PRAISE4                   "/userdata/media/voice/V037.mp3"
#define VOICE_SITTING_PRAISE5                   "/userdata/media/voice/V038.mp3"

#pragma pack(push, 1)

typedef enum
{
    STUDY_BEGIN = 1,
    STUDY_END,
    CHILD_AWAY, 
    CHILD_BACK,
    BAD_POSTURE
}STUDY_REPORT_TYPE;

typedef struct study_info_s
{
    long msg_type;
    int info_type;
    char file[128];
    char snap[128];
}StudyInfo;

typedef struct study_info_msg_s
{
    long msg_type;
    StudyInfo* info;
}StudyInfoMsg;


typedef struct report_info_s
{
    int child_unid; 
    int parent_unid;
    int report_type;
    char *study_date;
    char *report_time;
    int study_mode;
    int duration;
    char *video_url;
    char *snap_url;
    int camera_status;
}ReportInfo;


#pragma pack(pop)

int g_play_status;
int g_voice_status;
int g_video_status;


#endif //__COMMON_H__