#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <libavformat/avformat.h>

#include "sample_comm.h"
#include "utils.h"
//#define USING_PTS
#define USING_SEQ

#define VENC_CHN_NUM 1
#define STREAM_FRAME_RATE 25

typedef struct
{
	AVFormatContext* g_OutFmt_Ctx;	    //每个通道的AVFormatContext
	int vi;	                            //视频流索引号
	int ai;	                            //音频流索引号
	HI_BOOL b_First_IDR_Find;	        //第一帧是I帧标志
	long int VptsInc;	                //用于视频帧递增计数
	long int AptsInc;	                //音频帧递增
	HI_U64 Audio_PTS;	                //音频PTS
	HI_U64 Video_PTS;	                //视频PTS
	HI_U64 Afirst;	                    //是文件第一帧音频标志
	HI_U64 Vfirst;	                    //视频第一帧标志
	long int moov_pos;	                //moov的pos，未使用
	int moov_flags;	                    //moov前置标志，未使用
	int file_flags;	
	char filename[128];	               //文件名
} ffmpegCtx_t;
ffmpegCtx_t ffmpegCtx[VENC_CHN_NUM];

typedef struct media_context
{
	AVFormatContext* foramt_ctx;
	int vi;
	BOOL first_IDR;
	long vpts_inc;
	unsigned int64 first_video;
	long moov_flags;
	int moov_flags;
	int file_flags;
	char filename[128];
}MediaCtx;
MediaCtx g_media_ctx;

// static ffmpegCtx_t * GetVencChnCtx(int VeChn)
// {
// 	return (ffmpegCtx_t * )&ffmpegCtx[VeChn];
// }

static void generate_video_file_name()
{
	struct tm *tm;
	time_t now = time(0);
	tm = localtime(&now);
	snprintf(g_media_ctx.filename, 128, "/user/%04d%02d%02d-%02d%02d%02d.mp4",
				tm->tm_year + 1900,
				tm->tm_mon + 1, 
				tm->tm_mday,
				tm->tm_hour,
				tm->tm_min,
				tm->tm_sec);

	return;
}

static void generate_file_name(VENC_CHN VeChn)
{
	struct tm * tm;
	time_t now = time(0);
	ffmpegCtx_t * fc = GetVencChnCtx(VeChn);
	
	tm = localtime(&now);
	int child_unid = hxt_get_children_unid();
	snprintf(fc->filename, 128, "/user/child_%d/video/%04d%02d%02d-%02d%02d%02d.mp4",
								child_unid,
								tm->tm_year + 1900,
								tm->tm_mon + 1,
								tm->tm_mday,
								tm->tm_hour,
								tm->tm_min,
								tm->tm_sec);	
}


static int HI_PDT_Add_Stream(VENC_CHN VeChn)
{
    AVOutputFormat *pOutFmt = NULL;			//用于获取AVFormatContext->Format
    AVCodecParameters *vAVCodecPar=NULL;	//新替代参数AVStream->CodecPar

    AVStream *vAVStream = NULL;				//用于指向新建的视频流
	AVCodec *vcodec = NULL;	    			//用于指向视频编码器

	ffmpegCtx_t * fc = GetVencChnCtx(VeChn);
	
	pOutFmt = fc->g_OutFmt_Ctx->oformat;	//输出Format
	
    vcodec = avcodec_find_encoder(pOutFmt->video_codec);	//查找视频编码器，默认就是H264了
    if (NULL == vcodec)
    {
        printf("Muxing:could not find video encoder H264\n");
        return -1;
    }
 	
	//根据视频编码器信息（H264），在AVFormatContext里新建视频流通道
    vAVStream = avformat_new_stream(fc->g_OutFmt_Ctx, vcodec);	
    if (NULL == vAVStream)
    {
       printf("Muxing:could not allocate vcodec stream \n");
       return -1;
    }
   
    //给新建的视频流一个ID，0
    vAVStream->id = fc->g_OutFmt_Ctx->nb_streams - 1;	//nb_streams是当前AVFormatContext里面流的数量
    
	fc->vi = vAVStream->index;	//获取视频流的索引号
	
    vAVCodecPar = vAVStream->codecpar;	//
	if(vcodec->type == AVMEDIA_TYPE_VIDEO)	//编码器是视频编码器
    {
    	//对视频流的参数设置
		vAVCodecPar->codec_type = AVMEDIA_TYPE_VIDEO;	
        vAVCodecPar->codec_id = AV_CODEC_ID_H264;
        vAVCodecPar->bit_rate = 2000;	//kbps，好像不需要
        vAVCodecPar->width = 1280;	//像素
        vAVCodecPar->height = 720;
        vAVStream->time_base = (AVRational){1, STREAM_FRAME_RATE};	//时间基
        vAVCodecPar->format = AV_PIX_FMT_YUV420P;
    }

    return HI_SUCCESS;
}

