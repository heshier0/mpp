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

int init_buffer(int buf_size);
int destroy_buffer();
int clear_buffer();

char *get_free_buffer(int get_size);
int use_free_buffer(int used_size);

char *get_buffer(int get_size);
int release_buffer(int used_size);

int get_data_size();

#endif /* DATABUFFER_H_ */
