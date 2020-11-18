#ifndef __DB_H__
#define __DB_H__


#include <sqlite3.h>
#include "common.h"

BOOL open_hxt_service_db();
BOOL close_hxt_service_db();
void deinit_hxt_service_db();

BOOL delete_table(const char* table_name);
int get_number_value_from_table(const char* table_name, const char* col_name);
char* get_string_value_from_table(const char* table_name, const char* col_name);
BOOL set_number_value_into_table(const char* table_name, const char* col_name, const int value);
BOOL set_string_value_into_table(const char* table_name, const char* col_name, const char* value);


/*desk params*/
BOOL set_device_id(const char* uuid);
BOOL set_desk_bind_status(const int status);
BOOL set_parent_id(const int parent_id);
char* get_device_id();
int get_desk_bind_status();
int get_parent_id();
BOOL deinit_desk_params();

/*iflyos params*/
BOOL set_iflyos_params(const char* token, const char* sn);
char* get_iflyos_token();
char* get_iflyos_sn();
BOOL deinit_iflyos_params();

/*wifi params*/
BOOL set_wifi_params(const  char* ssid, const char* pwd, const char* check_code);
char* get_wifi_ssid();
char* get_wifi_pwd();
char* get_wifi_check_code();
BOOL deinit_wifi_params();

/*server params*/
BOOL set_server_params(const char* wscUrl, const char* uploadUrl);
BOOL set_server_url(const char* url);
BOOL set_websocket_url(const char* url);
BOOL set_upload_url(const char* url);
char* get_server_url();
char* get_api_version();
char* get_websocket_url();
char* get_upload_url();
BOOL deinit_server_params();

/*connect params*/
BOOL set_connect_params(const char* token, const int tokenExpiredTime);
char* get_server_token();
int get_server_token_expired_time();
BOOL deinit_connect_params();

/*update params*/
BOOL set_update_params(const int version_id, const char* version_no, const char* packUrl);
int get_update_version_id();
char* get_update_version_no();
char* get_update_packUrl();
BOOL deinit_update_params();

/*running params*/
BOOL set_running_params(int judge_time, int video_length, int video_ratio, int video_cnt, int snap_count, int offline_storage, int attach_ratio);
int get_judge_time();
int get_video_length();
int get_video_ratio();
int get_video_count();
int get_snap_count();
int get_offline_storage();
int get_attach_ratio();
BOOL deinit_running_params();

/*user params*/
BOOL set_user_params(int child_id, int study_mode, int alarm_type, int selected);
BOOL update_select_child(int child_id);
BOOL update_study_mode(int child_id, int sudy_mode);
BOOL update_alarm_type(int child_id, int study_mode);
BOOL set_all_unselected();
int get_select_child_id();
int get_study_mode(int child_unid);
int get_alarm_type(int child_unid);
BOOL delete_child(int child_unid);
BOOL deinit_user_params();

/*volume info*/
int check_mute();
BOOL set_mute(BOOL mute);
int get_volume();
BOOL set_volume(int vol);

/*upload count*/
BOOL create_upload_count_info(int child_unid);
BOOL inc_upload_count(int child_unid);
int get_upload_count_of_day(int child_unid);
BOOL init_upload_count(int child_unid);
BOOL deinit_upload_count_params();


#endif //!__DB_H__