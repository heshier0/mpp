#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <uwsc/uwsc.h>

#include "common.h"
#include "utils.h"
#include "databuffer.h"

#include "iflyos_defines.h"

#define BUFFER_SIZE     (4*1024*1024)
#define PCM_LENGTH      640

extern BOOL g_iflyos_wbsc_running;

DATABUFFER g_voice_buffer;
static BOOL g_sampling = FALSE;
BOOL g_stop_capture = FALSE;

static pthread_t read_pcm_tid;
static pthread_t send_pcm_tid;

static void* thread_read_pcm_cb(void *data)
{
    char pcm_buf[PCM_LENGTH] = {0};
    memset(pcm_buf, 0, PCM_LENGTH);
        
    int fd = open(PCM_FIFO, O_RDONLY);
    if (-1 == fd)
    {
        utils_print("open PCM_FIFO error\n");
        return NULL;
    }   

    while(g_sampling)
    {
        int read_count = read(fd, pcm_buf, PCM_LENGTH);
        if( read_count <= 0)
        {
            continue;
        }
        char *ptr = get_free_buffer(&g_voice_buffer, read_count);
        if(NULL == ptr)
        {
            memset(pcm_buf, 0, PCM_LENGTH);
            continue;
        }
        memcpy(ptr, pcm_buf, read_count);
        memset(pcm_buf, 0, PCM_LENGTH);
        use_free_buffer(&g_voice_buffer, read_count);
    }

    close(fd);
    utils_print("read pcm thread exit...\n");

    return NULL;
}

static void* thread_send_pcm_cb(void *data)
{
    if (NULL == data)
    {
        return NULL;
    }    

    BOOL requested = FALSE;
    struct uwsc_client *cl = (struct uwsc_client *)data;

    while(g_sampling)
    {
        if (g_stop_capture)
        {
            utils_print("To send END flag !!!!!\n");
            cl->send(cl, "__END__", strlen("__END__"), UWSC_OP_BINARY);
            g_stop_capture = FALSE;
            requested = FALSE;
        }

        char* ptr = get_buffer(&g_voice_buffer, PCM_LENGTH);
        if(NULL == ptr)
        {
            continue;
        }
        // send data
        iflyos_write_audio(ptr, PCM_LENGTH);
        release_buffer(&g_voice_buffer, PCM_LENGTH);
    }
    utils_print("send pcm thread exit...\n");
    
    return NULL;
}

static void iflyos_uwsc_onopen(struct uwsc_client *cl)
{
    utils_print("iflyos onopen\n");

    g_iflyos_wbsc_running = TRUE;

    // int sock_fd = create_udp_server();
    /* sample voice to iflyos */
    g_sampling = TRUE;

    pthread_create(&read_pcm_tid, NULL, thread_read_pcm_cb, NULL);
    pthread_create(&send_pcm_tid, NULL, thread_send_pcm_cb, (void*)cl);

    /* send cmd to tell service */
    send_voice_sample_start_cmd();
}

static void iflyos_uwsc_onmessage(struct uwsc_client *cl,
	void *data, size_t len, bool binary)
{
    utils_print("%s: [%.*s]\n", utils_get_current_time(), (int)len, (char *)data);
    
    if (binary) {
        //文件
    } 
    else 
    {
        char* name = iflyos_get_response_name(data);
        if (NULL == name)
        {
            return;
        }
        
        if(strcmp(name, aplayer_audio_out) == 0)
        {
            iflyos_play_response_audio(data);
        }
        else if (strcmp(name, recog_stop_capture) == 0)
        {
            g_stop_capture = TRUE;
        }
        else
        {
            /**/
        }
        
        free(name);
    }
}

static void iflyos_uwsc_onerror(struct uwsc_client *cl, int err, const char *msg)
{
    utils_print("iflyos onerror:%d: %s\n", err, msg);
    if (g_sampling)
    {
        g_sampling = FALSE;
        send_voice_sample_stop_cmd();

        pthread_join(read_pcm_tid, NULL);
        pthread_join(send_pcm_tid, NULL);
    }

    ev_break(cl->loop, EVBREAK_ALL);
}

static void iflyos_uwsc_onclose(struct uwsc_client *cl, int code, const char *reason)
{
    utils_print("iflyos onclose:%d: %s\n", code, reason);
    if (g_sampling)
    {
        g_sampling = FALSE;
        send_voice_sample_stop_cmd();

        pthread_join(read_pcm_tid, NULL);
        pthread_join(send_pcm_tid, NULL);
    }

    ev_break(cl->loop, EVBREAK_ALL);
}

int iflyos_websocket_start()
{
	int ping_interval = -1;	/* second */
    struct uwsc_client *cl;
    struct ev_loop *loop;

    create_buffer(&g_voice_buffer, BUFFER_SIZE);
    iflyos_load_cfg();

    char ifly_url[255] = {0};
    char* device_id = iflyos_get_device_id();//hxt_get_desk_uuid_cfg();
    char* token = iflyos_get_token();//hxt_get_iflyos_token_cfg();
    utils_print("iflyos device_id is %s and token is %s\n", device_id, token);
    sprintf(ifly_url, "wss://ivs.iflyos.cn/embedded/v1?token=%s&device_id=%s", token, device_id); 

    loop = ev_loop_new(EVFLAG_AUTO);
    cl = uwsc_new(loop, ifly_url, ping_interval, NULL);
    if (!cl)
    {
        utils_print("iflyos websocket client init failed...\n");
        return -1;
    }
    
    iflyos_init_cae_lib((void*)cl);

	utils_print("iflyos connect...\n");

    cl->onopen = iflyos_uwsc_onopen;
    cl->onmessage = iflyos_uwsc_onmessage;
    cl->onerror = iflyos_uwsc_onerror;
    cl->onclose = iflyos_uwsc_onclose;
   
    ev_run(loop, 0);
    utils_print("iflyos process exit....\n");

    g_iflyos_wbsc_running = FALSE;
    iflyos_deinit_cae_lib();
    free(cl);
    iflyos_unload_cfg();
    destroy_buffer(&g_voice_buffer);
    
    
   

    return 0;
}

