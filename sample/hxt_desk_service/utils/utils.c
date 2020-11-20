#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <dirent.h>
#include <unistd.h>

#include <curl/curl.h>
#include <cJSON.h>

#include "common.h"
#include "utils.h"
#include "db.h"


static char* separate_filename(const char *url)
{
    char * filename = NULL;
    if (NULL == url)
    {
        return NULL;
    }

    char *start = NULL;
    char *end = NULL;

    if ((start = strrchr(url, '/')) == NULL)
    {
        return NULL;
    }
    start ++;
    
    if ((end = strrchr(url, '?')) == NULL)
    {
        int length = (int)(url+strlen(url) - start);
        filename = (char*)utils_calloc(length+1);
        memcpy(filename, start, (url+strlen(url) - start));
    }
    else 
    {
        int length = (int)(end -start);
        filename = (char*)utils_calloc(length+1);
        memcpy(filename, start, (end - start));
    }
    //filename[strlen(filename)] = '\0';

    return filename;
}

static BOOL check_system_cmd_result(const pid_t status)
{
    pid_t res = (pid_t) status;
    if(-1 == res)
    {
        utils_print("system cmd execute failed.\n");
        return FALSE;
    }
    else
    {
        if(WIFEXITED(status))
        {
            if(0 == WEXITSTATUS(status))      
            {
                return TRUE;
            }  
        }
    }

    return FALSE;
}

char *utils_get_current_time()
{
    static char s[20] = {0};
    memset(s, 0, 20);
    time_t t;
    struct tm* ltime;
    
    time(&t);
    ltime = localtime(&t);
    strftime(s, 20, "%Y-%m-%d %H:%M:%S", ltime);

    return s;
}

BOOL utils_write_fifo(const int fd, const char* write_buffer, const int write_len, const int timeout_val)
{
    BOOL write_result = FALSE;
    int ret = -1;
    struct timeval time_out;
    fd_set write_fds;

    if(-1 == fd)
    {
        return FALSE;
    }

    while(1)
    {
        FD_ZERO(&write_fds);
        FD_SET(fd, &write_fds);

        time_out.tv_sec = timeout_val;
        time_out.tv_usec = 0;

        ret = select(fd + 1, NULL, &write_fds, NULL, &time_out);
        if (ret < 0)
        {
            utils_print("get select error\n");
            break;
        }
        else if (0 == ret) 
        {
            utils_print("get select write timeout\n");
            break;
        }
        if(FD_ISSET(fd, &write_fds))
        {
            write(fd, write_buffer, write_len);
            write_result = TRUE;
        }
    }
    
    return write_result;
}

BOOL utils_send_local_voice(const char *path)
{

    int ret = -1;

    if(NULL == path)
    {
        return FALSE;
    }

    int fd = utils_open_fifo(MP3_FIFO, O_WRONLY);
    if (-1 == fd)
    {
        return FALSE;
    }

    FILE *fp = fopen(path, "rb");
    if(NULL == fp)
    {
        return FALSE;
    }
    char buff[1024];
    int read_length = 0; 

    struct timeval time_out;
    fd_set write_fds;

    while(1)
    {
        FD_ZERO(&write_fds);
        FD_SET(fd, &write_fds);

        time_out.tv_sec = 1;
        time_out.tv_usec = 0;
        ret = select(fd + 1, NULL, &write_fds, NULL, &time_out);
        if (ret < 0)
        {
            utils_print("get MP3_FIFO select error\n");
            break;
        }
        else if (0 == ret) 
        {
            utils_print("get MP3_FIFO select write timeout\n");
            break;
        }
        if(FD_ISSET(fd, &write_fds))
        {
            memset(buff, 1024, 0);
            read_length = fread(buff, 1024, 1, fp);
            if(read_length <= 0)
            {
                break;
            }
            write(fd, buff, 1024); //block
        }
    }

    fclose(fp);
    close(fd);

    return TRUE;
}

BOOL utils_send_mp3_voice(const char *url)
{
    char cmd[256] = {0};
    sprintf(cmd, "curl --insecure -o %s %s", MP3_FIFO, url);

    pid_t status = system(cmd);
    
    return check_system_cmd_result(status);
}

static size_t save_download_data(void *ptr, size_t size, size_t nmemb, void *data)
{
    FILE* fp = (FILE*)data;
    if (fwrite(ptr, size, nmemb, fp) != size * nmemb)
    {
        utils_print("fwrite download file error\n");
    }
    
    return (size * nmemb);
}

