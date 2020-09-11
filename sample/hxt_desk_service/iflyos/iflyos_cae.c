#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#include <uwsc/uwsc.h>

#include "utils.h"
#include "hlw.h"



#define CONF_PATH   "/userdata/config/iflyos/hlw.ini"
#define PARAM_PATH  "/userdata/config/iflyos/hlw.param"

extern BOOL g_stop_capture;

CAE_HANDLE cae_handle = NULL;
FILE *frecord = NULL;
FILE *frecog = NULL;
FILE *fivw = NULL;
FILE *flog = NULL;

static BOOL wake_up = FALSE;

static void ivw_fn(short angle, short channel, float power, short CMScore, short beam, char *param1, void *param2, void *userdata)
{
    utils_print("angle=%d, channel=%d, power=%f, CMScore=%d, beam=%d, param1=%s\n",
                    angle, channel, power, CMScore, beam, param1);

    if (flog != NULL)
    {
        fprintf(flog, "ivw_fn: angle=%d, channel=%d, power=%f, CMScore=%d, beam=%d, param1=%s\n",
                    angle, channel, power, CMScore, beam, param1);
        fflush(flog);
    }
	wake_up = TRUE;
}

static void ivw_audio_fn(const void* audio_data, unsigned int audio_len, int param1, const void* param2, void *userdata)
{
    // if (fivw != NULL)
    // {
    //     fwrite(audio_data, 1, audio_len, fivw);
    //     fflush(fivw);
    // }

	struct uwsc_client *cl = (struct uwsc_client *)userdata;
	if (wake_up)
	{
		char *req = iflyos_create_audio_in_request();
		cl->send(cl, req, strlen(req), UWSC_OP_TEXT);
		free(req);
		wake_up = FALSE;
	}

	if(!g_stop_capture)
	{
		cl->send(cl, audio_data, audio_len, UWSC_OP_BINARY);
	}
}

static void recog_audio_fn(const void* audio_data, unsigned int audio_len, int param1, const void *param2, void *userdata)
{
    // if (frecog != NULL)
    // {
    //     fwrite(audio_data, 1, audio_len, frecog);
    //     fflush(frecog);
    // }
}

int iflyos_init_cae_lib(void* data)
{
    const char* ivw_path = "ivw.pcm";
    const char* recog_path = "recog.pcm";
    const char* log_path = "log.txt";

    int rv, i;
    char buffer[1024] = {0};
    int flag = 1;

    utils_print("version: %s\n", CAEGetVersion());
#ifdef DEBUG
    CAESetShowLog(flag);
#endif

    rv = CAENew(&cae_handle, CONF_PATH, ivw_fn, ivw_audio_fn, recog_audio_fn, PARAM_PATH, data);
    if (rv != 0)
    {
        return -1;
    }

    char *sn = hxt_get_iflyos_cae_sn();
    if (CAEAuth(sn))
    {
        utils_print("CAE auth error\n");
    }

	frecog = fopen(recog_path, "wb");
	if (frecog == NULL) 
    {
		utils_print("open recog pcm error\n");
		return -1;
	}

	fivw = fopen(ivw_path, "wb");
	if (fivw == NULL) 
    {
		utils_print("open ivw pcm error\n");
		return -1;
	}

	flog = fopen(log_path, "w");
	if (flog == NULL) 
    {
		utils_print("open ivw log error\n");
		return -1;
	}

    return 0;
}

void iflyos_deinit_cae_lib()
{
    if (cae_handle != NULL)
    {
        CAEDestroy(cae_handle);
    }

    if(frecog != NULL)
    {
        fclose(frecog);
    }

    if(fivw != NULL)
    {
        fclose(fivw);
    }

    if(flog != NULL)
    {
        fclose(flog);
    }
}

int iflyos_write_audio(void* buffer, int buf_length)
{
    int rv;
	if (NULL == buffer)
	{
		return -1;
	}

    rv = CAEAudioWrite(cae_handle, buffer, buf_length);
    if(rv != 0)
    {
        utils_print("cae write error: %d\n", rv);
        return rv;
    }

    return rv;
}

