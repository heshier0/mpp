#include <sys/types.h>
#include <sys/stat.h>

#include "utils.h"
#include "yuv2mp4.h"

static void encode(MP4_CTX_S *mp4_ctx, AVFrame *frame, AVPacket *pkt)
{
    int ret;

	if(!frame)
	{
		return;
	}
	/* send the frame to the encoder */
	ret = avcodec_send_frame(mp4_ctx->codec_ctx, frame);
	if (ret < 0) 
    {
		utils_print("Error sending a frame for encoding\n");
		return;
	}

	while (ret >= 0)
     {
		ret = avcodec_receive_packet(mp4_ctx->codec_ctx, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return;
        }
		else if (ret < 0)
        {
			return;
		}

		mp4_ctx->pkt->stream_index = mp4_ctx->video->index;
		av_packet_rescale_ts(mp4_ctx->pkt, mp4_ctx->codec_ctx->time_base, mp4_ctx->video->time_base);
		mp4_ctx->pkt->pos = -1;
		ret = av_interleaved_write_frame(mp4_ctx->format_ctx, mp4_ctx->pkt);

		av_packet_unref(mp4_ctx->pkt);
	}
}

int init_mp4_context(const char* mp4_file, int width, int height, MP4_CTX_S *mp4_ctx)
{
    if(NULL == mp4_file)
    {
        return -1;
    }

	int idx = 30;
	int bitrate = 400000;

    mp4_ctx->picture_size = width * height;

	//初始化AVFormatContext结构体,根据文件名获取到合适的封装格式
	avformat_alloc_output_context2(&(mp4_ctx->format_ctx), NULL, NULL, mp4_file);

	mp4_ctx->fmt = mp4_ctx->format_ctx->oformat;

	//打开文件
	if (avio_open(&(mp4_ctx->format_ctx->pb), mp4_file, AVIO_FLAG_READ_WRITE))
	{
		utils_print("output file open fail!");
		return -1;
	}

	//初始化视频码流
	mp4_ctx->video = avformat_new_stream(mp4_ctx->format_ctx, NULL);
	if (mp4_ctx->video == NULL)
	{
		utils_print("failed allocating output stram\n");
		return -2;
	}
	// avcodec_copy_context(mp4_ctx->pCodecCtx, inCodexc);
	mp4_ctx->video->time_base.num = 1;
	mp4_ctx->video->time_base.den = 25;

	//编码器Context设置参数
	mp4_ctx->codec_ctx = mp4_ctx->video->codec;
	mp4_ctx->codec_ctx->codec_id = mp4_ctx->fmt->video_codec;
	mp4_ctx->codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	mp4_ctx->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	mp4_ctx->codec_ctx->width = width;
	mp4_ctx->codec_ctx->height = height;
	mp4_ctx->codec_ctx->time_base.num = 1;
	mp4_ctx->codec_ctx->time_base.den = 25;
	mp4_ctx->codec_ctx->bit_rate = bitrate; // 400000
	mp4_ctx->codec_ctx->gop_size = 12;
	mp4_ctx->codec_ctx->thread_count = 4;

	if (mp4_ctx->codec_ctx->codec_id == AV_CODEC_ID_H264)
	{
		utils_print("mp4_ctx->codec_ctx->codec_id == AV_CODEC_ID_H264! \n");
		mp4_ctx->codec_ctx->qmin = 10;
		mp4_ctx->codec_ctx->qmax = idx; // 越大编码质量越差51
		mp4_ctx->codec_ctx->qcompress = 0.6;
	}

	if (mp4_ctx->codec_ctx->codec_id == AV_CODEC_ID_MPEG2VIDEO)
	{
		utils_print("mp4_ctx->codec_ctx->codec_id->codec_id == AV_CODEC_ID_MPEG2VIDEO! \n");
		mp4_ctx->codec_ctx->max_b_frames = 2;
	}
	if (mp4_ctx->codec_ctx->codec_id == AV_CODEC_ID_MPEG1VIDEO)
	{
		utils_print("mp4_ctx->codec_ctx->codec_id == AV_CODEC_ID_MPEG1VIDEO! \n");
		mp4_ctx->codec_ctx->mb_decision = 2;
	}

	//寻找编码器并打开编码器
	mp4_ctx->codec = avcodec_find_encoder(mp4_ctx->codec_ctx->codec_id);
	if (!mp4_ctx->codec)
	{
		utils_print("no right encoder! \n");
		return -3;
	}
	if (avcodec_open2(mp4_ctx->codec_ctx, mp4_ctx->codec, NULL) < 0)
	{
		utils_print("open encoder fail! \n");
		return -4;
	}

	//输出格式信息
	av_dump_format(mp4_ctx->format_ctx, 0, mp4_file, 1);

	//初始化帧
	mp4_ctx->picture = av_frame_alloc();
	mp4_ctx->picture->width = mp4_ctx->codec_ctx->width;
	mp4_ctx->picture->height = mp4_ctx->codec_ctx->height;
	mp4_ctx->picture->format = mp4_ctx->codec_ctx->pix_fmt;
	mp4_ctx->size = avpicture_get_size(mp4_ctx->codec_ctx->pix_fmt, mp4_ctx->codec_ctx->width, mp4_ctx->codec_ctx->height);
	mp4_ctx->picture_buf = (uint8_t*)av_malloc(mp4_ctx->size);
	avpicture_fill((AVPicture*)mp4_ctx->picture, mp4_ctx->picture_buf, mp4_ctx->codec_ctx->pix_fmt, mp4_ctx->codec_ctx->width, mp4_ctx->codec_ctx->height);

	avformat_write_header(mp4_ctx->format_ctx, NULL);

	int y_size = mp4_ctx->codec_ctx->width * mp4_ctx->codec_ctx->height;
	utils_print("out_taget = %x  %x \n", mp4_ctx->codec_ctx->codec_tag);

	mp4_ctx->pkt = av_packet_alloc();
	if (!mp4_ctx->pkt)
	{
		return -1;
	}

	return 0;
}

