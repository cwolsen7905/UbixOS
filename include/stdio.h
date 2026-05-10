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

 $Id: stdio.h 88 2016-01-12 00:11:29Z reddawg $

**************************************************************************************/

#ifndef _STDIO_H
#define _STDIO_H

#include <sys/types.h>
#include <wchar.h>
#include <stdarg.h>

/* Type Definitions */

typedef struct fileDescriptor {
  u_long  fd;
  uint32_t size;
  } FILE;

/* Definitions */

extern FILE fdTable[];

#define stdin   (&fdTable[0])
#define stdout  (&fdTable[1])
#define stderr  (&fdTable[2])

#define SEEK_SET 0

#define EOF     (-1)

/* Functions Definitions */

int fprintf(FILE *, const char *,...);
int printf(const char *, ...);
int vfprintf(FILE *,const char *,vaList args);
int vsprintf(char *buf,const char *fmt,vaList args);
FILE *fopen(const char *,const char *);
int fwrite(const void *ptr,int size,int nmemb,FILE *fd);
int fgetc(FILE *fd);

//New Functions Listed From Here On Till I'm Done Writing A libc
int sprintf(char *string, const char *format, ...);
char *gets(char *string);
size_t fread(void *pointer,size_t size,size_t count, FILE *stream);
int fclose(FILE *fp);
int fseek(FILE *,long offset,int whence);


/**** Proper LIBC Stuff ****/

typedef    __off_t         fpos_t;

extern __const int sys_nerr;
extern __const char *__const sys_errlist[];


int      asprintf(char **, const char *, ...) __printflike(2, 3);
int      feof(FILE *);
char    *fgets(char * __restrict, int, FILE * __restrict);
int      fflush(FILE *);
/**** END ****/

#endif

