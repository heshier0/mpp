#ifndef __HXT_DEFINES_H__
#define __HXT_DEFINES_H__

#include "common.h"




/* http request url */
#define HXT_SERVER_URL                      "https://www.horxeton.com:7002/api/v1"

#define HXT_GET_TOKEN                       "/Authorize/GetDeskToken"
#define HXT_GET_DESK_CONFIG                 "/Device/GetDeskConfig"
#define HXT_STATUS_REPORT                   "/Device/DeskStatusReport"
#define HXT_STUDY_REPORT                    "/User/StudyReport"
#define HXT_STUDY_REPORT_BATCH              "/User/StudyReportBatch"
#define HXT_UPLOAD_FILE                     "/Upload/UploadFileCustom?childunid=%d\\&studyDate=%s"
#define HXT_GETMAX_CHUNK                    "/Upload/GetMaxChunk?md5=%s\\&ext=%s"
#define HXT_UPLOAD_CHUNK                    "/Upload/Chunkload?md5=%s\\&chunk=%d\\&chunks=%d"
#define HXT_MERGE_FILES                     "/Upload/MergeFiles?md5=%s\\&ext=%s\\&fileTotalSize=%lu\\&typeString=%s"
#define HXT_CHECK_WIFI_DATA                 "/Device/CheckWifiData"
#define HXT_BIND_DESK_WIFI                  "/Device/BindDeskByWifi"
#define HXT_CONFIRM_DESK_BIND               "/Device/BindDeskConfirm"
#define HXT_UPLOAD_SAMPLE_SNAP              "/Upload/UploadPicsAsync?childunid=%d"

/* http response result */
#define HXT_OK                              1
#define HXT_NO_REGISTER                     0
#define HXT_BIND_FAILED                     21         
#define HXT_AUTH_FAILED                     401

#define HXT_RES_STATUS_OK                   "S0001"
#define HXT_RES_BIND_FAIL                   "S0301"
#define HXT_RES_AUTH_FAIL                   "S0401"
#define HXT_RES_NO_REG                      "S0000"

/* hxt websocket result defines */
#define HXT_CONNECT_OK          0
#define HXT_BASIC_CFG           1
#define HXT_UPDATE_REMIND       2
#define HXT_WAKE_CAMERA         3
#define HXT_USER_DATA           4
#define HXT_ALARM_VARRY         5
#define HXT_STUDY_MODE_VARRRY   6
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

/* file path defines */
#define UPDATE_FILES                        ("/userdata/update/update.tar.gz")
#define HXT_CFG                             ("/userdata/config/hxt_config.json")
#define HXT_INIT_CFG                        ("/userdata/config/.hxt_init_config.json")
#define HXT_CHILD_VIDEO_PATH                ("/user/child_%d/video/")
#define HXT_CHILD_SNAP_PATH                 ("/user/child_%d/snap/")
#define HXT_CHILD_ALARM_FILE                ("/user/child_%d/alarm/P00%d.mp3")
#define HXT_CHILD_ALARM_FILE_TMP            ("/user/child_%d/alarm/P00%d.tmp")
#define WIFI_CONFIG                         ("/userdata/config/wifi.json")
#define IFLYOS_CAE_CONF_PATH                ("/userdata/config/iflyos/hlw.ini")
#define IFLYOS_CAE_PARAM_PATH               ("/userdata/config/iflyos/hlw.param")
#define IFLYOS_CFG                          ("/userdata/config/iflyos/iflyos_config.json") 

// #pragma pack(push, 1)
// #pragma pack(pop)

#endif //__HXT_DEFINES_H__