#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "file_utils.h"

int get_file_count(const char* root)
{
    DIR *dir;
    struct dirent* ptr;
    char path[256] = {0};
    int total = 0;

    if(NULL == root)
    {
        return -1;
    }

    dir = opendir(root);
    if (NULL == dir)
    {
        printf("fail to open dir %s\n", root);
        return -1;
    }

    while ( (ptr = readdir(dir)) != NULL )
    {
        if(strcmp(ptr->d_name, ".") == 0 ||
            strcmp(ptr->d_name, "..") == 0)
        {
           continue; 
        }
    printf("222222\n");

        if (ptr->d_type == DT_DIR)
        {
            memset(path, 0, sizeof(path));
            strcpy(path, root);
            strcat(path, "/");
            strcat(path, ptr->d_name);
            total += get_file_count(path);
        }
    printf("333333333333\n");

        if (ptr->d_type == DT_REG)
        {
            total ++;
        }
    }

    closedir(dir);

    return total;
}   