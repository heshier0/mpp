#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#include <sample_comm.h>

#include "board_media.h"

#define USING_SEQ
#define STREAM_FRAME_RATE 25

static int g_count = 0;

typedef struct media_context
{
	AVFormatContext* format_ctx;			//每个通道的AVFormatContext
	int video_idx;							//视频流索引号
	int first_IDR;							//第一帧是I帧标志	
	long vpts_inc;							//用于视频帧递增计数
	unsigned long long video_pts;			//视频PTS
	unsigned long long first_video;			//视频第一帧标志
	long moov_pos;							//moov的pos，未使用		
	int moov_flags;							//moov前置标志，未使用
	int file_flags;
	char filename[128];						//视频绝对路径
}MediaCtx;

MediaCtx g_media_ctx;
pthread_mutex_t g_media_mutex = PTHREAD_MUTEX_INITIALIZER;


static BOOL init_video_codec_params(int width, int height)
{
	AVOutputFormat *out_fmt = NULL;
	AVCodecParameters *av_codec_params = NULL;
	AVStream *v_stream = NULL;		//video stream 
	AVCodec *v_codec = NULL;		//video codec

	out_fmt = g_media_ctx.format_ctx->oformat;
	v_codec = avcodec_find_encoder(out_fmt->video_codec);
	if(NULL == v_codec)
	{
		utils_print("Muxing: could not find video encoder H264\n");
		return FALSE;
	}

	//cerate new video stream
	v_stream = avformat_new_stream(g_media_ctx.format_ctx, v_codec);
	if(NULL == v_stream)
	{
		utils_print("Muxing: could not allocate vcodec stream\n");
		return FALSE;
	}
	v_stream->id = g_media_ctx.format_ctx->nb_streams - 1;
	
	g_media_ctx.video_idx = v_stream->index;

	av_codec_params = v_stream->codecpar;

	if (v_codec->type == AVMEDIA_TYPE_VIDEO)
	{
		av_codec_params->codec_type = AVMEDIA_TYPE_VIDEO;
		av_codec_params->codec_id = AV_CODEC_ID_H264;
		av_codec_params->bit_rate = 0;
		av_codec_params->width = height;
		av_codec_params->height = width;
		av_codec_params->format = AV_PIX_FMT_YUV420P;
		
		v_stream->time_base = (AVRational){1, STREAM_FRAME_RATE};
	}

	return TRUE;
}

static BOOL add_sps_pps(unsigned char* buf, unsigned int size, int width, int height)
{
	int ret = 0;

	if(!init_video_codec_params(width, height))
	{
		utils_print("Muxing: init video codec params failed\n");
		goto INIT_VIDEO_CODEC_PARAMS_FAILED;
	}


	g_media_ctx.format_ctx->streams[g_media_ctx.video_idx]->codecpar->extradata_size = size;
	g_media_ctx.format_ctx->streams[g_media_ctx.video_idx]->codecpar->extradata = (unsigned char*)av_malloc(size + AV_INPUT_BUFFER_PADDING_SIZE);
	memcpy(g_media_ctx.format_ctx->streams[g_media_ctx.video_idx]->codecpar->extradata, buf, size);

	ret = avformat_write_header(g_media_ctx.format_ctx, NULL);
	if (ret < 0)
	{
		utils_print("Muxing: avformat_write_header failed\n");
		goto WRITE_HEADER_FAILED;
	}

	return TRUE;

WRITE_HEADER_FAILED:
	if (g_media_ctx.format_ctx && !(g_media_ctx.format_ctx->flags & AVFMT_NOFILE))
	{
		avio_close(g_media_ctx.format_ctx->pb);
	}

INIT_VIDEO_CODEC_PARAMS_FAILED:
	if(NULL != g_media_ctx.format_ctx)
	{
		avformat_free_context(g_media_ctx.format_ctx);
	}
	g_media_ctx.video_idx = -1;
	g_media_ctx.vpts_inc = 0;
	g_media_ctx.first_IDR = 0;	//sps,pps flags cleared
	return FALSE;
}

