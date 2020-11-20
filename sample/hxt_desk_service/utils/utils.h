#ifndef __UTILS_H__
#define __UTILS_H__

#include "hxt_defines.h"

#include <cJSON.h>

char *utils_get_current_time();

BOOL utils_send_mp3_voice(const char *url);
BOOL utils_send_local_voice(const char *path);

BOOL utils_download_file(const char *url, const char* save_file_path);
// BOOL utils_upload_file(const char* url, const char* header, const char* local_file_path, char* out_buffer, int buffer_length);
BOOL utils_upload_file(const char* url, const char* header, const char* local_file_path, char **out);
//BOOL utils_post_json_data(const char *url, const char* header_content, const char* json_data, char* out, int out_length);
BOOL utils_post_json_data(const char* url, const char* header_content, const char* json_data, char **out);
// BOOL utils_send_get_request(const char* url, const char* header_content, char* out, int out_length);
BOOL utils_send_get_request(const char* url, const char* header_content, char** out);

int utils_open_fifo(const char* name, int mode);
/*use linux shell cmd and linux pipe to achieve*/
//display format "yyyy-mm-dd"
char* utils_date_to_string();
//display format "yyyy-mm-dd HH:MM:SS" 
char* utils_time_to_string();
//change "yyyy-mm-dd HH:MM:SS" to integer
int utils_change_time_format(const char* str_time);

char* utils_get_file_md5sum(const char* file_name);
int utils_split_file_to_chunk(const char* path);
unsigned long utils_get_file_size(const char* path);

void utils_link_wifi(const char* ssid, const char* pwd);
void utils_disconnect_wifi();
BOOL utils_check_wifi_state();
void utils_system_reboot();
void utils_system_reset();

void utils_generate_mp4_file_name(char* file_name);

int utils_query_file_count(char* path);
void utils_query_file_names(const char *path, char **file_list);

void utils_save_yuv_test(const char* yuv_data, const int width, const int height);
void utils_save_pcm_test(const char* pcm_data, int length);

// int utils_send_msg(void* data, int length);
// int utils_recv_msg(void* data, int length);



#endif //__UTILS_H__