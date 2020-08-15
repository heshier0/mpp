#ifndef __HXT_DEFINES_H__
#define __HXT_DEFINES_H__


#define HXT_GET_TOKEN_URL               "/Authorize/GetDeskToken"
#define HXT_STATUS_REPORT               "/Device/DeskStatusReport"
#define HXT_STUDY_REPORT                "/User/StudyReport"
#define HXT_STUDY_REPORT_BATCH          "/User/StudyReportBatch"
#define HXT_UPLOAD_FILE                 "/Upload/UploadFileCustom?childunid=%d"
#define HXT_GETMAX_CHUNK                "/Upload/GetMaxChunk?md5=%s&ext=%s"
#define HXT_UPLOAD_CHUNK                "/Upload/Chunkload?md5=%s&chunk=%d&chunks=%d"
#define HXT_MERGE_FILES                 "/Upload/MergeFiles?md5=%s&ext=%s&fileTotalSize=%lu&typeString=%s"
#define HXT_CHECK_WIFI_DATA             "/Device/CheckWifiData"
#define HXT_BIND_DESK_WIFI              "/Device/BindDeskByWifi"

#define HXT_UPLOAD_SAMPLE_SNAP          "/Upload/UploadPicsAsync?childunid=%d"

#define HXT_RET_OK                      1
#define HXT_STATUS_OK                   "S0001"


#define HXT_CONNECT_OK          0
#define HXT_DEFAULT_CFG         1
#define HXT_UPDATE_REMIND       2
#define HXT_WAKE_CAMERA         3
#define HXT_VARY_TONE           4
#define HXT_TONE_TYPE           5
#define HXT_VERIFY_CODE         6
#define HXT_BIND_CHILD_ID       7
#define HXT_VARY_CHILD_ID       8
#define HXT_GET_IFLYOS_TOKEN    10
#define HXT_STOP_STUDY          14
#define HXT_DISCONNECT          15
#define HXT_POWEROFF            16
#define HXT_RESTART             17

#define HXT_DESK_STATUS         11
#define HXT_STUDY_INFO          12
#define HXT_STUDY_INFOS         13

#pragma pack(push, 1)
typedef struct hxt_result
{
    char pass_status;
    char code[5];
    int status;
    char desc[32];
    char msg[512];
}HxtResult;

typedef struct hxt_children_data
{
    int unid;
    int study_mode;
    int alarm_type;
}HxtChildrenData;

typedef struct hxt_wifi_data
{
    char wifi_ssid[32];
    char wifi_pwd[32];
}HxtWifiData;


#pragma pack(pop)

#endif //__HXT_DEFINES_H__