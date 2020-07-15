#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>

#include "sample_comm.h"

#include "utils.h"

#define PLAY_LENGTH     1152
#define SAMPLE_LENGTH   640
#define PCM_FIFO    "/tmp/my_pcm_fifo"


void board_play_mp3_file(const char* path)
{
    int ret;
    if(NULL == path)
    {
        return;
    }

    AUDIO_STREAM_S audio_stream;
    ADEC_CHN adec_chn = 0;
    FILE *pfd = NULL;
    int read_len = 0;
    char* pstream = NULL;

    pfd = fopen(path, "rb");
    if(NULL == pfd)
    {
        utils_print("mp3 file not exist\n");
        return;
    }

    board_init_audio_out();
    pstream = (char*)utils_malloc(sizeof(char) * MAX_AUDIO_STREAM_LEN);
    if (NULL == pstream)
    {
        return;
    }

    while(1)
    {
        audio_stream.pStream = pstream;
        read_len = fread(audio_stream.pStream, sizeof(char), PLAY_LENGTH, pfd);
        if (read_len > 0)
        {
            utils_print("read %d bytes from %s\n", read_len, path);
            audio_stream.u32Len = read_len;
            ret = HI_MPI_ADEC_SendStream(adec_chn, &audio_stream, HI_TRUE);
            if (HI_SUCCESS != ret)
            {
                utils_print("HI_MPI_ADEC_SendStream(%d) failed with %#x!\n", adec_chn, ret);
                break;
            }
        }
        else
        {
            utils_print("read end...\n");
            ret = HI_MPI_ADEC_SendEndOfStream(adec_chn, HI_FALSE);
            if (HI_SUCCESS != ret)
            {
                utils_print("HI_MPI_ADEC_SendEndOfStream failed!\n");
            }
            break;
        }
    }

    if(pstream != NULL)
    {
        utils_free(pstream);
        pstream = NULL;
    }

    if(pfd != NULL)
    {
        fclose(pfd);
    }

    board_deinit_audio_out();

    return;
}

void board_play_mp3_fifo(BOOL playing)
{
    AUDIO_STREAM_S audio_stream;
    ADEC_CHN adec_chn = 0;
    int fd = -1;
    int ret;
    unsigned char* pstream = NULL;
    
    const char* fifo_mp3 = "/tmp/my_mp3_fifo";
    if (access(fifo_mp3, F_OK) == -1)
    {
        int res = mkfifo(fifo_mp3, 0777);
        if(res != 0)
        {
            printf("could not create fifo %s\n", fifo_mp3);
            return;
        }
    }
    fd = open(fifo_mp3,  O_RDONLY | O_NONBLOCK);
    if(-1 == fd)
    {
        utils_print("mp3 file not exist\n");
        return;
    }

    board_init_audio_out();
    pstream = (unsigned char*)utils_malloc(sizeof(unsigned char) * MAX_AUDIO_STREAM_LEN);
    if (NULL == pstream)
    {
        return;
    }

    while(playing)
    {
        audio_stream.pStream = pstream;
        unsigned int read_len = read(fd, audio_stream.pStream, PLAY_LENGTH);
        if (read_len <= 0)
        {
            ret = HI_MPI_ADEC_SendEndOfStream(adec_chn, HI_FALSE);
            if (HI_SUCCESS != ret)
            {
                utils_print("HI_MPI_ADEC_SendEndOfStream failed!\n");
            }

            sleep(1);
        }
        audio_stream.u32Len = read_len;
        ret = HI_MPI_ADEC_SendStream(adec_chn, &audio_stream, HI_TRUE);
        if (HI_SUCCESS != ret)
        {
            utils_print("HI_MPI_ADEC_SendStream(%d) failed with %#x!\n", adec_chn, ret);
            break;
        }
    }

    utils_print("play thread exit...\n");

    if(pstream != NULL)
    {
        utils_free(pstream);
        pstream = NULL;
    }
    board_deinit_audio_out();

    close(fd);

    

    return;
}

void board_sample_pcm_fifo(BOOL sampling)
{
    int ret;
    int  aenc_fd = -1;
    int aenc_chn = 0;
    int adec_chn = 0;
    AUDIO_STREAM_S stStream;
    fd_set read_fds;
    struct timeval TimeoutVal;

    //added by hekai
    if (access(PCM_FIFO, F_OK) == -1)
    {
    int res = mkfifo(PCM_FIFO, 0777);
    if(res != 0)
    {
        printf("could not create fifo %s\n", PCM_FIFO);
        return;
    }
    }
    int fd = open(PCM_FIFO, O_WRONLY);
    if (fd == -1)
    {
        printf("open pcm fifo failed\n");
        return;
    }
    //end added

    FD_ZERO(&read_fds);
    aenc_fd = HI_MPI_AENC_GetFd(aenc_chn);
    FD_SET(aenc_fd, &read_fds);
           
    int test_file_fd = open("/tmp/test.pcm", "wb+");

    while (sampling)
    {
        TimeoutVal.tv_sec = 1;
        TimeoutVal.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_SET(aenc_fd, &read_fds);

        ret = select(aenc_fd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (ret < 0)
        {
            break;
        }
        else if (0 == ret)
        {
            printf("%s: get aenc stream select time out\n", __FUNCTION__);
            break;
        }

        if (FD_ISSET(aenc_fd, &read_fds))
        {
            /* get stream from aenc chn */
            ret = HI_MPI_AENC_GetStream(aenc_chn, &stStream, HI_FALSE);
            if (HI_SUCCESS != ret )
            {
                printf("%s: HI_MPI_AENC_GetStream(%d), failed with %#x!\n", \
                       __FUNCTION__, aenc_chn, ret);
                return;
            }

            /* send stream to decoder and play for testing */
            if (0)
            {
                ret = HI_MPI_ADEC_SendStream(adec_chn, &stStream, HI_TRUE);
                if (HI_SUCCESS != ret )
                {
                    printf("%s: HI_MPI_ADEC_SendStream(%d), failed with %#x!\n", \
                           __FUNCTION__, adec_chn, ret);
                    return;
                }
            }

            /* save audio stream to file */

            if(test_file_fd != -1)
            {
                (HI_VOID)write(test_file_fd, stStream.pStream, stStream.u32Len);
            }

            //modified by hekai
            int write_count = write(fd, stStream.pStream, stStream.u32Len);
            printf("write %d bytes to fifo\n", write_count);
            //end modified 

RELEASE_STREAM:
            /* finally you must release the stream */
            ret = HI_MPI_AENC_ReleaseStream(aenc_chn, &stStream);
            if (HI_SUCCESS != ret )
            {
                printf("%s: HI_MPI_AENC_ReleaseStream(%d), failed with %#x!\n", \
                       __FUNCTION__, aenc_chn, ret);
                return;
            }
        }
    }

    close(test_file_fd);

    close(fd);

    return;
}