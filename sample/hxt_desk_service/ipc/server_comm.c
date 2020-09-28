#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include "utils.h"
#include "command.h"
#include "server_comm.h"

#define MPP_SERVICE_PORT        (10086)
#define MPP_SERVICE_ADDR        ("127.0.0.1")

static int g_service_sock = -1;

int connect_to_mpp_service()
{
    struct sockaddr_in server;
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(MPP_SERVICE_PORT);
    server.sin_addr.s_addr = inet_addr(MPP_SERVICE_ADDR);
    
    int size = sizeof(struct sockaddr_in);
    g_service_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(connect(g_service_sock, (struct sockaddr*)&server, size) < 0)
    {
        utils_print("connect to mpp service failed:%s\n", strerror(errno));
        return;
    }
    utils_print("MPP service connected\n");
    return g_service_sock;
} 

int send_setup_video_ratio_cmd(int width, int height)
{
    int write_count = -1;
    if(g_service_sock == -1)
    {
        return -1;
    }
    video_ratio_t video_ratio;
    bzero(&video_ratio, sizeof(video_ratio_t));
    video_ratio.width = width;
    video_ratio.height = height;

    cmd_header_t header;
    bzero(&header, sizeof(cmd_header_t));
    header.flag = CMD_FLAG;
    header.cmd = CMD_SETUP_VIDEO_RATIO;
    header.length = sizeof(video_ratio_t);
    header.type = 1;
    header.reserve = 0;

    write_count = send(g_service_sock, (void*)&header, sizeof(cmd_header_t), 0);
    write_count += send(g_service_sock, (void*)&video_ratio, sizeof(video_ratio_t), 0);

    return write_count;
}

int send_posture_start_cmd(int width, int height)
{
    int write_count = -1;
    if(g_service_sock == -1)
    {
        return -1;
    }
    video_ratio_t video_ratio;
    bzero(&video_ratio, sizeof(video_ratio_t));
    video_ratio.width = width;
    video_ratio.height = height;

    cmd_header_t header;
    bzero(&header, sizeof(cmd_header_t));
    header.flag = CMD_FLAG;
    header.cmd = CMD_START_POSTURE;
    header.length = sizeof(video_ratio_t);
    header.type = 1;
    header.reserve = 0;

    write_count = send(g_service_sock, (void*)&header, sizeof(cmd_header_t), 0);
    write_count += send(g_service_sock, (void*)&video_ratio, sizeof(video_ratio_t), 0);

    return write_count;
}

int send_posture_stop_cmd()
{
    int write_count = -1;
    if(g_service_sock == -1)
    {
        return -1;
    }
    cmd_header_t header;
    bzero(&header, sizeof(cmd_header_t));
    header.flag = CMD_FLAG;
    header.cmd = CMD_STOP_POSTURE;
    header.length = 0;
    header.type = 1;
    header.reserve = 0;

    write_count = send(g_service_sock, (void*)&header, sizeof(cmd_header_t), 0);

    return write_count;
}

int send_voice_sample_start_cmd()
{
    int write_count = -1;
    if(g_service_sock == -1)
    {
        return -1;
    }
    cmd_header_t header;
    bzero(&header, sizeof(cmd_header_t));
    header.flag = CMD_FLAG;
    header.cmd = CMD_START_VOICE_SAMPLE;
    header.length = 0;
    header.type = 1;
    header.reserve = 0;

    write_count = send(g_service_sock, (void*)&header, sizeof(cmd_header_t), 0);

    return write_count;
}

int send_voice_sample_stop_cmd()
{
    int write_count = -1;
    if(g_service_sock == -1)
    {
        return -1;
    }
    cmd_header_t header;
    bzero(&header, sizeof(cmd_header_t));
    header.flag = CMD_FLAG;
    header.cmd = CMD_STOP_VOICE_SAMPLE;
    header.length = 0;
    header.type = 1;
    header.reserve = 0;

    write_count = send(g_service_sock, (void*)&header, sizeof(cmd_header_t), 0);

    return write_count;
}

int send_recording_mp4_cmd(const char* video_file, const char* snap_file)
{
    int write_count = -1;
    if(g_service_sock == -1)
    {
        return -1;
    }

    // time_t now = time(0);
	// struct tm *_tm = localtime(&now);
	// int child_unid = hxt_get_child_unid();
    study_video_t study_video;
    bzero(&study_video, sizeof(study_video));
    strcpy(study_video.video_name, video_file);
    strcpy(study_video.snap_name, snap_file);
	// snprintf(study_video.video_name, 128, "/user/child_%d/video/%04d%02d%02d-%02d%02d%02d.mp4",
	// 							child_unid,
	// 							_tm->tm_year + 1900,
	// 							_tm->tm_mon + 1, 
	// 							_tm->tm_mday,
	// 							_tm->tm_hour,
	// 							_tm->tm_min,
	// 							_tm->tm_sec);

    // snprintf(study_video.snap_name, 128, "/user/child_%d/snap/%04d%02d%02d-%02d%02d%02d.jpg",
    //                         child_unid,
    //                         _tm->tm_year + 1900,
    //                         _tm->tm_mon + 1, 
    //                         _tm->tm_mday,
    //                         _tm->tm_hour,
    //                         _tm->tm_min,
    //                         _tm->tm_sec);

    cmd_header_t header;
    bzero(&header, sizeof(cmd_header_t));
    header.flag = CMD_FLAG;
    header.cmd = CMD_START_VIDEO_RECORD;
    header.length = sizeof(study_video_t);
    header.type = 1;
    header.reserve = 0;
    utils_print("length of study_info_t is %d\n", sizeof(study_video_t));
    write_count = send(g_service_sock, (void*)&header, sizeof(cmd_header_t), 0);
    write_count += send(g_service_sock, (void*)&study_video, sizeof(study_video_t), 0);

    return write_count;
}

int send_delete_mp4_cmd()
{
    int write_count = -1;
    if(g_service_sock == -1)
    {
        return -1;
    }
    cmd_header_t header;
    bzero(&header, sizeof(cmd_header_t));
    header.flag = CMD_FLAG;
    header.cmd = CMD_DEL_VIDEO_FILE;
    header.length = 0;
    header.type = 1;
    header.reserve = 0;

    write_count = send(g_service_sock, (void*)&header, sizeof(cmd_header_t), 0);

    return write_count;
}

int send_stop_record_mp4_cmd()
{
    int write_count = -1;
    if(g_service_sock == -1)
    {
        return -1;
    }
    cmd_header_t header;
    bzero(&header, sizeof(cmd_header_t));
    header.flag = CMD_FLAG;
    header.cmd = CMD_STOP_VIDEO_RECORD;
    header.length = 0;
    header.type = 1;
    header.reserve = 0;

    write_count = send(g_service_sock, (void*)&header, sizeof(cmd_header_t), 0);

    return write_count;    
}
