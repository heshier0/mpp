#include <sys/types.h>
#include <sys/stat.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include "utils.h"



typedef struct mp4_context
{
    AVFormatContext *format_ctx;
    AVOutputFormat *fmt;
    AVStream *video;
    AVCodecContext *codec_ctx;
    AVCodec *codec;
    AVFrame *picture;
    AVPacket *pkt;
    unsigned char *picture_buf;
    int size;
    int picture_size;
}MP4_CTX_S;

static void encode(MP4_CTX_S *mp4_write_info, AVFrame *frame, AVPacket *pkt)
{
    int ret;

	/* send the frame to the encoder */
	if (frame)
    {
        utils_print("Send frame %d \n", frame->pts);
    }
		
	ret = avcodec_send_frame(mp4_write_info->codec_ctx, frame);
	if (ret < 0) 
    {
		utils_print("Error sending a frame for encoding\n");
		return;
	}

	while (ret >= 0)
     {
		ret = avcodec_receive_packet(mp4_write_info->codec_ctx, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return;
        }
		else if (ret < 0)
         {
			utils_print("Error during encoding\n");
			return;
		}

		mp4_write_info->pkt->stream_index = mp4_write_info->video->index;
		av_packet_rescale_ts(mp4_write_info->pkt, mp4_write_info->codec_ctx->time_base, mp4_write_info->video->time_base);
		mp4_write_info->pkt->pos = -1;
		ret = av_interleaved_write_frame(mp4_write_info->format_ctx, mp4_write_info->pkt);

		av_packet_unref(mp4_write_info->pkt);
	}
}

int fill_mp4_info(const char* mp4_file, int width, int height, MP4_CTX_S *info, int idx, int bitrate)
{
    if(NULL == mp4_file)
    {
        return -1;
    }

    info->picture_size = width * height;

	//[2] --初始化AVFormatContext结构体,根据文件名获取到合适的封装格式
	avformat_alloc_output_context2(&(info->format_ctx), NULL, NULL, mp4_file);

	info->fmt = info->format_ctx->oformat;

	//[3] --打开文件
	if (avio_open(&(info->format_ctx->pb), mp4_file, AVIO_FLAG_READ_WRITE))
	{
		utils_print("output file open fail!");
		return -1;
	}

	//[4] --初始化视频码流
	info->video = avformat_new_stream(info->format_ctx, NULL);
	if (info->video == NULL)
	{
		utils_print("failed allocating output stram\n");
		return -2;
	}
	// avcodec_copy_context(info->pCodecCtx, inCodexc);
	info->video->time_base.num = 1;
	info->video->time_base.den = 25;


	//[5] --编码器Context设置参数
	info->codec_ctx = info->video->codec;
	info->codec_ctx->codec_id = info->fmt->video_codec;
	info->codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	info->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	info->codec_ctx->width = width;
	info->codec_ctx->height = height;
	info->codec_ctx->time_base.num = 1;
	info->codec_ctx->time_base.den = 25;
	info->codec_ctx->bit_rate = bitrate; // 400000
	info->codec_ctx->gop_size = 12;
	info->codec_ctx->thread_count = 4;

	if (info->codec_ctx->codec_id == AV_CODEC_ID_H264)
	{
		utils_print("info->codec_ctx->codec_id == AV_CODEC_ID_H264! \n");
		info->codec_ctx->qmin = 10;
		info->codec_ctx->qmax = idx; // 越大编码质量越差51
		info->codec_ctx->qcompress = 0.6;
	}

	if (info->codec_ctx->codec_id == AV_CODEC_ID_MPEG2VIDEO)
	{
		utils_print("info->codec_ctx->codec_id->codec_id == AV_CODEC_ID_MPEG2VIDEO! \n");
		info->codec_ctx->max_b_frames = 2;
	}
	if (info->codec_ctx->codec_id == AV_CODEC_ID_MPEG1VIDEO)
	{
		utils_print("info->codec_ctx->codec_id == AV_CODEC_ID_MPEG1VIDEO! \n");
		info->codec_ctx->mb_decision = 2;
	}
	//[5]

	//[6] --寻找编码器并打开编码器
	info->codec = avcodec_find_encoder(info->codec_ctx->codec_id);
	if (!info->codec)
	{
		utils_print("no right encoder! \n");
		return -3;
	}
	if (avcodec_open2(info->codec_ctx, info->codec, NULL) < 0)
	{
		utils_print("open encoder fail! \n");
		return -4;
	}

	//输出格式信息
	av_dump_format(info->format_ctx, 0, mp4_file, 1);

	//初始化帧
	info->picture = av_frame_alloc();
	info->picture->width = info->codec_ctx->width;
	info->picture->height = info->codec_ctx->height;
	info->picture->format = info->codec_ctx->pix_fmt;
	info->size = avpicture_get_size(info->codec_ctx->pix_fmt, info->codec_ctx->width, info->codec_ctx->height);
	info->picture_buf = (uint8_t*)av_malloc(info->size);
	avpicture_fill((AVPicture*)info->picture, info->picture_buf, info->codec_ctx->pix_fmt, info->codec_ctx->width, info->codec_ctx->height);

	//[7] --写头文件
	avformat_write_header(info->format_ctx, NULL);
	//[7]

	//AVPacket enpkt; //创建已编码帧
	int y_size = info->codec_ctx->width * info->codec_ctx->height;

	//av_new_packet(&info->pkt, info->size * 3);
	//info->codec_ctx->codec_tag = 828601953;
	utils_print("out_taget = %x  %x \n", info->codec_ctx->codec_tag);

	info->pkt = av_packet_alloc();
	if (!info->pkt)
	{
		return -1;
	}

	return 0;
}

