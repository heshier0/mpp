#ifndef __UTILS_H__
#define __UTILS_H__

#include <cJSON.h>

#ifdef DEBUG
#define utils_print(format, ...) printf("%d >>> %s " format "\n", __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define malloc_print(__ptr__,size) printf("[ALLOC] %32s:%4d | addr= %p, size= %lu, expr= `%s`\n", __FUNCTION__, __LINE__ , __ptr__, size, #size)
#define free_print(ptr)	printf("[ FREE] %32s:%4d | addr= %p, expr= `%s`\n", __FUNCTION__, __LINE__, ptr, #ptr)
#else
#define utils_print(format, ...)
#define malloc_print(__ptr__,size)
#define free_print(ptr)
#endif 

#define utils_malloc(size) ({ \
	void *__ptr__ = malloc(size); \
	memset(__ptr__, 0, size); \
	malloc_print(__ptr__,size); \
	__ptr__; \
	})

#define utils_calloc(size) ({ \
	void *__ptr__ = calloc(size, 1); \
	malloc_print(__ptr__, size); \
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

typedef struct post_memory_cb 
{
    char *memory;
    size_t size;
}PostMemCb;

void init_plugins();
void deinit_plugins();

cJSON* utils_load_cfg(const char* cfg);
void utils_unload_cfg(cJSON* root);
BOOL utils_reload_cfg(const char* cfg, cJSON* root);

char* utils_get_cfg_str_value(cJSON* root, const char* params_item, const char* prop_item);
double utils_get_cfg_number_value(cJSON* root, const char* params_item, const char* prop_item);
BOOL utils_set_cfg_str_value(cJSON* root, const char* cfg, const char* params_item, const char* prop_item, const char* value);
BOOL utils_set_cfg_number_value(cJSON* root, const char* cfg, const char* params_item, const char* prop_item, const double value);

BOOL utils_send_mp3_voice(const char *url);
BOOL utils_download_file(const char *url, char *out_buffer, int buffer_length);
BOOL utils_upload_file(const char* url, const char* header, const char* local_file_path, char* out_buffer, int buffer_length);
BOOL utils_post_json_data(const char *url, const char* header_content, const char* json_data, char* out, int out_length);
char* utils_get_response_value(const char* json_data, const char* root_name, const char* item_name, const char* sub_name, const char* last_node);
unsigned long utils_get_file_size(const char* file_name);

/*use linux shell cmd and linux pipe to achieve*/
//display format "yyyy-mm-dd"
char* utils_date_to_string();
//display format "yyyy-mm-dd HH:MM:SS" 
char* utils_time_to_string();
char* utils_get_file_md5sum(const char* file_name);
int utils_split_file_to_chunk(const char* path);
unsigned long utils_get_file_size(const char* path);

#endif //__UTILS_H__