#if 0
BOOL utils_download_file(const char *url, const char* save_file_path)
{
    if (NULL == url || NULL == save_file_path)
    {
        return FALSE;
    }

    char CMD_DOWNLOAD_FILE[512] = {0};
    sprintf(CMD_DOWNLOAD_FILE, "curl --insecure -o %s \"%s\"", save_file_path, url);
 
    utils_print("%s\n", CMD_DOWNLOAD_FILE);

    char out_buffer[1024] = {0};
    int buffer_length = 1024;

    FILE *fp = NULL;
    fp = popen(CMD_DOWNLOAD_FILE, "r");
    if(NULL != fp)
    {
        if(fgets(out_buffer, buffer_length, fp) == NULL)
        {
            pclose(fp);
            return FALSE;
        }
        // out_buffer[buffer_length-1] = '\0';
    }
    pclose(fp);

    return TRUE;
}
#endif

BOOL utils_download_file(const char* url, const char* save_file_path)
{
    BOOL ret = TRUE;
    if (NULL == url || NULL == save_file_path)
    {
        return FALSE;
    }

    FILE *fd = fopen(save_file_path, "w+");
    if (NULL == fd)
    {
        return FALSE;
    }

    CURLcode res;
    CURL* handle = curl_easy_init();
    if (handle)
    {
        struct curl_slist *header_list = NULL;
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, save_download_data);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*)fd);

        res = curl_easy_perform(handle);
        if (res != CURLE_OK)
        {
            utils_print("curl_easy_perform failed:%s\n", 
                curl_easy_strerror(res));
            ret = FALSE;
        }
        curl_easy_cleanup(handle);
    }

    fclose(fd);
    return ret;
}

static size_t get_response_data(void *ptr, size_t size, size_t nmemb, void *data)
{
    size_t written = size * nmemb;
    char *p = *(char**)data;
    *(char**)data = realloc(p, written + 1);
    p = *(char **)data;

    if (NULL == p)
    {
        utils_print("not enough memory\n");
        return -1;
    }

    strncpy(p, ptr, written);
    p[written] = '\0';

    return written;
}

#if 0
BOOL utils_upload_file(const char* url, const char* header, const char* local_file_path,
                        char* out_buffer, int buffer_length)
{
    if(NULL == url || NULL == local_file_path || NULL == out_buffer)
    {
        return FALSE;
    }

    char CMD_UPLOAD_FILE[1024] = {0};

    sprintf(CMD_UPLOAD_FILE, "curl --insecure --retry 3 -H \"%s\" -F \"file=@%s\" %s", header, local_file_path, url);
    utils_print("upload cmd: [%s]\n", CMD_UPLOAD_FILE);

    FILE *fp = NULL;
    fp = popen(CMD_UPLOAD_FILE, "r");
    if(NULL != fp)
    {
        if(fgets(out_buffer, buffer_length, fp) == NULL)
        {
            pclose(fp);
            return FALSE;
        }
        // out_buffer[buffer_length-1] = '\0';
        // utils_print("upload file:%s\n", out_buffer);
    }
    pclose(fp);
        
    return TRUE;
}
#endif 

BOOL utils_upload_file(const char* url, const char* header, const char* local_file_path, char **out)
{
    BOOL ret = TRUE;
    
    if(NULL == url || NULL == local_file_path)
    {
        return FALSE;
    }

    char* extra_header = (char *)header;
    if (NULL == extra_header)
    {
        extra_header = "";
    }

    FILE *fd = fopen(local_file_path, "rb");
    if (NULL == fd)
    {
        return FALSE;
    }

    struct stat file_info;
    if(fstat(fileno(fd), &file_info) != 0)
    {
        fclose(fd);
        return FALSE;
    }

    CURLcode res;
    CURL* handle = curl_easy_init();
    if (handle)
    {
        struct curl_slist *header_list = NULL;
        header_list = curl_slist_append(header_list, extra_header);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header_list);
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(handle, CURLOPT_READDATA, fd);
        curl_easy_setopt(handle, CURLOPT_INFILESIZE_LARGE,(curl_off_t)file_info.st_size);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, get_response_data);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*)out);

        res = curl_easy_perform(handle);
        if (res != CURLE_OK)
        {
            utils_print("curl_easy_perform failed:%s\n", 
                curl_easy_strerror(res));
            ret = FALSE;
        }

        curl_easy_cleanup(handle);
        curl_slist_free_all(header_list);
    }

    return ret;
}

