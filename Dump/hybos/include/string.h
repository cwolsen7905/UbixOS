#ifndef __TL_STRING_H
#define	__TL_STRING_H

#ifdef __cplusplus
extern "C"
{
#endif

/*#include <_size_t.h>*/
/*#include <_null.h>*/
#include <stddef.h>

void *memcpy(void *dst_ptr, const void *src_ptr, size_t count);
void *memsetw(void *dst, int val, size_t count);
size_t strlen(const char *str);
int strcmp(const char * src, const char * dst);
int strncmp(const char * first, const char * last, size_t count);
char *strcpy(char *s, const char *t);
char *strncpy(char * dest, const char * source, size_t count);

#ifdef __cplusplus
}
#endif

#endif

