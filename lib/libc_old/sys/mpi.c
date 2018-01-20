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

 $Id: mpi.c 89 2016-01-12 00:20:40Z reddawg $

*****************************************************************************************/

#include <sys/mpi.h>

int mpi_createMbox(char *name) { 
  volatile int status = 0x0;
  asm volatile(
    "int %0\n"
    : : "i" (0x81),"a" (50),"b" (&status),"c" (name)
    );

  return(status);
  }

int mpi_destroyMbox(char *name) {
  volatile int status = 0x0;
  asm volatile(
    "int %0\n"
    : : "i" (0x81),"a" (51),"b" (&status),"c" (name)
    );

  return(status);
  }

int mpi_postMessage(char *name,uInt32 type,mpi_message_t *msg) {
  asm volatile(
    "int %0\n"
    : : "i" (0x81),"a" (52),"b" (name),"c" (&type),"d" (msg)
    );
  return(type);
  }

int mpi_fetchMessage(char *name,mpi_message_t *msg) {
  volatile int status = 0x0;
  asm volatile(
    "int %0\n"
    : : "i" (0x81),"a" (53),"b" (name),"c" (msg),"d" (&status)
    );
  return(status);
  }

int mpi_spam(uInt32 type,void *data) {
  volatile int status = 0x0;
  asm volatile(
    "int %0\n"
    : : "i" (0x81),"a" (54),"b" (type),"c" (data),"d" (&status)
    );
  return(status);
  }

/***
 $Log: mpi.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:09  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:04  reddawg
 no message

 Revision 1.4  2004/08/14 11:23:02  reddawg
 Changes

 Revision 1.3  2004/08/02 18:50:13  reddawg
 Updates to make some variable volatile to make work with gcc 3.3. However there are still some issues but we have not caused new issues with gcc 2.95

 Revision 1.2  2004/05/26 15:39:22  reddawg
 mpi: brought mpiDestroyMbox(char *name) in to the userland

 Revision 1.1  2004/05/25 15:43:27  reddawg
 Added Userland MPI access

 END
 ***/

