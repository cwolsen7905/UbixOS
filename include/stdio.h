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

#include <stdarg.h>
#include <stddef.h>

/* Type Definitions */

typedef struct fileDescriptor {
  unsigned long  fd;
  unsigned int   size;
} FILE;

/* Definitions */

extern FILE fdTable[];

#define stdin   (&fdTable[0])
#define stdout  (&fdTable[1])
#define stderr  (&fdTable[2])

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define EOF     (-1)

/* Functions */

int      fprintf(FILE *, const char *, ...);
int      printf(const char *, ...);
int      vfprintf(FILE *, const char *, va_list);
int      vsprintf(char *, const char *, va_list);
int      sprintf(char *, const char *, ...);
int      snprintf(char *, size_t, const char *, ...);
int      vsnprintf(char *, size_t, const char *, va_list);
int      vprintf(const char *, va_list);
int      sscanf(const char *, const char *, ...);
int      asprintf(char **, const char *, ...);

FILE    *fopen(const char *, const char *);
int      fclose(FILE *);
size_t   fread(void *, size_t, size_t, FILE *);
int      fwrite(const void *, int, int, FILE *);
int      fseek(FILE *, long, int);
long     ftell(FILE *);
void     rewind(FILE *);
int      feof(FILE *);
int      ferror(FILE *);
int      fgetc(FILE *);
char    *fgets(char *, int, FILE *);
int      fputc(int, FILE *);
int      fputs(const char *, FILE *);
int      fflush(FILE *);
FILE    *fdopen(int, const char *);

char    *gets(char *);
int      remove(const char *);
void     perror(const char *);

#endif

