#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <sys/file.h>
#include <fcntl.h>
#include <sys/prctl.h>

#include "databuffer.h"
#include "board_mpp.h"
#include "command.h"

#define HXT_MPP_SERVICE_VERSION ("1.0.1")
#define MPP_SERVICE_PORT        (10086)
#define BUFFER_SIZE             (5*1024*1024)

int g_last_sock = -1;
DATABUFFER g_client_data;

static BOOL posturing = FALSE;
static BOOL sampling = FALSE;
static BOOL client_running = FALSE;

static int create_local_tcp_server()
{
    int reuse = 1;
    struct sockaddr_in local;
    
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if( setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
    {
        utils_print("setsockopt failed\n");
        return -1;
    }

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(MPP_SERVICE_PORT);
    local.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(bind(sock_fd, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
        utils_print("bind local port failed: %s\n", strerror(errno));
        close(sock_fd);
        return -1;
    }

    if (listen(sock_fd, 10) < 0)
    {
        utils_print("listen failed: %s\n", strerror(errno));
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

static void* receive_client_data_thread(void *args)
{
    if(NULL == args)
    {
        return NULL;
    }

    char buf[512] = {0};
    int new_sock = (int)args;
    if (-1 == new_sock)
    {
        return NULL;
    }
    
    prctl(PR_SET_NAME, "read_client_data");
    pthread_detach(pthread_self());

    while(client_running)
    {   
        memset(buf, 0, 256);
        int read_count = read(new_sock, buf, sizeof(buf)-1);
        if(read_count > 0)
        {
            buf[read_count] = '\0';
            char *ptr = get_free_buffer(&g_client_data, read_count);
            if(NULL == ptr)
            {
                memset(buf, 0, 256);
                continue;
            }
            memcpy(ptr, buf, read_count);
            use_free_buffer(&g_client_data, read_count);          
        }
        else
        {
            if (posturing)
            {
                stop_sample_video_thread();
                posturing = FALSE;
                //???
                stop_video_recording();
                delete_video();
            }

            if (sampling)
            {
                sampling = FALSE;
                stop_sample_voice_thread();
            }
            client_running = FALSE;
            
            break;
        }
    }
    close(new_sock);

    utils_print("read_client_data thread exit....\n");

    return NULL;
}

static void* process_desk_business_thread(void *args)
{
    cmd_header_t header;
    int len = sizeof(cmd_header_t);
    video_ratio_t video_ratio;
    study_video_t study_video;
    BOOL saving_video = FALSE;

    prctl(PR_SET_NAME, "process_cmd");
    pthread_detach(pthread_self());
    
    while(client_running)
    {
        bzero(&header, len);
        char* ptr = get_buffer(&g_client_data, len);
        if(NULL == ptr)
        {
            sleep(1);
            continue;
        }
        memcpy(&header, ptr, len);
        release_buffer(&g_client_data, len);
        ptr = NULL;
        
        if(header.flag != CMD_FLAG)
        {
            continue;
        }

        short cmd = header.cmd;
        utils_print("cmd is %d\n", cmd);
        switch (cmd)
        {
        case CMD_SETUP_VIDEO_RATIO:
            stop_video_system();
            bzero(&video_ratio, sizeof(video_ratio_t));
            ptr = get_buffer(&g_client_data, sizeof(video_ratio_t));
            memcpy(&video_ratio, ptr, sizeof(video_ratio_t));
            release_buffer(&g_client_data, sizeof(video_ratio_t));
            start_video_system(video_ratio.width, video_ratio.height);
            utils_print("reset video system w:%d h:%d\n", video_ratio.width, video_ratio.height);
        break;    
        case CMD_START_POSTURE:
            if(!posturing)
            {
                bzero(&video_ratio, sizeof(video_ratio_t));
                ptr = get_buffer(&g_client_data, sizeof(video_ratio_t));
                if (ptr != NULL)
                {
                    memcpy(&video_ratio, ptr, sizeof(video_ratio_t));
                    release_buffer(&g_client_data, sizeof(video_ratio_t));
                    start_sample_video_thread((void*)&video_ratio);
                    utils_print("start posture check\n");
                }
                posturing = TRUE;
            }
        break;      
        case CMD_STOP_POSTURE:
            if (posturing)
            {
                stop_sample_video_thread();
                posturing = FALSE;
                utils_print("stop posture check\n");
            }
        break;
        case CMD_START_VOICE_SAMPLE:
            if(!sampling)
            {
                sampling = TRUE;
                start_sample_voice_thread();
                utils_print("start voice sample\n");
            }
        break;
        case CMD_STOP_VOICE_SAMPLE:
            if (sampling)
            {
                sampling = FALSE;
                stop_sample_voice_thread();
                utils_print("stop voice sample\n");
            }
        break;      
        case CMD_START_VIDEO_RECORD:
            {
                bzero(&study_video, sizeof(study_video_t));
                ptr = get_buffer(&g_client_data, sizeof(study_video_t));
                memcpy(&study_video, ptr, sizeof(study_video_t));
                release_buffer(&g_client_data, sizeof(study_video_t));
                utils_print("video is %s, snap is %s\n", study_video.video_name, study_video.snap_name);
                start_video_recording(study_video.video_name);
                board_get_snap_from_venc_chn(study_video.snap_name);
            }
        break;
        case CMD_STOP_VIDEO_RECORD:
            {
                saving_video = TRUE;
                bzero(&study_video, sizeof(study_video_t));
                ptr = get_buffer(&g_client_data, sizeof(study_video_t));
                memcpy(&study_video, ptr, sizeof(study_video_t));
                release_buffer(&g_client_data, sizeof(study_video_t));
                stop_video_recording();
                saving_video = FALSE;
                utils_print("stop video....\n");
            }
        break;
        case CMD_DEL_VIDEO_FILE:
        {
            bzero(&study_video, sizeof(study_video_t));
            ptr = get_buffer(&g_client_data, sizeof(study_video_t));
            memcpy(&study_video, ptr, sizeof(study_video_t));
            release_buffer(&g_client_data, sizeof(study_video_t));
            delete_video();
            remove(study_video.snap_name);
            utils_print("delete snap %s\n", study_video.snap_name);
        }
        break;    
        default:
        break;
        }
    }
    utils_print("process_cmd thread exit....\n");
    return NULL;
}

static void start_receive_client_data(int sock_fd)
{
    pthread_t tid;
    pthread_create(&tid, NULL, receive_client_data_thread, (void*)sock_fd);
}

static void start_process_desk_business()
{
    pthread_t tid; 
    pthread_create(&tid, NULL, process_desk_business_thread, NULL);
}

static void handle_signal(int signo)
{
    utils_print("MPP SIGNAL break!!!!\n");
    client_running = FALSE;
    if (posturing)
    {
        stop_sample_video_thread();
        posturing = FALSE;
        //???
        stop_video_recording();
        delete_video();
    }

    if (sampling)
    {
        sampling = FALSE;
        stop_sample_voice_thread();
    }
}

int main(int argc, char **argv)
{
    int client_fd = -1;
    struct sockaddr_in client_addr;

    utils_print("%s\n", HXT_MPP_SERVICE_VERSION);

    int lock_fd = open("/tmp/.single.lock", O_RDWR | O_CREAT, 0666);
    if(lock_fd < 1)
    {
        utils_print("open lock failed\n");
        return -1;
    }
    int lock_err = flock(lock_fd, LOCK_EX | LOCK_NB);
    if(lock_err == -1)
    {
        utils_print("lock failed\n");
        return -2;
    }

    signal(SIGPIPE, handle_signal);
    //FIXME: add by huang  @2020-11-10  不能生成 core 先注释掉  19941608848
	//signal(SIGSEGV, handle_signal);

    socklen_t len = sizeof(len);
    int sock = create_local_tcp_server();
    if(sock == -1)
    {
        return -1;
    }
   
    create_buffer(&g_client_data, BUFFER_SIZE);

    create_pcm_fifo();
    create_mp3_fifo();

    if(!init_mpp())
    {
        utils_print("init mpp subsystem error.\n");
        goto ERR_INIT_MPP;
    }
    if (!start_video_system(640, 360))
    {
        utils_print("init video system error.\n");
        goto ERR_INIT_VIDEO_SYS;
    }
    /* 系统起来就需要播放mp3 */
    start_play_mp3_thread();

    /* to process hxt_desk_service */
    while(1)
    {
        utils_print("Wait to recv info from other process...\n");
        client_fd = accept(sock, (struct sockaddr *)&client_addr, &len);
        if(client_fd < 0)
        {
            utils_print("accept new client failed: %s\n", strerror(errno));
            sleep(5);
            continue;
        }
        if (g_last_sock != -1)
        {
            close(g_last_sock);
        }
        g_last_sock = client_fd;
        client_running = TRUE;
        start_receive_client_data(client_fd);
        start_process_desk_business();
    }

    /*deinit*/
    stop_play_mp3_thread();
    stop_video_system();
ERR_INIT_VIDEO_SYS:
    deinit_mpp();
ERR_INIT_MPP:    
    delete_pcm_fifo();
    delete_mp3_fifo();
    destroy_buffer(&g_client_data);

    flock(lock_fd, LOCK_UN);

    return 0;
}