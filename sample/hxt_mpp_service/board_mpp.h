#ifndef __board_mpp_h__
#define __board_mpp_h__


#ifdef __cplusplus
extern "C" {
#endif


#define utils_print(format, ...) printf("%s>>>%d: " format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
// #define malloc_print(__ptr__,size) printf("[ALLOC] %32s:%4d | addr= %p, size= %lu, expr= `%s`\n", __FUNCTION__, __LINE__ , __ptr__, size, #size)
// #define free_print(ptr)	printf("[ FREE] %32s:%4d | addr= %p, expr= `%s`\n", __FUNCTION__, __LINE__, ptr, #ptr)

// #define malloc_print(__ptr__,size)
// #define free_print(ptr)
// #define utils_print(format, ...)


#define utils_malloc(size) ({ \
	void *__ptr__ = malloc(size); \
	memset(__ptr__, 0, size); \
	__ptr__; \
	})

#define utils_calloc(size) ({ \
	void *__ptr__ = calloc(size, 1); \
	__ptr__; \
	})

#define utils_free(ptr) ({ \
	free(ptr); \
	})

typedef enum
{
    TRUE  = 1, 
    FALSE  = 0
}BOOL;

#define MP3_FIFO                "/tmp/mp3_fifo"
#define PCM_FIFO                "/tmp/pcm_fifo"



// BOOL start_vi();
// void stop_vi();
// BOOL start_vpss(int width, int height);
// void stop_all_vpss();
// BOOL start_venc(int width, int height);
// void stop_all_venc();
// BOOL bind_vi_vpss();
// void unbind_vi_vpss();
// BOOL bind_vpss_venc();
// void unbind_vpss_venc();

BOOL create_pcm_fifo();
void delete_pcm_fifo();
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



#ifdef __cplusplus
}
#endif


#endif // !__board_mpp_h__