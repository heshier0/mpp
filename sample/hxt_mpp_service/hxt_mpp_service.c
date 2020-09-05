#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#include <board_mpp.h>

#define MPP_SERVICE_PORT        (10086)

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

static void* handle_request(void* args)
{
    if(NULL == args)
    {
        return NULL;
    }

    char buf[256] = {0};
    int new_sock = (int)args;
    while(1)
    {   
        memset(buf, 0, 256);
        int read_count = read(new_sock, buf, sizeof(buf)-1);
        if(read_count > 0)
        {
            buf[read_count] = '\0';
            utils_print("HxtMppService recv: %s\n", buf);
        }
        else
        {
            close(new_sock);
            break;
        }
    }

    return NULL;
}

static void start_process_desk_business(int sock_fd)
{
    pthread_t tid;
    pthread_create(&tid, NULL, handle_request, (void*)sock_fd);
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
    
    create_pcm_fifo();
    create_mp3_fifo();

    if(!init_mpp())
    {
        utils_print("init mpp subsystem error.\n");
        return -1;
    }
    
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
        start_process_desk_business(client_fd);
    }

    /*deinit*/
    // stop_video_system();
    deinit_mpp();
    delete_pcm_fifo();
    delete_mp3_fifo();

    return 0;
}