static HI_S32 HI_ADD_SPS_PPS(VENC_CHN VeChn, uint8_t *buf, uint32_t size)
{
	HI_S32 ret;
	ffmpegCtx_t * fc = GetVencChnCtx(VeChn);
	
	ret = HI_PDT_Add_Stream(VeChn);	//创建一个新流并添加到当前AVFormatContext中
	if(ret < 0)
	{
		printf("Muxing:HI_PDT_Add_Stream faild\n");
		goto Add_Stream_faild;
	}
	
	fc->g_OutFmt_Ctx->streams[fc->vi]->codecpar->extradata_size = size;
	fc->g_OutFmt_Ctx->streams[fc->vi]->codecpar->extradata = (uint8_t*)av_malloc(size + AV_INPUT_BUFFER_PADDING_SIZE);
	memcpy(fc->g_OutFmt_Ctx->streams[fc->vi]->codecpar->extradata, buf, size);	//写入SPS和PPS

	ret = avformat_write_header(fc->g_OutFmt_Ctx, NULL);		//写文件头
	
	if(ret < 0)
	{
		printf("Muxing:avformat_write_header faild\n");
		goto write_header_faild;
	}	
    return HI_SUCCESS;
    
write_header_faild:
	if (fc->g_OutFmt_Ctx && !(fc->g_OutFmt_Ctx->flags & AVFMT_NOFILE))
		avio_close(fc->g_OutFmt_Ctx->pb);
	
Add_Stream_faild:
    if(NULL != fc->g_OutFmt_Ctx)
		avformat_free_context(fc->g_OutFmt_Ctx);
	fc->vi = -1;
	fc->ai = -1;		//AeChn
	fc->VptsInc=0;
	fc->AptsInc=0;
	fc->b_First_IDR_Find = 0;	//sps，pps帧标志清除
	return HI_FAILURE;
}

//MP4创建函数：初始化，写文件头。
int HI_PDT_CreateMp4(VENC_CHN VeChn)
{
    int ret = 0; 
    ffmpegCtx_t * fc = GetVencChnCtx(VeChn);
    
    AVOutputFormat *pOutFmt = NULL;	//输出Format指针
    #ifdef FFMPEG_MUXING	
    AVCodec *audio_codec;
    #endif
    
	generate_file_name(VeChn);

    avformat_alloc_output_context2(&(fc->g_OutFmt_Ctx), NULL, NULL, fc->filename);//初始化输出视频码流的AVFormatContext。
	if (NULL == fc->g_OutFmt_Ctx)	//失败处理
    {	
        printf("Muxing:Could not deduce output format from file extension: using mp4. \n");//添加日志
        avformat_alloc_output_context2(&(fc->g_OutFmt_Ctx), NULL, "mp4", fc->filename);
		if (NULL == fc->g_OutFmt_Ctx)
    	{
    		printf("Muxing:avformat_alloc_output_context2 failed\n");
        	return -1;
    	}
    }

    pOutFmt = fc->g_OutFmt_Ctx->oformat;		//获取输出Format指针
    
    if (pOutFmt->video_codec == AV_CODEC_ID_NONE)	//检查视频编码器
    {
        printf("Muxing:add_video_stream ID failed\n"); 
		goto exit_outFmt_failed;
	}
	
	// if (pOutFmt->audio_codec == AV_CODEC_ID_NONE)	//检查音频编码器
    // {
    //     printf("Muxing:add_audio_stream ID failed\n"); 
	// 	goto exit_outFmt_failed;
	// }
	
    if (!(pOutFmt->flags & AVFMT_NOFILE))	//应该是判断文件IO是否打开
    {
        ret = avio_open(&(fc->g_OutFmt_Ctx->pb), fc->filename, AVIO_FLAG_WRITE);	//创建并打开mp4文件
        if (ret < 0)
        {
        	printf("chan:%d filename:%s\n",VeChn, fc->filename);
            printf("Muxing:could not create video file\n");
            goto exit_avio_open_failed;
        }
    }
    
	//初始化一些参数
	fc->Video_PTS = 0;	
	// fc->Audio_PTS = 0;
	fc->Vfirst = 0;
	// fc->Afirst = 0;
	fc->vi = -1;
	fc->ai = -1;
	fc->b_First_IDR_Find = 0;
	return HI_SUCCESS;
	
//错误处理
exit_avio_open_failed:	
	if (fc->g_OutFmt_Ctx && !(fc->g_OutFmt_Ctx->flags & AVFMT_NOFILE))
		avio_close(fc->g_OutFmt_Ctx->pb);
		
exit_outFmt_failed:
	if(NULL != fc->g_OutFmt_Ctx)
		avformat_free_context(fc->g_OutFmt_Ctx);
	return -1;
}