void uninit_mp4_context(MP4_CTX_S * mp4_ctx)
{
	encode(mp4_ctx, NULL, mp4_ctx->pkt);
	av_write_trailer(mp4_ctx->format_ctx);

	if (mp4_ctx->video)
	{
		avcodec_close(mp4_ctx->video->codec);
		av_free(mp4_ctx->picture);
		av_free(mp4_ctx->picture_buf);
	}
	if (mp4_ctx->format_ctx)
	{
		avio_close(mp4_ctx->format_ctx->pb);
		avformat_free_context(mp4_ctx->format_ctx);
	}

	av_packet_free(&mp4_ctx->pkt);

    return;
}

void yuv2mp4(MP4_CTX_S *mp4_ctx, const char* yuv_data, const int width, const int height, const char* out_file, int frame_idx)
{
	int y_size = width * height;
	int yuv_size = y_size * 3 / 2;
	
	if(NULL == mp4_ctx || NULL == yuv_data)
	{
		return;
	}
	memcpy(mp4_ctx->picture_buf, yuv_data, yuv_size);
	mp4_ctx->picture->data[0] = mp4_ctx->picture_buf; 							//亮度Y
	mp4_ctx->picture->data[1] = mp4_ctx->picture_buf + y_size; 					//U
	mp4_ctx->picture->data[2] = mp4_ctx->picture_buf + y_size * 5 / 4; 			//V

	mp4_ctx->picture->data[0] = mp4_ctx->picture_buf; 										//亮度Y
	mp4_ctx->picture->data[1] = mp4_ctx->picture_buf + mp4_ctx->picture_size; 				//U
	mp4_ctx->picture->data[2] = mp4_ctx->picture_buf + mp4_ctx->picture_size * 5 / 4; 		//V
	//AVFrame PTS
	mp4_ctx->picture->pts = frame_idx;

	encode(mp4_ctx, mp4_ctx->picture, mp4_ctx->pkt);

	return;
}

#if 0
BOOL start_yuv2mp4()
{
	char *in_file = "/user/vpss_grp0_chn1_720x576_P420_29.yuv";
    char out_file[128];
    utils_generate_mp4_file_name(out_file);
    int width = 720;
    int height = 576;
    MP4_CTX_S mp4_ctx;
    init_mp4_context(out_file, width, height, &mp4_ctx);

    FILE* fp = fopen(in_file, "rb");
    char yuv_buf[720*576*3/2] = {0};
    int read_count = 0;
    int frame_idx = 0;
    while(!feof(fp))
    {
        read_count = fread(yuv_buf, 1, width*height*3/2, fp);
        yuv2mp4(&mp4_ctx, yuv_buf, width, height, out_file, frame_idx);
        frame_idx ++;
        
    }
    uninit_mp4_context(&mp4_ctx);
    fclose(fp);
}
#endif