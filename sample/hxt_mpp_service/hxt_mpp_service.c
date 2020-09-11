#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#include "databuffer.h"
#include "board_mpp.h"
#include "command.h"

#define MPP_SERVICE_PORT        (10086)

#define BUFFER_SIZE             (5*1024)

int g_last_sock = -1;
DATABUFFER g_client_data;

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

    int new_sock = (int)args;
    char buf[256] = {0};
    while(1)
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
            memset(buf, 0, 256);

            use_free_buffer(&g_client_data, read_count);          
        }
        else
        {
            close(new_sock);
            break;
        }
    }

    return NULL;
}

static void* process_desk_business_thread(void *args)
{
    BOOL posturing = FALSE;
    BOOL sampling = FALSE;

    cmd_header_t header;
    int len = sizeof(cmd_header_t);
    video_ratio_t video_ratio;
    study_video_t study_video;
    while(1)
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
                posturing = TRUE;
                bzero(&video_ratio, sizeof(video_ratio_t));
                ptr = get_buffer(&g_client_data, sizeof(video_ratio_t));
                memcpy(&video_ratio, ptr, sizeof(video_ratio_t));
                release_buffer(&g_client_data, sizeof(video_ratio_t));
                // start_video_system(video_ratio.width, video_ratio.height);  
                start_sample_video_thread((void*)&video_ratio);
                utils_print("start posture check\n");
            }
        break;      
        case CMD_STOP_POSTURE:
            if (posturing)
            {
                posturing = FALSE;
                stop_sample_video_thread();
                // stop_video_system();
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
                stop_sample_void_thread();
                utils_print("stop voice sample\n");
            }
        break;      
        case CMD_START_VIDEO_RECORD:
            bzero(&study_video, sizeof(study_video_t));
            ptr = get_buffer(&g_client_data, sizeof(study_video_t));
            memcpy(&study_video, ptr, sizeof(study_video_t));
            release_buffer(&g_client_data, sizeof(study_video_t));
            utils_print("video is %s, snap is %s\n", study_video.video_name, study_video.snap_name);
            start_video_recording(study_video.video_name);
        break;
        case CMD_STOP_VIDEO_RECORD:
            stop_video_recording();
            board_get_snap_from_venc_chn(study_video.snap_name);
        break;
        case CMD_DEL_VIDEO_FILE:
            delete_video();
            unlink(study_video.snap_name);
        break;    
        default:
        break;
        }
    }

    return NULL;
}

static void start_receive_client_data(int sock_fd)
{
    pthread_t tid;
    pthread_create(&tid, NULL, receive_client_data_thread, (void*)sock_fd);
    pthread_detach(tid);
}

static void start_process_desk_business()
{
    pthread_t tid; 
    pthread_create(&tid, NULL, process_desk_business_thread, NULL);
    pthread_detach(tid);
}

int main(int argc, char **argv)
{
    int client_fd = -1;
    struct sockaddr_in client_addr;

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
        delete_pcm_fifo();
        delete_mp3_fifo();
        destroy_buffer(&g_client_data);
        return -1;
    }
    start_video_system(640, 360);
    /* 系统起来就需要播放mp3 */
    start_play_mp3_thread();
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
        start_receive_client_data(client_fd);
        start_process_desk_business();
    }

    /*deinit*/
    stop_play_mp3_thread();
    destroy_buffer(&g_client_data);
    stop_video_system();
    deinit_mpp();
    delete_pcm_fifo();
    delete_mp3_fifo();

    return 0;
}