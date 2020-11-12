#ifndef __board_mpp_h__
#define __board_mpp_h__

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MP3_FIFO                "/tmp/mp3_fifo"
#define MPP_DATA_PORT           10099
#define PCM_DATA_PORT           10098

BOOL create_mp3_fifo();
void delete_mp3_fifo();

BOOL rotate_picture();

BOOL start_video_system(int width, int height);
void stop_video_system();

BOOL start_audio_system();
void stop_audio_system();

BOOL init_mpp();
void deinit_mpp();

void start_play_mp3_thread();
void stop_play_mp3_thread();

void start_sample_video_thread(void* data);
void stop_sample_video_thread();

void start_sample_voice_thread();
void stop_sample_voice_thread();

void board_get_snap_from_venc_chn(const char* jpg_file);

void start_video_recording(const char* filename);
void stop_video_recording();
void delete_video();

#ifdef __cplusplus
}
#endif


#endif // !__board_mpp_h__