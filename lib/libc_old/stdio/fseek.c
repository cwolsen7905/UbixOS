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

 $Id: fseek.c 160 2016-01-19 04:01:03Z reddawg $

*****************************************************************************************/

#include <stdio.h>

size_t _fseek(FILE *stream, long offset, int whence);

asm(
  ".globl _fseek\n"
  "_fseek:\n"
  "movl $294,%eax\n"
  "int $0x80\n"
  "ret\n"
  );


int fseek(FILE *stream,long offset,int whence) {
/*
  asm volatile(
    "int %0\n"
    : : "i" (0x80),"a" (27),"b" (fd),"c" (offset),"d" (whence)
    );
  return(offset+whence);
*/
  return(_fseek(stream,offset,whence));
  }

/***
 $Log: fseek.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:10  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:22:55  reddawg
 no message

 Revision 1.3  2004/06/17 15:16:02  reddawg
 Made asm statements volatile

 Revision 1.2  2004/06/16 19:38:26  reddawg
 Updated CW Cleaned Out Dead Code

 END
 ***/
