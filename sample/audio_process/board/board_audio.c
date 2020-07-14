#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "sample_comm.h"

#include "utils.h"

#define PLAY_LENGTH     1024
#define SAMPLE_LENGTH   640

static ADEC_CHN adec_chn = 0;

void board_play_mp3_file(const char* path)
{
    int ret;
    if(NULL == path)
    {
        return;
    }

    AUDIO_STREAM_S audio_stream;
    FILE *pfd = NULL;
    int read_len = 0;
    char* pstream = NULL;

    pfd = fopen(path, "rb");
    if(NULL == pfd)
    {
        utils_print("mp3 file not exist\n");
        return;
    }

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

    return;
}

void board_play_mp3_fifo()
{
    utils_print("adec_chn is %d\n", adec_chn);

    AUDIO_STREAM_S audio_stream;
    int fd = -1;
    int ret;
    char* pstream = NULL;
    
    const char* fifo_mp3 = "/tmp/my_mp3_fifo";
    if (access(fifo_mp3, F_OK) == -1)
    {
        int res = mkfifo(fifo_mp3, 0777);
        if(res != 0)
        {
            printf("could not create fifo %s\n", fifo_mp3);
            return -1;
        }
    }
    fd = open(fifo_mp3,  O_RDONLY | O_NONBLOCK);
    if(-1 == fd)
    {
        utils_print("mp3 file not exist\n");
        return;
    }

    pstream = (char*)malloc(sizeof(char) * MAX_AUDIO_STREAM_LEN);
    if (NULL == pstream)
    {
        return;
    }

    BOOL played = FALSE;
    while(1)
    {
        audio_stream.pStream = pstream;
        int read_len = read(fd, audio_stream.pStream, SAMPLE_LENGTH);
        if (read_len > 0)
        {
            audio_stream.u32Len = read_len;
            utils_print("send to adec...\n");
            ret = HI_MPI_ADEC_SendStream(adec_chn, &audio_stream, HI_TRUE);
            if (HI_SUCCESS != ret)
            {
                utils_print("HI_MPI_ADEC_SendStream(%d) failed with %#x!\n", adec_chn, ret);
                break;
            }
            utils_print("OK!\n");
            played = TRUE;
        }
        else
        {
            if(!played)
            {
                sleep(1);
                continue;
            }

            utils_print("read end...\n");
            ret = HI_MPI_ADEC_SendEndOfStream(adec_chn, HI_FALSE);
            if (HI_SUCCESS != ret)
            {
                utils_print("HI_MPI_ADEC_SendEndOfStream failed!\n");
            }
            utils_print("over!\n");
            played = FALSE;
        }
    }

    if(pstream != NULL)
    {
        free(pstream);
        pstream = NULL;
    }

    close(fd);

    return;
}
