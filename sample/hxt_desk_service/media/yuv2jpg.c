
#include <sys/types.h>
#include <sys/stat.h>

#include <libavformat/avformat.h>
#include <libswscale/swscale.h>


#include "utils.h"

typedef struct jpg_context
{
    AVCodecContext *decoder;
    AVCodecContext *jpg_encoder;
    AVFrame *frame;
    AVPacket pkt;
    int width;
    int height;
    char src_uri[1024];
    char dst_uri[1024];
    char token[1024];
}JPG_CTX_S;

static int create_jpg_encoder(JPG_CTX_S *ctx)
{
    AVCodec *codec = NULL;
    AVCodecContext *c = NULL;

    av_register_all();

    if(ctx->width == 0 || ctx->height == 0)
    {
        utils_print("width or height error\n");
        return -1;
    }

    codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!codec)
    {
        utils_print("codec not found\n");
        return -1;
    }
    
    c = avcodec_alloc_context3(codec);
    if (!c)
    {
        utils_print("AVCodecContext alloc not success\n");
        return -1;
    }

    c->width = ctx->width;
    c->height = ctx->height;
    c->pix_fmt = AV_PIX_FMT_YUVJ420P;
    c->time_base = (AVRational){1, 25};

    if(avcodec_open2(c, codec, NULL) < 0)
    {
        utils_print("could not open codec\n");
        goto error;
    }

    ctx->jpg_encoder = c;
    return 0;
error:
    if(c)
    {
        avcodec_free_context(&c);
    }    
    utils_print("init jpg encoder error\n");
    return -1;
}

static int save_jpg(char *dst_uri, unsigned char* data, int size)
{
    FILE *out_file = NULL;
    out_file = fopen(dst_uri, "w+");
    if(NULL == out_file)
    {
        utils_print("open jpg file failed\n");
        return -1;
    }
    int ret = fwrite(data, 1, size, out_file);
    if( ret != size) 
    {
        utils_print("write jpg picture failed\n");
        fclose(out_file);
        return -1;
    }
    fclose(out_file);

    return 0;
}

static int yuv2jpg(JPG_CTX_S *ctx)
{
    int ret = 0;
    int got_packet = 0;
    ret = avcodec_encode_video2(ctx->jpg_encoder, &ctx->pkt, ctx->frame, &got_packet);
    if (ret < 0 || got_packet == 0)
    {
        utils_print("adt encoder video failed\n");
        return -1;
    }
    else if (got_packet)
    {
        save_jpg(ctx->dst_uri, ctx->pkt.data, ctx->pkt.size);
    }

    return 0;
}

static int destroy_context(JPG_CTX_S *ctx)
{
    av_free_packet(&ctx->pkt);
    if(ctx->frame)
    {
        av_frame_free(&(ctx->frame));
    }
    if(ctx->jpg_encoder)
    {
        avcodec_flush_buffers(ctx->jpg_encoder);
        avcodec_free_context(&ctx->jpg_encoder);
        av_free(ctx->decoder);
    }
    if(ctx->decoder)
    {
        avcodec_flush_buffers(ctx->decoder);
        avcodec_free_context(&ctx->decoder);
        av_free(ctx->decoder);
    }

    av_free(ctx);
    ctx = NULL; 
    return 0;
}

void trans_yuv2jpg(char* src_uri, char* dst_uri, int width, int height)
{
    int ret;
    JPG_CTX_S* ctx = (JPG_CTX_S* )malloc(sizeof(JPG_CTX_S));
    memset(ctx, 0, sizeof(JPG_CTX_S));
    ctx->width = width;
    ctx->height = height;
    sprintf(ctx->src_uri, "%s", src_uri);
    sprintf(ctx->dst_uri, "%s", dst_uri);
    sprintf(ctx->token, "%s", "biubiubiu");

    ret = create_jpg_encoder(ctx);

    if(ret < 0)
        return;
    char* data_buffer = (char* )malloc(width * height * 3 / 2);
    memset(data_buffer, 0, width * height * 3 / 2);

    FILE* fp = fopen(src_uri, "rb");
    fread(data_buffer, 1, width*height*3/2, fp);
    fclose(fp);

    ctx->frame = av_frame_alloc();
    av_init_packet(&ctx->pkt);
    ctx->frame->data[0] = (uint8_t* )malloc(width * height);
    memcpy(ctx->frame->data[0], data_buffer, width * height);
    ctx->frame->data[1] = (uint8_t* )malloc(width * height / 4);
    memcpy(ctx->frame->data[1], data_buffer + width * height, width * height / 4);
    ctx->frame->data[2] = (uint8_t* )malloc(width * height / 4);
    memcpy(ctx->frame->data[2], data_buffer + width * height + width * height / 4, width * height / 4);

    ctx->frame->linesize[0] = width;
    ctx->frame->linesize[1] = width / 2;
    ctx->frame->linesize[2] = width / 2;
    ctx->frame->width = width;
    ctx->frame->height = height;

    ctx->frame->format = AV_PIX_FMT_YUV420P;

    yuv2jpg(ctx);
    //end
    free(data_buffer);
    destroy_context(ctx);
}