/* init mp4 file structure */
BOOL board_create_mp4_file(const char* filename)
{
	int ret = 0;
	AVOutputFormat *output_fmt = NULL;

	// pthread_mutex_lock(&g_media_mutex);
	g_count = 0;
	memset(&g_media_ctx, 0, sizeof(g_media_ctx));

	if (NULL == filename)
	{
		return FALSE;
	}
	strcpy(g_media_ctx.filename, filename);

	avformat_alloc_output_context2(&(g_media_ctx.format_ctx), NULL, NULL, g_media_ctx.filename);
	if(NULL == g_media_ctx.format_ctx)
	{
		avformat_alloc_output_context2(&(g_media_ctx.format_ctx), NULL, "mp4", g_media_ctx.filename);
		if(NULL == g_media_ctx.format_ctx)
		{
			utils_print("Muxing: avformat_alloc_output_context2 fialed\n");
			return FALSE;
		}
	}

	output_fmt = g_media_ctx.format_ctx->oformat;
	if (output_fmt->video_codec == AV_CODEC_ID_NONE)
	{
		utils_print("Muxing: add_video_stream ID failed\n");
		goto OUTPUT_FMT_FAILED;
	}

	if (!(output_fmt->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&(g_media_ctx.format_ctx->pb), g_media_ctx.filename, AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			utils_print("Muxing: could not create video file: %s\n", g_media_ctx.filename);
			goto AVIO_OPEN_FAILED;
		}
	}

	g_media_ctx.video_pts = 0;
	g_media_ctx.first_video = 0;
	g_media_ctx.video_idx = -1;
	g_media_ctx.first_IDR = 0;

	utils_print("TO create %s!\n", g_media_ctx.filename);
	// pthread_mutex_unlock(&g_media_mutex);

	return TRUE;

AVIO_OPEN_FAILED:
	if (g_media_ctx.format_ctx && !(g_media_ctx.format_ctx->flags & AVFMT_NOFILE))
	{
		avio_close(g_media_ctx.format_ctx->pb);
	}

OUTPUT_FMT_FAILED:
	if (NULL != g_media_ctx.format_ctx)
	{
		avformat_free_context(g_media_ctx.format_ctx);
	}

	// pthread_mutex_unlock(&g_media_mutex);
	return FALSE;
}

void board_close_mp4_file()
{
	int ret;

	// pthread_mutex_lock(&g_media_mutex);
	if(g_media_ctx.format_ctx)
	{
		ret = av_write_trailer(g_media_ctx.format_ctx);
		if (ret < 0)
		{
			utils_print("av_write_trailer failed\n");
		}
	}

	if (g_media_ctx.format_ctx && !(g_media_ctx.format_ctx->oformat->flags & AVFMT_NOFILE))
	{
		ret = avio_close(g_media_ctx.format_ctx->pb);
		if (ret < 0)
		{
			utils_print("avio_close failed\n");
		}
	}

	if (g_media_ctx.format_ctx)
	{
		avformat_free_context(g_media_ctx.format_ctx);
		g_media_ctx.format_ctx = NULL;
	}

	//clear flags
	g_media_ctx.video_idx = -1;
	g_media_ctx.vpts_inc = 0;
	g_media_ctx.first_video = 0;
	g_media_ctx.first_IDR = 0;

	g_count = 0;

	utils_print("SAVE MP4 %s Successfully!\n", g_media_ctx.filename);
	// pthread_mutex_unlock(&g_media_mutex);
}

