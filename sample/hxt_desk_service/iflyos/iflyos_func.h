#ifndef __ILFYOS_FUNC_H__
#define __ILFYOS_FUNC_H__

#include "iflyos_common_def.h"
#include "iflyos_request_def.h"
#include "iflyos_response_def.h"

/* cae module */
int iflyos_init_cae_lib(void* data);
int iflyos_init_cae_lib(void* data);
int iflyos_write_audio(void* buffer, int buf_length);

/* request */
char* iflyos_create_audio_in_request();
char* iflyos_create_txt_in_request(const char* txt_buffer);

/* response */
char* iflyos_get_response_name(const char* json_data);
void iflyos_play_response_audio(void *data);



#endif //  __ILFYOS_FUNC_H__ 