#if 0
BOOL utils_post_json_data(const char *url, const char* header_content, const char* json_data, char* out, int out_length)
{

    if (NULL == url || NULL == out)
    {
        return FALSE;
    }

    char CMD_POST_JSON[1024] = {0};
    char* extra_header = (char *)header_content;
    if( NULL == extra_header)
    {
        extra_header = "";
    }
    char* data = (char*)json_data;
    if(NULL == data)
    {
        data = "";
    }

    sprintf(CMD_POST_JSON, "curl --insecure -X POST -H \"Content-Type:application/json;charset=UTF-8\" -H \"%s\" -d \'%s\' %s", 
                                extra_header, data, url);

    FILE *fp = NULL;
    fp = popen(CMD_POST_JSON, "r");
    if(NULL != fp)
    {
        if(fgets(out, out_length, fp) == NULL)
        {
            pclose(fp);
            return FALSE;
        }
    }
    pclose(fp);

    return TRUE;
}
#endif 

BOOL utils_post_json_data(const char* url, const char* header_content, const char* json_data, char **out)
{
    BOOL ret = TRUE;
    if (NULL == url)
    {
        return FALSE;
    }

    char* extra_header = (char *)header_content;
    if( NULL == extra_header)
    {
        extra_header = "";
    }

    char* data = (char*)json_data;
    if(NULL == data)
    {
        data = "";
    }

    CURLcode res;
    CURL* handle = curl_easy_init();
    if (handle)
    {
        struct curl_slist *header_list = NULL;
        header_list = curl_slist_append(header_list, "Content-Type:application/json;charset=UTF-8");
        header_list = curl_slist_append(header_list, extra_header);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header_list);
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(handle, CURLOPT_POST, 1);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, strlen(json_data));
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, get_response_data);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*)out);

        res = curl_easy_perform(handle);
        if (res != CURLE_OK)
        {
            utils_print("curl_easy_perform failed:%s\n", 
                curl_easy_strerror(res));
            ret = FALSE;
        }

        curl_easy_cleanup(handle);
        curl_slist_free_all(header_list);
    }

    return ret;
}

#if 0
BOOL utils_send_get_request(const char* url, const char* header_content, char* out, int out_length)
{
    if (NULL == url || NULL == out)
    {
        return FALSE;
    }

    char* extra_header = (char *)header_content;
    if( NULL == extra_header)
    {
        extra_header = "";
    }

    char CMD_GET_REQ[1024] = {0};
    sprintf(CMD_GET_REQ, "curl --insecure -H \"Content-Type:application/json;charset=UTF-8\" -H \"%s\" %s", extra_header, url);
    utils_print("GET request: [%s]\n", CMD_GET_REQ);                            

    FILE *fp = NULL;
    fp = popen(CMD_GET_REQ, "r");
    if(NULL != fp)
    {
        if(fgets(out, out_length, fp) == NULL)
        {
            pclose(fp);
            return FALSE;
        }
    }
    pclose(fp);

    return TRUE;
}
#endif 

// static size_t get_alioss_response_data(void *ptr, size_t size, size_t nmemb, char *data)
// {
//     size_t written = size * nmemb;
//     char *p = *(char**)data;
//     *(char**)data = realloc(p, written + 1);
//     p = *(char **)data;

//     if (NULL == p)
//     {
//         utils_print("not enough memory\n");
//         return -1;
//     }

//     strncpy(p, ptr, written);
//     p[written] = '\0';

//     return written;
// }

BOOL utils_send_get_request(const char* url, const char* header_content, char** out)
{
    BOOL ret = TRUE;
    if (NULL == url)
    {
        return FALSE;
    }

    char* extra_header = (char *)header_content;
    if( NULL == extra_header)
    {
        extra_header = "";
    }

    CURLcode res;
    CURL* handle = curl_easy_init();
    if (handle)
    {
        struct curl_slist *header_list = NULL;
        header_list = curl_slist_append(header_list, "Content-Type:application/json;charset=UTF-8");
        header_list = curl_slist_append(header_list, extra_header);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header_list);
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, get_response_data);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*)out);

        res = curl_easy_perform(handle);
        if (res != CURLE_OK)
        {
            utils_print("curl_easy_perform failed:%s\n", 
                curl_easy_strerror(res));
            ret = FALSE;
        }

        curl_easy_cleanup(handle);
        curl_slist_free_all(header_list);
    }

    return ret;
}

char* utils_date_to_string()
{
    static char str_date[16] = {0};
    time_t t;
    struct tm* ltime;
    
    time(&t);
    ltime = localtime(&t);
    strftime(str_date, 16, "%Y-%m-%d", ltime);
    return str_date;
}

char* utils_time_to_string()
{
    static char str_time[20] = {0};
    time_t t;
    struct tm* ltime;
    
    time(&t);
    ltime = localtime(&t);
    strftime(str_time, 20, "%Y-%m-%d %H:%M:%S", ltime);
    return str_time;
}

