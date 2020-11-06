#ifndef __HXT_CLIENT_H__
#define __HXT_CLIENT_H__

#include "hxt_defines.h"

BOOL hxt_query_wifi_info(void *data);
BOOL hxt_bind_desk_with_wifi_request();
BOOL hxt_confirm_desk_bind_request();
BOOL hxt_get_token_request();
BOOL hxt_refresh_token_request();
BOOL hxt_get_desk_cfg_request();
BOOL hxt_get_new_version_request(const char* update_url);
int hxt_file_upload_request(const char* filename, const char* study_date, char* server_file_path);
BOOL hxt_sample_snap_upload_request(const char* filename, const char* study_date, char* server_file_path);
BOOL hxt_update_children_alarm_files(void* data);
BOOL hxt_update_children_alarm_file(void* data);
BOOL hxt_parse_user_data(void* data);
BOOL hxt_unbind_child(int child_unid);

BOOL hxt_get_aliyun_config(void **opt);
BOOL init_upload_options(void **opts, void *data);
void deinit_upload_options(void *opts);
char* hxt_upload_file(const char* path, void *opts);
BOOL hxt_init_aliyun_env();
void hxt_deinit_aliyun_env();

char* hxt_get_posture_detect_model_path(int study_mode);
char* hxt_get_posture_class_model_path(int study_mode);

int hxt_get_video_width();
int hxt_get_video_height();





#endif //!__HXT_CLIENT_H__