#ifndef DEBUG
#define BUFFER_SIZE (256 * 4)

void test_ivw_fn(short angle, short channel, float power, short CMScore, short beam, char *param1, void *param2, void *userData)
{
	printf("test_ivw_fn angle = %d, channel = %d, power = %f, CMScore = %d, beam = %d, param1 = %s\n", 
		angle, channel, power, CMScore, beam, param1);
	//内部已经设置
	//CAESetRealBeam(handle, beam);
	if (flog != NULL) {
		fprintf(flog, "test_ivw_fn angle = %d, channel = %d, power = %f, CMScore = %d, beam = %d, param1 = %s\n", 
			angle, channel, power, CMScore, beam, param1);
		fflush(flog);
	}
}

void test_ivw_audio_fn(const void *audioData, unsigned int audioLen, int param1, const void *param2, void *userData)
{
	//printf("test_ivw_audio_fn audioData = %p, audioLen = %d\n", audioData, audioLen);
	if (fivw != NULL) {
		fwrite(audioData, 1, audioLen, fivw);
		fflush(fivw);
	}
}

void test_recog_audio_fn(const void *audioData, unsigned int audioLen, int param1, const void *param2, void *userData)
{
	//printf("test_audio_fn audioData = %p, audioLen = %d\n", audioData, audioLen);
	if (frecog != NULL) {
		fwrite(audioData, 1, audioLen, frecog);
		fflush(frecog);
	}
}
void iflyos_cae_test()
{
    const char* conf_path			= "/userdata/config/iflyos/hlw.ini";
	const char* param_path			= "/userdata/config/iflyos/hlw.param";
	const char* rec_path			= "rec.pcm";
	const char* ivw_path			= "ivw.pcm";
	const char* recog_path			= "recog.pcm";
	const char* log_path			= "log.pcm";

	int rv, i;
	char buffer[BUFFER_SIZE];
	int frame = 0;
	int nread = 0;
	int flag = 0;

	// CAESetShowLog(0);  //0 调试  1信息   2错误

	fprintf(stderr, "version : %s\n", CAEGetVersion());
	
	CAESetShowLog(flag);

	rv = CAENew(&cae_handle, conf_path, test_ivw_fn, test_ivw_audio_fn, test_recog_audio_fn, param_path,  NULL);
	if (rv != 0) {
		return -1;
	}

	if (CAEAuth("abadba2f-4ef4-4a8d-9d46-61557dcd52bb")) {
		fprintf(stderr, "auth error!!!!!!!\n");
	} else {
		fprintf(stderr, "auth ok!!!!!!!\n");
	}
	
	frecord = fopen(rec_path, "rb");
	if (frecord == NULL) {
		fprintf(stderr, "open record pcm error");
		return -1;
	}
	frecog = fopen(recog_path, "wb");
	if (frecog == NULL) {
		fprintf(stderr, "open recog pcm error");
		return -1;
	}
	fivw = fopen(ivw_path, "wb");
	if (fivw == NULL) {
		fprintf(stderr, "open ivw pcm error");
		return -1;
	}
	flog = fopen(log_path, "w");
	if (flog == NULL) {
		fprintf(stderr, "open ivw log error");
		return -1;
	}


	fprintf(stderr, "write start...");
	frame = 0;
	for (;;){
		nread = fread(buffer, 1, BUFFER_SIZE, frecord);
		if (nread != BUFFER_SIZE){
			fprintf(stderr, "read eof");
			break;
		}
			
		rv = CAEAudioWrite(cae_handle, buffer, nread);
		if (rv != 0) {
			fprintf(stderr, "cae write error");
			exit(1);
		}

		++frame;
		if (frame % 1000 == 0) {
			fprintf(stderr, "frame = %d\n", frame);
		}
	}

	fprintf(stderr, "write end...");

	CAEDestroy(cae_handle);

	if (frecord != NULL) fclose(frecord);
	if (frecog != NULL) fclose(frecog);
	if (fivw != NULL) fclose(fivw);
	if (flog != NULL) fclose(flog);

	return 0;
}
#endif