int utils_change_time_format(const char* str_time) 
{
    if (NULL == str_time)
    {
        return 0;
    }
    struct tm t;
    char buf[255] = {0};
    memset(&t, 0, sizeof(struct tm));
    strptime(str_time, "%Y-%m-%d %H:%M:%S", &t);
    time_t tt = mktime(&t);

    return (int)tt;
}

char* utils_get_file_md5sum(const char* file_name)
{
    // md5sum demo_config.sh | awk -F " " '{print $1}'
    if(NULL == file_name)
    {
        return NULL;
    }
    char CMD_GET_MD5[256] = {0};
    sprintf(CMD_GET_MD5, "md5sum %s|awk -F \" \" \'{print $1}\'", file_name);
    static char str_md5[64] = {0};
    char line[64] = {0};
    FILE *fp = NULL;
    fp = popen(CMD_GET_MD5, "r");
    if(NULL != fp)
    {
        if(fgets(line, sizeof(line), fp) == NULL)
        {
            pclose(fp);
            return NULL;
        }
        // line[strlen(line)] = '\0';
    }

    pclose(fp);
    strcpy(str_md5, line);

    return str_md5;
}

unsigned long utils_get_file_size(const char* path)
{
    unsigned long file_size = 0;
    FILE *fp = NULL;
    
    if(NULL == path)
    {
        return 0;
    }

    fp = fopen(path, "r");
    if(NULL == fp)
    {
        return file_size;
    }
    fseek(fp, 0L, SEEK_END);
    file_size = ftell(fp);
    fclose(fp);

    return file_size;
}

int utils_split_file_to_chunk(const char* path)
{
    int chunk_count = 0;
    if(NULL == path)
    {
        return 0;;
    }
    //separate file name with suffix
    char* full_name = separate_filename(path);
    if(NULL == full_name)
    {
        return 0;
    }
    printf("full name is %s\n", full_name);
    char* p = strchr(full_name, '.');
    char file_name[64] = {0};
    memcpy(file_name, full_name, strlen(p));
    printf("file name is %s\n", file_name);
    char* suffix = strrchr(full_name, '.');
    if(NULL == suffix)
    {
        return 0;
    }
    suffix ++;
    printf("suffix name is %s\n", suffix);

    char CMD_CHUNK_DIR[256] = {0};
    sprintf(CMD_CHUNK_DIR, "mkdir -p /userdata/chunk/%s_%s", file_name, suffix);
    char CMD_SPLIT_FILE[256] = {0};
    sprintf(CMD_SPLIT_FILE, "split -b 5m %s -a 3 /userdata/chunk/%s_%s/%s_%s_", path, file_name, suffix, file_name, suffix);
    char CMD_COUNT_CHUNKS[256] = {0};
    sprintf(CMD_COUNT_CHUNKS, "ls -l /userdata/chunk/%s_%s|wc -l", file_name, suffix);

    system(CMD_CHUNK_DIR);
    system(CMD_SPLIT_FILE);
    
    char line[64] = {0};
    FILE *fp = NULL;
    fp = popen(CMD_COUNT_CHUNKS, "r");
    if(NULL != fp)
    {
        if(fgets(line, sizeof(line), fp) == NULL)
        {
            pclose(fp);
            return 0;
        }
        chunk_count = atoi(line);
    }
    pclose(fp);
    
    //modify chunk file index 
    char CMD_CHUNK_SERIAL[256] = {0};
    sprintf(CMD_CHUNK_SERIAL, "sh /userdata/bin/script/modify_chunk_names.sh %s_%s", file_name, suffix);
    system(CMD_CHUNK_SERIAL);

    return chunk_count;
}

int utils_open_fifo(const char* name, int mode)
{
    int fd = -1;
    char* fifo_name = (char *)name;
    if(NULL == fifo_name)
    {
        return fd;
    }

    if (access(fifo_name, F_OK) == -1)
    {
        int res = mkfifo(fifo_name, 0666);
        if(res != 0)
        {
            utils_print("could not create fifo %s\n", fifo_name);
            return -1;
        }
    }
    fd = open(fifo_name, mode);

    return fd; 
}

