/*
 * databuffer.h
 *
 *  Created on: Nov 17, 2016
 *      Author: huajia
 */

#ifndef DATABUFFER_H_
#define DATABUFFER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>




typedef struct data_buffer
{
    char* data_buf;
    int buffer_size;
    int start_idx;
    int data_size;
    int reserve_type;
    int reserve_size;
    pthread_mutex_t buf_mutex;
}DATABUFFER;

int create_buffer(DATABUFFER *buf, int buf_size);
int destroy_buffer(DATABUFFER *buf);
int clear_buffer(DATABUFFER *buf);

char *get_free_buffer(DATABUFFER *buf, int get_size);
int use_free_buffer(DATABUFFER *buf, int used_size);

char *get_buffer(DATABUFFER *buf, int get_size);
int release_buffer(DATABUFFER *buf, int used_size);

int get_data_size(DATABUFFER *buf);


#if 0
int init_buffer(int buf_size);
int destroy_buffer();
int clear_buffer();

char *get_free_buffer(int get_size);
int use_free_buffer(int used_size);

char *get_buffer(int get_size);
int release_buffer(int used_size);

int get_data_size();
#endif 
#endif /* DATABUFFER_H_ */
