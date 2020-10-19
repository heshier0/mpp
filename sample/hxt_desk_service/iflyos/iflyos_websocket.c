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
#include <sys/prctl.h>

#include <uwsc/uwsc.h>

#include "common.h"
#include "utils.h"
#include "databuffer.h"
#include "db.h"
#include "server_comm.h"
#include "iflyos_func.h"

#define BUFFER_SIZE     (4*1024*1024)
#define PCM_LENGTH      640

extern BOOL g_hxt_wbsc_running;
extern BOOL g_iflyos_wbsc_running;
// extern BOOL g_iflyos_first_login;

DATABUFFER g_voice_buffer; 
static BOOL g_sampling = FALSE;
volatile BOOL g_stop_capture = FALSE; 

static pthread_t read_pcm_tid;
static pthread_t send_pcm_tid;
struct uwsc_client *iflyos_wsc;

static void* thread_read_pcm_cb(void *data)
{
    char pcm_buf[PCM_LENGTH] = {0};
    memset(pcm_buf, 0, PCM_LENGTH);
        
    prctl(PR_SET_NAME, "read_pcm_cb");
    pthread_detach(pthread_self());

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

    prctl(PR_SET_NAME, "send_pcm_cb");
    pthread_detach(pthread_self());

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

    /* sample voice to iflyos */
    g_sampling = TRUE;

    pthread_create(&read_pcm_tid, NULL, thread_read_pcm_cb, NULL);
    pthread_create(&send_pcm_tid, NULL, thread_send_pcm_cb, (void*)cl);

    /* send cmd to tell service */
    send_voice_sample_start_cmd();

    /* */
    // if (g_iflyos_first_login)
    // {
        utils_send_local_voice(VOICE_IFLYOS_READY);
    //     g_iflyos_first_login = FALSE;
    // }
}

static void iflyos_uwsc_onmessage(struct uwsc_client *cl,
	void *data, size_t len, bool binary)
{
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
            utils_print("To play audio....\n");
            iflyos_play_response_audio(data);
        }
        else if (strcmp(name, recog_stop_capture) == 0)
        {
            utils_print("Stop capture ....\n");
            g_stop_capture = TRUE;
        }
        else
        {
            utils_print("other response\n");
        }
        
        utils_free(name);
    }

    utils_print("%s: %.*s\n", utils_get_current_time(), (int)len, (char *)data);
}

static void iflyos_uwsc_onerror(struct uwsc_client *cl, int err, const char *msg)
{
    utils_print("iflyos onerror:%d: %s\n", err, msg);
    if (g_sampling)
    {
        g_sampling = FALSE;
        send_voice_sample_stop_cmd();
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
    }

    ev_break(cl->loop, EVBREAK_ALL);
}

void iflyos_websocket_stop()
{
    utils_print("To stop iflyos websocket\n");
    char buf[128] = "";
    iflyos_wsc->send(iflyos_wsc, buf, strlen(buf + 2) + 2, UWSC_OP_CLOSE);
}

int iflyos_websocket_start()
{
	int ping_interval = -1;	/* second */
    // struct uwsc_client *wsc;
    struct ev_loop *loop;

    prctl(PR_SET_NAME, "iflyos_websocket");
    pthread_detach(pthread_self());

    create_buffer(&g_voice_buffer, BUFFER_SIZE);

    char ifly_url[255] = {0};

    char* device_id = get_device_id();
    char* token = get_iflyos_token();
    utils_print("iflyos device_id is %s and token is %s\n", device_id, token);
    sprintf(ifly_url, "wss://ivs.iflyos.cn/embedded/v1?token=%s&device_id=%s", token, device_id); 
    utils_free(token);
    utils_free(device_id);

    loop = ev_loop_new(EVFLAG_AUTO);
    iflyos_wsc = uwsc_new(loop, ifly_url, ping_interval, NULL);
    if (!iflyos_wsc)
    {
        utils_print("iflyos websocket client init failed...\n");
        destroy_buffer(&g_voice_buffer);
        return -1;
    }
    
    if (!iflyos_init_cae_lib((void*)iflyos_wsc))
    {
        utils_print("init cae lib error\n");
        goto IFLYOS_EXIT;
    }

	utils_print("iflyos connect...\n");

    iflyos_wsc->onopen = iflyos_uwsc_onopen;
    iflyos_wsc->onmessage = iflyos_uwsc_onmessage;
    iflyos_wsc->onerror = iflyos_uwsc_onerror;
    iflyos_wsc->onclose = iflyos_uwsc_onclose;
   
    ev_run(loop, 0);
    
    // pthread_join(&read_pcm_tid, NULL);
    // pthread_join(&send_pcm_tid, NULL);
 IFLYOS_EXIT:   
    iflyos_deinit_cae_lib();
    utils_free(iflyos_wsc);
    destroy_buffer(&g_voice_buffer);
    g_iflyos_wbsc_running = FALSE;
    utils_print("iflyos process exit....\n");

    return 0;
}


static void* iflyos_websocket_cb(void* data)
{
    iflyos_websocket_start();
    return NULL;
}

void start_iflyos_websocket_thread()
{
    if (!g_hxt_wbsc_running)
    {
        return;
    }

    pthread_t iflyos_tid;
    char *token = get_iflyos_token();
    char *sn = get_iflyos_sn();
    if (token != NULL && sn != NULL)
    {
        pthread_create(&iflyos_tid, NULL, iflyos_websocket_cb, NULL);

        utils_free(token);
        utils_free(sn);
    }

    return;
}

