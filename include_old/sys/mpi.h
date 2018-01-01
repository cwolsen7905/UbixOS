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

 $Id: mpi.h 89 2016-01-12 00:20:40Z reddawg $

*****************************************************************************************/

#ifndef _MPI_H
#define _MPI_H

#include <sys/types.h>

#define MESSAGE_LENGTH 248

struct mpi_message {
  char               data[MESSAGE_LENGTH];
  uInt32             header;
  struct mpi_message *next;
  };

typedef struct mpi_message mpi_message_t;


int mpi_createMbox(char *);
int mpi_destroyMbox(char *);
int mpi_postMessage(char *,uInt32,mpi_message_t *);
int mpi_fetchMessage(char *,mpi_message_t *);
int mpi_fpam(uInt32 type,void *);

#endif

/***
 $Log: mpi.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:08  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:29  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:14:16  reddawg
 no message

 Revision 1.5  2004/08/14 11:23:02  reddawg
 Changes

 Revision 1.4  2004/05/28 03:53:30  reddawg
 mpi: oops can't forget userland

 Revision 1.3  2004/05/26 15:39:22  reddawg
 mpi: brought mpiDestroyMbox(char *name) in to the userland

 Revision 1.2  2004/05/25 18:48:48  reddawg
 Userland now uses MESSAGE_LENGTH

 Revision 1.1  2004/05/25 15:43:27  reddawg
 Added Userland MPI access

 Revision 1.1  2004/05/25 14:07:01  reddawg
 Sorry we can't forget the headers files

 END
 ***/

