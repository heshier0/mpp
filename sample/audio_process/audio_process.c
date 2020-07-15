
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "utils.h"


int main(int argc, char **argv)
{
    utils_print("HXT V1.0.0\n");
   
    /* init hisi board */
    // board_init_sys();
   
    /* connect to hxt server */
    hxt_load_cfg();
    if (hxt_get_token_cfg() == NULL)
    {
        utils_print("send request to get token\n");
        hxt_get_token_request();
    }

    pid_t hxt_pid = fork();
    if (hxt_pid == 0)
    {
        hxt_websocket_start();
        return 0;
    }

    pid_t iflyos_pid = fork();
    if (iflyos_pid == 0)
    {
        iflyos_websocket_start();
        return 0;
    }

    int st1, st2, st3;
    waitpid(hxt_pid, &st1, 0);
    waitpid(iflyos_pid, &st2, 0);

    utils_print("~~~~EXIT~~~~\n");
    
    // board_deinit_sys();

    hxt_unload_cfg();
    
    
    return 0;
}
