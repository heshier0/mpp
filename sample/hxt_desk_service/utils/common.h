#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 

#ifdef DEBUG
    #define utils_print(format, ...) printf("%s>>>%d: " format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
    #define malloc_print(__ptr__,size) printf("[ALLOC] %32s:%4d | addr= %p, size= %lu, expr= `%s`\n", __FUNCTION__, __LINE__ , __ptr__, size, #size)
    #define free_print(ptr)	printf("[ FREE] %32s:%4d | addr= %p, expr= `%s`\n", __FUNCTION__, __LINE__, ptr, #ptr)
#else
    #define malloc_print(__ptr__,size)
    #define free_print(ptr)
    #define utils_print(format, ...)
#endif 

#define utils_malloc(size) ({ \
	void *__ptr__ = malloc(size); \
	memset(__ptr__,0,size); \
    malloc_print(__ptr__,size); \
	__ptr__; \
	})

#define utils_calloc(size) ({ \
	void *__ptr__ = calloc(size, 1); \
    malloc_print(__ptr__,size); \
	__ptr__; \
	})

#define utils_free(ptr) ({ \
    free_print(ptr); \
	free(ptr); \
	})


typedef enum
{
    TRUE  = 1, 
    FALSE  = 0
}BOOL;

typedef enum 
{
    OFFLINE = 0,
    ONLINE = 1
}WORK_MODE;

#define HXT_DESK_SERVICE_VERSION        "1.0.5"
#define HXT_DESK_SERVICE_VERSION_NUMBER  100000


#define MP3_FIFO                "/tmp/mp3_fifo"
#define PCM_FIFO                "/tmp/pcm_fifo"
#define VIDEO_FIFO              "/tmp/video_fifo"
#define PCM_RECV_PORT           10099
// #define STUDY_INFO_MQ_KEY       (232323L)

/* db */
#define DB_PATH     ("/userdata/data/desk.db")
#define INIT_SQL    ("/userdata/data/init.sql")

//wifi config file
#define WIFI_CFG                ("/userdata/config/wifi.conf")

//posture pattern file
#define LOW_CLASS_DETECT_FILE   ("/userdata/pattern/low_class/detect.wk")
#define MEDIUM_CLASS_DETECT_FILE   ("/userdata/pattern/medium_class/detect.wk")
#define HIGH_CLASS_DETECT_FILE   ("/userdata/pattern/high_class/detect.wk")
#define LOW_CLASS_SAMPLE_FILE   ("/userdata/pattern/low_class/class.wk")
#define MEDIUM_CLASS_SAMPLE_FILE   ("/userdata/pattern/medium_class/class.wk")
#define HIGH_CLASS_SAMPLE_FILE   ("/userdata/pattern/high_class/class.wk")

//local voice
#define VOICE_DEVICE_OPEN                       "/userdata/media/voice/V001.mp3"
#define VOICE_CONNECT_ERROR                     "/userdata/media/voice/V002.mp3"  
#define VOICE_SERVER_CONNECT_OK                 "/userdata/media/voice/V003.mp3"  
#define VOICE_QUERY_WIFI_ERROR                  "/userdata/media/voice/V004.mp3"
#define VOICE_DEPLOYING_NET                     "/userdata/media/voice/V005.mp3"
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
#define VOICE_QUERY_WIFI_INFO                   "/userdata/media/voice/V039.mp3"
#define VOICE_IFLYOS_READY                      "/userdata/media/voice/V040.mp3"
#define VOICE_IFLYOS_ECHO                       "/userdata/media/voice/V041.mp3"
#define VOICE_BEEP                              "/userdata/media/voice/fengming.mp3"

/* db table name */
#define IFLYOS_DEVICE_TABLE                 "IflyosParams"
#define DESK_PARAMS_TABLE                   "DeskParams"
#define SERVER_PARAMS_TABLE                 "ServerParams"
#define WIFI_PARAMS_TABLE                   "WifiParams"
#define CONNECT_PARAMS_TABLE                "ConnectParams"
#define RUNNING_PARAMS_TABLE                "RunningParams"
#define UPDATE_PARAMS_TABLE                 "UpdateParams"
#define USER_PARAMS_TABLE                   "UserParams"
#define REPORT_INFOS_TABLE                  "ReportInfos"
#define VOLUME_INFOS_TABLE                   "VolumeInfos"

#pragma pack(push, 1)

typedef enum
{
    BOOTING = 0,
    NO_BIND = 1,
    BINDING = 2,
    NORMAL = 3,
    CHECKING = 4,
    WIFI_ERR = 5,
    CAMERA_ERR = 6,
    RESETING = 7,
    SLEEPING = 8
}LED_STATUS;

typedef enum
{
    STUDY_BEGIN = 1,
    STUDY_END,
    CHILD_AWAY, 
    CHILD_BACK,
    BAD_POSTURE
}STUDY_REPORT_TYPE;

typedef enum
{
    CAMERA_ON = 1,
    CAMERA_OFF = 2,
    CAMERA_FAIL = 3
}CAMERA_STATUS;

typedef struct study_info_s
{
    long msg_type;
    int info_type;
    char file[128];
    char snap[128];
}StudyInfo;

typedef struct report_info_s
{
    int parent_unid;
    int child_unid; 
    int report_type;
    char study_date[32];
    char report_time[32];
    int study_mode;
    int duration;
    char video_url[256];
    char snap_url[256];
    int camera_status;
}ReportInfo;

#pragma pack(pop)


int g_play_status;
int g_voice_status;
int g_video_status;

int g_child_binding;


#endif //__COMMON_H__