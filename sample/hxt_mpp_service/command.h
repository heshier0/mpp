#ifndef __command_h__
#define __command_h__

#ifdef __cplusplus
extern "C" {
#endif


#define CMD_FLAG            0X5AA5
#define CMD_HEADER_LNGTH    10

#define CMD_START_POSTURE           100
#define CMD_STOP_POSTURE            101
#define CMD_START_VOICE_SAMPLE      102
#define CMD_STOP_VOICE_SAMPLE       103
#define CMD_START_VIDEO_RECORD      104
#define CMD_STOP_VIDEO_RECORD       105
#define CMD_DEL_VIDEO_FILE          106

#define CMD_SETUP_VIDEO_RATIO       200


#pragma pack(push, 1)

typedef struct __cmd_header__
{
    unsigned short flag;                    /*协议标志字段,固定值：0x5AA5*/         
    unsigned short length;                  /*数据包长度*/
    unsigned short type;                    /*协议类型，默认为1表示hxt_desk_service进程*/
    unsigned short cmd;                     /*命令字段*/
    unsigned int reserve;                   /*保留字段*/
}cmd_header_t;

typedef struct __cmd_video_ratio__
{
    unsigned int width;
    unsigned int height;
}video_ratio_t;

typedef struct __cmd_stduy_video__
{
    char video_name[128];
    char snap_name[128];
}study_video_t;

#pragma pack(pop)


#ifdef __cplusplus
}
#endif //__cplusplus


#endif // !__command_h__