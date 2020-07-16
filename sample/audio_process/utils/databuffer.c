/*
 * databuffer.c
 *
 *  Created on: Nov 17, 2016
 *      Author: huajia
 */

#include <pthread.h>

#include "util.h"
#include "databuffer.h"

#define RESERVED_BUF_SIZE 	(16*1024*1024)

enum
{
	NOT_RESERVED = 0,
	RESERVED_DATA = 1,
	RESERVED_FREE = 2,
};

static int g_buffer_size;
static int g_start_idx;
static int g_data_size;
static int g_reserve_type;
static int g_reserve_size;
char *g_data_buffer = NULL;
pthread_mutex_t g_buf_mutex;

int init_buffer(int buf_size)
{
	if(g_data_buffer != NULL)
	{
		free(g_data_buffer);
		g_data_buffer = NULL;
	}
	g_data_buffer = (char *)malloc(buf_size+RESERVED_BUF_SIZE);
	memset(g_data_buffer, 0x0, buf_size);

	g_buffer_size = buf_size;
	g_start_idx = 0;
	g_data_size = 0;
	g_reserve_type = NOT_RESERVED;
	pthread_mutex_init(&g_buf_mutex, NULL);

	return 0;
}

int destroy_buffer()
{
	if(g_data_buffer != NULL)
	{
		free(g_data_buffer);
		g_data_buffer = NULL;
	}
	pthread_mutex_destroy(&g_buf_mutex);

	return 0;
}

int clear_buffer()
{
	if(g_data_buffer == NULL)
	{
		return -1;
	}

	pthread_mutex_lock(&g_buf_mutex);
	g_start_idx = 0;
	g_data_size = 0;
	memset(g_data_buffer, 0x0, g_buffer_size);
	pthread_mutex_unlock(&g_buf_mutex);

	return 0;
}

char *get_free_buffer(int get_size)
{
	pthread_mutex_lock(&g_buf_mutex);
	if(g_buffer_size-g_data_size < get_size)
	{
		pthread_mutex_unlock(&g_buf_mutex);
		return NULL;
	}

	int free_idx = (g_start_idx + g_data_size) % g_buffer_size;
	if(free_idx + get_size > g_buffer_size)
	{
		g_reserve_type = RESERVED_FREE;
		g_reserve_size = free_idx + get_size - g_buffer_size;
	}

	char *ptr = g_data_buffer + free_idx;

	pthread_mutex_unlock(&g_buf_mutex);

	return ptr;
}

int use_free_buffer(int used_size)
{
	pthread_mutex_lock(&g_buf_mutex);
	g_data_size += used_size;
	if(g_reserve_type == RESERVED_FREE)
	{
		memcpy(g_data_buffer, g_data_buffer+g_buffer_size, g_reserve_size);
		g_reserve_type = NOT_RESERVED;
	}

	pthread_mutex_unlock(&g_buf_mutex);

	return 0;
}

char *get_buffer(int get_size)
{
	pthread_mutex_lock(&g_buf_mutex);
	if(g_data_size < get_size)
	{
		pthread_mutex_unlock(&g_buf_mutex);
		return NULL;
	}

	if(g_start_idx + get_size > g_buffer_size)
	{
		g_reserve_type = RESERVED_DATA;
		g_reserve_size = g_start_idx + get_size - g_buffer_size;
		memcpy(g_data_buffer+g_buffer_size, g_data_buffer, g_reserve_size);
	}

	char *ptr = g_data_buffer + g_start_idx;
	pthread_mutex_unlock(&g_buf_mutex);

	return ptr;
}

int release_buffer(int used_size)
{
	pthread_mutex_lock(&g_buf_mutex);
	memset(g_data_buffer+g_start_idx, 0x0, used_size);
	g_start_idx += used_size;
	g_start_idx %= g_buffer_size;
	g_data_size -= used_size;
	if(g_data_size < 0)
	{
		g_data_size = 0;
	}
	if(g_reserve_type == RESERVED_DATA)
	{
		g_reserve_type = NOT_RESERVED;
	}
	pthread_mutex_unlock(&g_buf_mutex);

	return 0;
}

int get_data_size()
{
	return g_data_size;
}
