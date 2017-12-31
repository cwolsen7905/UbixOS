/**************************************************************************************
 Copyright (c) 2002 The UbixOS Project
 All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions, the following disclaimer and the list of authors.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions, the following disclaimer and the list of authors
in the documentation and/or other materials provided with the distribution. Neither the name of the UbixOS Project nor the names of its
contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id: string.h 88 2016-01-12 00:11:29Z reddawg $

**************************************************************************************/

#ifndef _STRING_H
#define _STRING_H

#include <sys/cdefs.h>
#include <sys/_null.h>
#include <sys/_types.h>
#include <strings.h>

#ifndef _SIZE_T_DECLARED
typedef __size_t        size_t;
#define _SIZE_T_DECLARED
#endif

void    *memccpy(void * __restrict, const void * __restrict, int, size_t);
int      memcmp(const void *, const void *, size_t);
void    *memcpy(void * __restrict, const void * __restrict, size_t);
void    *memmove(void *, const void *, size_t);
void    *memset(void *, int, size_t);
char    *stpcpy(char *, const char *);
char    *strcasestr(const char *, const char *);
char    *strcat(char * __restrict, const char * __restrict);
char    *strchr(const char *, int);
int      strcmp(const char *, const char *);
char    *strcpy(char * __restrict, const char * __restrict);
size_t   strcspn(const char *, const char *);
char    *strdup(const char *);
char    *strerror(int);
int      strerror_r(int, char *, size_t);
size_t   strlcat(char *, const char *, size_t);
size_t   strlcpy(char *, const char *, size_t);
size_t   strlen(const char *);
void     strmode(int, char *);
char    *strncat(char * __restrict, const char * __restrict, size_t);
int      strncmp(const char *, const char *, size_t);
char    *strncpy(char * __restrict, const char * __restrict, size_t);
char    *strnstr(const char *, const char *, size_t);
char    *strpbrk(const char *, const char *);
char    *strrchr(const char *, int);
char    *strsep(char **, const char *);
size_t   strspn(const char *, const char *);
char    *strstr(const char *, const char *);
char    *strtok(char * __restrict, const char * __restrict);
char    *strtok_r(char *, const char *, char **);
void     swab(const void *, void *, size_t);

#endif