void uninit_write_mp4_info(MP4_CTX_S * info)
{

	if (info->video)
	{
		avcodec_close(info->video->codec);
		av_free(info->picture);
		av_free(info->picture_buf);
	}
	if (info->format_ctx)
	{
		avio_close(info->format_ctx->pb);
		avformat_free_context(info->format_ctx);
	}

	av_packet_free(&info->pkt);

    return;
}

BOOL start_yuv2mp4()
{
    int size; 
    FILE *yuv_file = fopen("/user/vpss_grp0_chn1_720x576_P420_29.yuv", "rb");
    if (!yuv_file)
    {
        utils_print("can't open file\n");
        return FALSE;
    }
    
    int in_w = 720, in_h = 576;
	int framenum = 29;
	const char* out_file = "src01.mp4";

	int y_size = in_w * in_h;

	MP4_CTX_S info;

    // 说明， 30 和图像质量有关， 10-51 都行的，越大质量越差
    // 400000 比特率自行理解，
	int ret = fill_mp4_info("test1111.mp4", in_w, in_h, &info, 30, 400000);


	//[8] --循环编码每一帧
	for (int i = 0; i < framenum; i++)
	{
		//读入YUV
		if (fread(info.picture_buf, 1, y_size * 3 / 2, yuv_file) < 0)
		{
			utils_print("read error\n");
			goto end;
		}
		else if (feof(yuv_file))
			break;

		info.picture->data[0] = info.picture_buf; //亮度Y
		info.picture->data[1] = info.picture_buf + y_size; //U
		info.picture->data[2] = info.picture_buf + y_size * 5 / 4; //V

		info.picture->data[0] = info.picture_buf; //亮度Y
		info.picture->data[1] = info.picture_buf + info.picture_size; //U
		info.picture->data[2] = info.picture_buf + info.picture_size * 5 / 4; //V
		//AVFrame PTS
		info.picture->pts = i;

		encode(&info, info.picture, info.pkt);

	}
	encode(&info, NULL, info.pkt);

	//[8]
end:
	av_write_trailer(info.format_ctx);
	//[10]
	uninit_write_mp4_info(&info);

	fclose(yuv_file);

	return 0;

}