void HI_PDT_CloseMp4(VENC_CHN VeChn)
{
	int ret;
	ffmpegCtx_t * fc = GetVencChnCtx(VeChn);
	
    if (fc->g_OutFmt_Ctx)
    {
       ret = av_write_trailer(fc->g_OutFmt_Ctx);	//写文件尾
       if(ret < 0)
       {
       		printf("av_write_trailer faild\n");
       }
    }
    if (fc->g_OutFmt_Ctx && !(fc->g_OutFmt_Ctx->oformat->flags & AVFMT_NOFILE)) //文件状态检测
	{
		ret = avio_close(fc->g_OutFmt_Ctx->pb);	//关闭文件
		if(ret < 0)
        {
       		printf("avio_close faild\n");
        }
	}
	if (fc->g_OutFmt_Ctx)
    {
        avformat_free_context(fc->g_OutFmt_Ctx);	//释放结构体
        fc->g_OutFmt_Ctx = NULL;
    }
    //清除相关标志
	fc->vi = -1;
	fc->ai = -1;		
	fc->VptsInc=0;
	fc->AptsInc=0;
	fc->Afirst=0;
	fc->Vfirst=0;
	fc->b_First_IDR_Find = 0;

	utils_print("SAVE MP4 Successfully\n");
}

HI_S32 HI_PDT_WriteVideo(VENC_CHN VeChn, VENC_STREAM_S *pstStream)
{
	HI_U32 i=0;	
	HI_U8* pPackVirtAddr = NULL;	//码流首地址
	HI_U32 u32PackLen = 0;			//码流长度
    int ret = 0;
    AVStream *Vpst = NULL; 			//视频流指针
    AVPacket pkt;					//音视频包结构体，这个包不是海思的包，填充之后，用于最终写入数据
    uint8_t sps_buf[32];			//
    uint8_t pps_buf[32];
    uint8_t sps_pps_buf[64];	
    HI_U32 pps_len=0;
    HI_U32 sps_len=0;
    ffmpegCtx_t * fc = GetVencChnCtx(VeChn);
    
	if(NULL == pstStream)	//裸码流有效判断
	{
		return HI_FAILURE;
	}

	// frame I : 4 packs; frame P: 1 packs
    for (i = 0 ; i < pstStream->u32PackCount; i++)	
    {
    	//从海思码流包中获取数据地址，长度
        pPackVirtAddr = pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset;	
        u32PackLen = pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset;	
		av_init_packet(&pkt);	//初始化AVpack包，
		pkt.flags=AV_PKT_FLAG_KEY;	//默认是关键帧
		switch(pstStream->pstPack[i].DataType.enH264EType)
		{
			case H264E_NALU_SPS:	//如果这个包是SPS
				pkt.flags =   0;	//不是关键帧
				if(fc->b_First_IDR_Find == 2)	//如果不是第一个SPS帧
				{
					continue; //discard
				}
				else //如果是第一个SPS帧
				{
					sps_len = u32PackLen;
					memcpy(sps_buf, pPackVirtAddr, sps_len);
					if(fc->b_First_IDR_Find == 1)	//如果PPS帧已经收到
					{
						memcpy(sps_pps_buf, sps_buf, sps_len);	//复制sps
						memcpy(sps_pps_buf+sps_len, pps_buf, pps_len);	//加上pps
						//去添加视频流，和SPS PPS信息，这步之后才开始写入视频帧
						ret = HI_ADD_SPS_PPS(VeChn, sps_pps_buf, sps_len+pps_len);
						if(ret < 0)
							return HI_FAILURE;
					}
					fc->b_First_IDR_Find++;
				}
				continue;
			case H264E_NALU_PPS:
				pkt.flags = 0;	//不是关键帧
				if(fc->b_First_IDR_Find == 2)	//如果不是第一个PPS帧
				{
					continue;
				}
				else //是第一个PPS帧
				{
					pps_len = u32PackLen;
					memcpy(pps_buf, pPackVirtAddr, pps_len);	//复制
					if(fc->b_First_IDR_Find ==1)	//如果SPS帧已经收到
					{
						memcpy(sps_pps_buf, sps_buf, sps_len);
						memcpy(sps_pps_buf + sps_len, pps_buf, pps_len);
						//这里和SPS那里互斥，只有一个会执行，主要是看SPS和PPS包谁排在后面
						ret = HI_ADD_SPS_PPS(VeChn, sps_pps_buf, sps_len+pps_len); 
						if(ret < 0)
							return HI_FAILURE;
					}
					fc->b_First_IDR_Find++;
				}
				continue;
			case H264E_NALU_SEI:	//增强帧
				continue;
			case H264E_NALU_PSLICE:		//P帧
			case H264E_NALU_IDRSLICE:	//I帧
				if(fc->b_First_IDR_Find !=2)	//如果这个文件还没有收到过sps和pps帧
				{
					continue;	//跳过，不处理这帧
				}
				break;
			default:
				break;
		}
		if(fc->vi < 0)	//流索引号，如果g_OutFmt_Ctx里面还没有新建视频流，也就是说还没收到I帧
		{
			printf("vi less than 0 \n");
			return HI_SUCCESS;
		}
		if(fc->Vfirst==0)	//如果是文件的第一帧视频
		{
			fc->Vfirst=1;	
			#ifdef USING_SEQ						//使用帧序号计算PTS
			    fc->Video_PTS = pstStream->u32Seq; 	//记录初始序号
			#endif
			#ifdef USING_PTS						//直接使用海思的PTS
			    fc->Video_PTS = pstStream->pstPack[i].u64PTS;	//记录开始时间戳
			#endif
		}
		Vpst = fc->g_OutFmt_Ctx->streams[fc->vi];	//根据索引号获取视频流地址
	    pkt.stream_index = Vpst->index;				//视频流的索引号 赋给 包里面的流索引号，表示这个包属于视频流
		
		//以下，时间基转换，PTS很重要，涉及音视频同步问题
		#if 0	//原博主的，可以用
			pkt.pts = av_rescale_q_rnd((fc->VptsInc++), Vpst->codec->time_base,Vpst->time_base,(enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
			pkt.dts = av_rescale_q_rnd(pkt.pts, Vpst->time_base,Vpst->time_base,(enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		#endif
		#if 1	
			#ifdef USING_SEQ	
				pkt.pts = av_rescale_q_rnd(pstStream->u32Seq - fc->Video_PTS, (AVRational){1, STREAM_FRAME_RATE},Vpst->time_base,(enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
				pkt.dts = pkt.pts;	//只有I、P帧，相等就行了
			#endif
			#ifdef USING_PTS	
				pkt.pts = pkt.dts =(int64_t)((pstStream->pstPack[i].u64PTS - fc->Video_PTS) *0.09+0.5);
			#endif
		#endif
		pkt.duration = 40;	
		pkt.duration = av_rescale_q(pkt.duration, Vpst->time_base, Vpst->time_base);
		pkt.pos = -1;			
		pkt.data = pPackVirtAddr ;	//接受视频数据NAUL
   		pkt.size = u32PackLen;		//视频数据长度
		
		//把AVpack包写入mp4
		ret = av_interleaved_write_frame(fc->g_OutFmt_Ctx, &pkt);
		if (ret < 0)
		{
		    printf("Muxing:cannot write video frame\n");
		    return HI_FAILURE;
		}
    }
	return HI_SUCCESS;
}

int HI_PDT_Init(void)
{
	int i;
	
	for (i = 0; i < VENC_CHN_NUM; i++)
	{
		memset(&ffmpegCtx[i], 0, sizeof(ffmpegCtx_t));
		HI_PDT_CreateMp4(i);
	}
	
	return 0;
}

int HI_PDT_Exit(void)
{
	int i;
	
	for (i = 0; i < VENC_CHN_NUM; i++)
	{
		HI_PDT_CloseMp4(i);
	}
		
	return 0;
}



int board_init_mp4()
{
	
}

void delete_current_mp4_file()
{
	ffmpegCtx_t * fc = GetVencChnCtx(0);
	unlink(fc->filename);

	return;
}