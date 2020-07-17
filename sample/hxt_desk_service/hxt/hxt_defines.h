#ifndef __HXT_DEFINES_H__
#define __HXT_DEFINES_H__


#define HXT_GET_TOKEN_URL               "/Authorize/GetDeskToken"
#define HXT_STATUS_REPORT               "/Device/DeskStatusReport"
#define HXT_STUDY_REPORT                "/User/StudyReport"
#define HXT_STUDY_REPORT_BATCH          "/User/StudyReportBatch"
#define HXT_UPLOAD_FILE                 "/Upload/UploadFileCustom?absoluteUrl=true"
#define HXT_GETMAX_CHUNK                "/Upload/GetMaxChunk?md5=%s&ext=%s"
#define HXT_UPLOAD_CHUNK                "/Upload/Chunkload?md5=%s&chunk=%d&chunks=%d"
#define HXT_MERGE_FILES                 "/Upload/MergeFiles?md5=%s&ext=%s&fileTotalSize=%lu&typeString=%s"
#define HXT_CHECK_WIFI_DATA             "/Device/CheckWifiData"
#define HXT_BIND_DESK_WIFI              "/Device/BindDeskByWifi"

#define HXT_RET_OK                      1
#define HXT_STATUS_OK                   "S0001"

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