BOOL board_write_mp4(void* input_stream, int width, int height)
{
	int ret = 0;
	unsigned int i = 0;
	unsigned char* pack_virt_addr = NULL;
	unsigned int pack_len = 0;
	
	AVStream *video_stream = NULL;
	AVPacket pkt;

	unsigned char sps_buf[32];
	unsigned char pps_buf[32];
	unsigned char sps_pps_buf[64];
	unsigned int pps_len = 0;
	unsigned int sps_len = 0;

	if(NULL == input_stream)
	{
		return FALSE;
	}
	VENC_STREAM_S* venc_stream = (VENC_STREAM_S*)input_stream;

	// pthread_mutex_lock(&g_media_mutex);
	for(i = 0; i < venc_stream->u32PackCount; i++)
	{
		pack_virt_addr = venc_stream->pstPack[i].pu8Addr + venc_stream->pstPack[i].u32Offset;
		pack_len = venc_stream->pstPack[i].u32Len - venc_stream->pstPack[i].u32Offset;
		av_init_packet(&pkt);
		pkt.flags = AV_PKT_FLAG_KEY;	//key frame default
		switch (venc_stream->pstPack[i].DataType.enH264EType)
		{
		case H264E_NALU_SPS:
			pkt.flags = 0;
			if (g_media_ctx.first_IDR == 2) //if first frame is not SPS, discard
			{
				continue;
			}
			else
			{
				sps_len = pack_len;
				memcpy(sps_buf, pack_virt_addr, sps_len);
				if (g_media_ctx.first_IDR == 1)		// PPS frame received
				{
					memcpy(sps_pps_buf, sps_buf, sps_len);				//sps
					memcpy(sps_pps_buf + sps_len, pps_buf, pps_len);	//add pps
					if (!add_sps_pps(sps_pps_buf, sps_len + pps_len, width, height))
					{
						return FALSE;
					}
				}
				g_media_ctx.first_IDR ++;
			}
			continue;
		case H264E_NALU_PPS:
			pkt.flags = 0;						//not key frame;
			if (g_media_ctx.first_IDR == 2)		//not a PPS, discard
			{
				continue;
			}
			else
			{
				pps_len = pack_len;
				memcpy(pps_buf, pack_virt_addr, pps_len);
				if (g_media_ctx.first_IDR == 1)
				{
					memcpy(sps_pps_buf, sps_buf, sps_len);
					memcpy(sps_pps_buf + sps_len, pps_buf, pps_len);	//add pps
					if (!add_sps_pps(sps_pps_buf, sps_len + pps_len, width, height))
					{
						return FALSE;
					}
				}
				g_media_ctx.first_IDR ++;
			}
			continue;
		case H264E_NALU_SEI:				//enforce 
			continue;	
		case H264E_NALU_PSLICE:				//P frame
		case H264E_NALU_IDRSLICE:			//I frame
			if (g_media_ctx.first_IDR != 2)
			{
				continue;					//ignore
			}
			break;
		default:
			break;
		}

		if (g_media_ctx.video_idx < 0)
		{
			utils_print("video index less than 0\n");
			return TRUE;
		}

		if (g_media_ctx.first_video == 0)
		{
			g_media_ctx.first_video = 1;
			#ifdef USING_SEQ
				g_media_ctx.video_pts = venc_stream->u32Seq;
			#else
				g_media_ctx.video_pts = venc_stream->pstPack[i].u64PTS;
			#endif
		}
		
		video_stream = g_media_ctx.format_ctx->streams[g_media_ctx.video_idx];
		pkt.stream_index = video_stream->index;
#if 0
		pkt.pts = av_rescale_q_rnd((g_media_ctx.vpts_inc++), 
									video_stream->codec->time_base, 
									video_stream->time_base, 
									(enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.pts, 
									video_stream->time_base, 
									video_stream->time_base, 
							(enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
#else
		#ifdef USING_SEQ
			pkt.dts = pkt.pts = av_rescale_q_rnd(venc_stream->u32Seq - g_media_ctx.video_pts, 
										(AVRational){1, STREAM_FRAME_RATE},
										video_stream->time_base, 
										(enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		#else
			pkt.pts = pkt.dts = (long long int)((venc_stream->pstPack[i].u64PTS - g_media_ctx.video_pts) * 0.09 + 0.5);
		#endif	
#endif		
		// pkt.duration = 40;
		// pkt.duration = av_rescale_q(pkt.duration, (AVRational){1, STREAM_FRAME_RATE}, video_stream->time_base);
		pkt.pos = -1;
		pkt.data = pack_virt_addr;
		pkt.size = pack_len;

		ret = av_interleaved_write_frame(g_media_ctx.format_ctx, &pkt);
		if(ret < 0)
		{
			utils_print("Muxing: cannot write video frame\n");
			return FALSE;
		}						
	}
	// pthread_mutex_unlock(&g_media_mutex);

	return TRUE;
}

void board_delete_current_mp4_file()
{

	unlink(g_media_ctx.filename);
	utils_print("delete MP4 %s\n", g_media_ctx.filename);
	
	return;
}