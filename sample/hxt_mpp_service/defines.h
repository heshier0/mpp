#ifndef __defines_h__
#define __defines_h__

#define utils_print(format, ...) printf("%s>>>%d: " format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define malloc_print(__ptr__,size) printf("[ALLOC] %32s:%4d | addr= %p, size= %lu, expr= `%s`\n", __FUNCTION__, __LINE__ , __ptr__, size, #size)
#define free_print(ptr)	printf("[ FREE] %32s:%4d | addr= %p, expr= `%s`\n", __FUNCTION__, __LINE__, ptr, #ptr)

// #define malloc_print(__ptr__,size)
// #define free_print(ptr)
// #define utils_print(format, ...)


#define utils_malloc(size) ({ \
	void *__ptr__ = malloc(size); \
	memset(__ptr__,0,size); \
	__ptr__; \
	})

#define utils_calloc(size) ({ \
	void *__ptr__ = calloc(size, 1); \
	__ptr__; \
	})

#define utils_free(ptr) ({ \
	free(ptr); \
	})

typedef enum
{
    TRUE  = 1, 
    FALSE  = 0
}BOOL;

#endif // ! __defines_h__


