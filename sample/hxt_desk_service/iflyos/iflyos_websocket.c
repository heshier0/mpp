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

#include <hi_common.h>
#include <sample_comm.h>

#include "common.h"
#include "utils.h"
#include "databuffer.h"
#include "db.h"
#include "server_comm.h"
#include "iflyos_func.h"

#define BUFFER_SIZE     (4*1024*1024)
#define PCM_LENGTH      640*4

extern BOOL g_hxt_wbsc_running;
extern BOOL g_iflyos_wbsc_running;
extern BOOL g_iflyos_first_login;

static BOOL g_sampling = FALSE;
volatile BOOL g_stop_capture = FALSE; 

static struct uwsc_client *iflyos_wsc = NULL;
static struct ev_loop *g_iflyos_loop = NULL; 

static void* thread_read_pcm_cb(void *data)
{
    char pcm_buf[PCM_LENGTH] = {0};
           
    prctl(PR_SET_NAME, "read_pcm_cb");
    pthread_detach(pthread_self());

    BOOL requested = FALSE;
    struct uwsc_client *cl = (struct uwsc_client *)data;

    /*create udp server to recv data*/
    int sockfd = -1;
    struct sockaddr_in local_addr, remote_addr;
    bzero(&local_addr, sizeof(local_addr));
    bzero(&remote_addr, sizeof(remote_addr));
    int addr_len = sizeof(struct sockaddr_in);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        utils_print("craete pcm recv server failed\n");
        return NULL;
    }

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)     
    {    
        utils_print("reuse addr PCM_RECV_PORT failed\n");
        return NULL;  
    }

    bzero(&local_addr, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(PCM_RECV_PORT);
    local_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if ((bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr))) < 0)
    {
        utils_print("bind error\n");
        return NULL;
    }

    while(g_sampling)
    {
        if (g_stop_capture)
        {
            utils_print("To send END flag...\n");
            if (cl != NULL)
            {
                cl->send(cl, "__END__", strlen("__END__"), UWSC_OP_BINARY);
            }
            utils_print("OK!!!\n");
            g_stop_capture = FALSE;
            requested = FALSE;
        }

        memset(pcm_buf, 0, PCM_LENGTH);
        int read_count = recvfrom(sockfd, pcm_buf, PCM_LENGTH, 0, (struct sockaddr*)&remote_addr, &addr_len);
        if (read_count <= 0)
        {
            usleep(500 * 1000);
            continue;
        }

        iflyos_write_audio(pcm_buf, PCM_LENGTH);
    }

    close(sockfd);
    utils_print("read pcm thread exit...\n");

    return NULL;
}

static void iflyos_uwsc_onopen(struct uwsc_client *cl)
{
    utils_print("iflyos onopen\n");

    // g_iflyos_wbsc_running = TRUE;

    /* sample voice to iflyos */
    g_sampling = TRUE;

    /* send cmd to tell service */
    send_voice_sample_start_cmd();

    pthread_t read_pcm_tid;
    pthread_create(&read_pcm_tid, NULL, thread_read_pcm_cb, (void*)cl);

    /* */
    if (g_iflyos_first_login)
    {
        utils_send_local_voice(VOICE_IFLYOS_READY);
        g_iflyos_first_login = FALSE;
    }
}

static void iflyos_uwsc_onmessage(struct uwsc_client *cl,
	void *data, size_t len, bool binary)
{
    utils_print("iflyos recv:\n");
    utils_print("%s: %.*s\n", utils_get_current_time(), (int)len, (char *)data);
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
}

static void iflyos_uwsc_onerror(struct uwsc_client *cl, int err, const char *msg)
{
    utils_print("iflyos onerror:%d: %s\n", err, msg);
    send_voice_sample_stop_cmd();
    g_sampling = FALSE;

    ev_break(cl->loop, EVBREAK_ALL);
}

static void iflyos_uwsc_onclose(struct uwsc_client *cl, int code, const char *reason)
{
    utils_print("iflyos onclose:%d: %s\n", code, reason);
    send_voice_sample_stop_cmd();
    g_sampling = FALSE;

    ev_break(cl->loop, EVBREAK_ALL);
}

void iflyos_websocket_stop()
{
    if (!g_iflyos_wbsc_running)
    {
        return;
        
    }
    send_voice_sample_stop_cmd();
    g_sampling = FALSE; 
    
    // char buf[128] = "";
    // iflyos_wsc->send(iflyos_wsc, buf, strlen(buf + 2) + 2, UWSC_OP_CLOSE);   
    if (g_iflyos_loop)
    {
        utils_print("iflyos weboscket break\n");
        ev_break(g_iflyos_loop, EVBREAK_ALL);
    }

    return; 
}

static void handle_signal(int signo)
{
    if (SIGPIPE == signo)
    {
        utils_print("PCM_FIFO break\n");
    }
}

int iflyos_websocket_start()
{
	int ping_interval = -1;	/* second */
    // struct uwsc_client *wsc;
    // struct ev_loop *loop;

    signal(SIGPIPE, handle_signal);

    prctl(PR_SET_NAME, "iflyos_websocket");
    pthread_detach(pthread_self());

    char ifly_url[255] = {0};

    char* device_id = get_device_id();
    char* token = get_iflyos_token();
    utils_print("iflyos device_id is %s and token is %s\n", device_id, token);
    sprintf(ifly_url, "wss://ivs.iflyos.cn/embedded/v1?token=%s&device_id=%s", token, device_id); 
    utils_free(token);
    utils_free(device_id);

    g_iflyos_loop = ev_loop_new(EVFLAG_AUTO);
    iflyos_wsc = uwsc_new(g_iflyos_loop, ifly_url, ping_interval, NULL);
    if (!iflyos_wsc)
    {
        utils_print("iflyos websocket client init failed...\n");
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
   
    ev_run(g_iflyos_loop, 0);

 IFLYOS_EXIT:   
    iflyos_deinit_cae_lib();
    utils_free(iflyos_wsc);
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
    pthread_t iflyos_tid;
    
    if (g_iflyos_wbsc_running)
    {
        return;
    }

    g_iflyos_wbsc_running = TRUE;

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