void utils_link_wifi(const char* ssid, const char* pwd)
{
    if(NULL == ssid || NULL == pwd)
    {
        return;
    }
    // utils_disconnect_wifi();
    // sleep(10);

    system("ifconfig wlan0 up");

    char CMD_SAVE_WIFI_INFO[256] = {0};
    sprintf(CMD_SAVE_WIFI_INFO, "wpa_passphrase %s %s > %s", ssid, pwd, WIFI_CFG);
    system(CMD_SAVE_WIFI_INFO);

    char CMD_LINK_WIFI[256] = {0};
    sprintf(CMD_LINK_WIFI, "nohup wpa_supplicant -B -c %s -iwlan0 > /dev/null 2>&1 &", WIFI_CFG);
    system(CMD_LINK_WIFI);

    char CMD_DHCP_IP[256] = {0};
    sprintf(CMD_DHCP_IP, "nohup udhcpc -b -i wlan0 > /dev/null 2>&1 &");
    system(CMD_DHCP_IP);

    return;
}

void utils_disconnect_wifi()
{
    system("kill -9 $(pidof wpa_supplicant)");
    system("kill -9 $(pidof udhcpc)");
    system("ifconfig wlan0 down");
}

BOOL utils_check_wifi_state()
{
    char line[64] = {0};
    FILE *fp = NULL;
    int link_mode = 0;
    
    fp = popen("cat /sys/class/net/wlan0/operstate", "r");
    if(NULL != fp)
    {
        if(fgets(line, sizeof(line), fp) == NULL)
        {
            pclose(fp);
            return FALSE;
        }
        line[2] = '\0';
    }
    pclose(fp);
    if (strcmp("up", line) == 0)
    {
        return TRUE;
    }

    return FALSE;
}

void utils_system_reboot()
{
    system("reboot");
}

void utils_system_reset()
{
    /* remove child study data, include video and snap */
    system("rm -rf /user/*");
    /* remove wifi config */
    system("rm /userdata/config/wifi.conf");
    /* deinit database data */
    deinit_hxt_service_db();

    return;
}

void utils_generate_mp4_file_name(char* file_name)
{
    if( NULL == file_name)
    {
        return ;
    }

	struct tm * tm;
	time_t now = time(0);
	tm = localtime(&now);
	
	snprintf(file_name, 128, "%04d%02d%02d-%02d%02d%02d.mp4", 
    	     tm->tm_year + 1900,
	         tm->tm_mon + 1,
	         tm->tm_mday,
	         tm->tm_hour,
	         tm->tm_min,
	         tm->tm_sec);
	
    return;
}

int utils_query_file_count(char* path)
{
    int file_count = 0;

    char CMD_COUNT_IN_PATH[256] = {0};
    sprintf(CMD_COUNT_IN_PATH, "ls %s|wc -l", path);
    
    char line[64] = {0};
    FILE *fp = NULL;
    fp = popen(CMD_COUNT_IN_PATH, "r");
    if(NULL != fp)
    {
        if(fgets(line, sizeof(line), fp) == NULL)
        {
            pclose(fp);
            return 0;
        }
        file_count = atoi(line);
    }
    pclose(fp);

    return file_count;
}

void utils_query_file_names(const char *path, char **file_list)
{
    DIR *dir;
    struct dirent *ptr;
    int idx = 0;

    if (NULL == file_list)
    {
        return;
    }

    if ((dir=opendir(path)) == NULL)
    {
        utils_print("Open %s error...\n", path);
        return;
    }

    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)
        {
            continue;
        }
        else if(ptr->d_type == 8)    ///file
        {
	        strcpy(file_list[idx], path);
            strcat(file_list[idx++], ptr->d_name);
	    }
        else 
        {
	        continue;
        }
    }
    closedir(dir);

    return 0;
}

void utils_save_yuv_test(const char* yuv_data, const int width, const int height)
{
    struct tm * tm;
	time_t now = time(0);
	tm = localtime(&now);
	char file_name[128] = {0};

	snprintf(file_name, 128, "%04d%02d%02d-%02d%02d%02d.yuv", 
    	     tm->tm_year + 1900,
	         tm->tm_mon + 1,
	         tm->tm_mday,
	         tm->tm_hour,
	         tm->tm_min,
	         tm->tm_sec);

    FILE* pfd = fopen(file_name, "wb");
    fwrite(yuv_data, width * height * 3 / 2, 1, pfd);
    fflush(pfd);
    fclose(pfd);
}

void utils_save_pcm_test(const char* pcm_data, int length)
{
    struct tm * tm;
	time_t now = time(0);
	tm = localtime(&now);
	char file_name[128] = {0};

	snprintf(file_name, 128, "%04d%02d%02d-%02d%02d%02d.pcm", 
    	     tm->tm_year + 1900,
	         tm->tm_mon + 1,
	         tm->tm_mday,
	         tm->tm_hour,
	         tm->tm_min,
	         tm->tm_sec);

    FILE* pfd = fopen(file_name, "wb");
    fwrite(pcm_data, 1, length, pfd);
    fflush(pfd);
    fclose(pfd);
}

