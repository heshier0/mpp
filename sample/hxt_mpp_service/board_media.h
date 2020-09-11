#ifndef __board_media_h__
#define __board_media_h__

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif



BOOL board_create_mp4_file(const char* filename);
void board_close_mp4_file();
BOOL board_write_mp4(void* input_stream, int width, int height);
void board_delete_current_mp4_file();


#ifdef __cplusplus
}
#endif
#endif // !__board_media_h__