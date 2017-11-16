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

 $Id: getgid.c 148 2016-01-18 19:34:32Z reddawg $

*****************************************************************************************/

#include <sys/types.h>

int getgid(void) {
  volatile int gid = 0x0;

  asm volatile(
    "movl $0x2f,%%eax \n"
    "int %1\n"
    "movl %%eax, %0\n"
    : "=r" (gid)
    : "i" (0x80)
    : "eax"
    );

  return(gid);
  }


/*
int getgid(void) {
  volatile int gid = 0x0;
  asm volatile(
    "mov $32,%%eax\n" 
    "int %0\n"
    : : "i" (0x80),"a" (&gid)
    );
  return(gid);
  }
*/

/***
 $Log: getgid.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:09  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:03  reddawg
 no message

 Revision 1.4  2004/08/02 18:50:13  reddawg
 Updates to make some variable volatile to make work with gcc 3.3. However there are still some issues but we have not caused new issues with gcc 2.95

 Revision 1.3  2004/08/01 20:14:18  reddawg
 Fixens

 Revision 1.2  2004/08/01 19:59:19  reddawg
 *** empty log message ***

 END
 ***/
