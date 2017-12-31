/*****************************************************************************************
 Copyright (c) 2002-2004 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id: vfprintf.c 158 2016-01-19 02:08:13Z reddawg $

*****************************************************************************************/

#include <stdio.h>
#include <string.h>

int _write(int fd, void *ptr, size_t size);

asm(
  ".globl _write\n"
  "_write:\n"
  "movl $0x4,%eax\n"
  "int $0x80\n"
  "ret\n"
  );

int vfprintf(FILE *fd,const char *fmt,vaList args) {
  int retVal = 0;

  char data[512] = {'A','B','C'};

  retVal = vsprintf(&data, fmt, args);

  return(_write(fd->fd, &data, 512));

  /*
  return asm volatile(
    "movl &data,%%eax\n"
    "push %%eax\n"
    "movl $0x4,%%eax\n"
    "int %0"
    :
    : "i" (0x80),"a" (retVal)
    );
  */

  }

/*
int vfprintf(FILE *fd,const char *fmt,vaList args) {
  int retVal = 0;
  char data[512];
  retVal = vsprintf(data,fmt,args);
  asm volatile(
  //  "push %4\n"
  //  "push %3\n"
  //  "push %2\n"
    "int %0\n"
    "nop\n"
  //  "addl  $12,%%esp\n"
    :
    : "i" (0x90),"a" (23),"g" (&data),"g" (0x69),"g" (fd)
    );
  return(retVal);
  }
*/

/***
 $Log: vfprintf.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:10  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:22:56  reddawg
 no message

 Revision 1.3  2004/07/05 23:07:23  reddawg
 Old libc updates

 Revision 1.2  2004/06/16 19:38:26  reddawg
 Updated CW Cleaned Out Dead Code

 END
 ***/
