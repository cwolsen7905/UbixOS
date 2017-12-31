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

 $Id: mpi.h 79 2016-01-11 16:21:27Z reddawg $

*****************************************************************************************/

#ifndef _MPI_H
#define _MPI_H

#include <sys/types.h>
#include <ubixos/sched.h>

#define MESSAGE_LENGTH 248

struct mpi_message {
  char                 data[MESSAGE_LENGTH];
  uInt32               header;
  pidType              pid;
  struct mpi_message  *next;
  };

struct mpi_mbox {
  struct mpi_mbox     *next;
  struct mpi_mbox     *prev;
  struct mpi_message  *msg;
  struct mpi_message  *msgLast;
  char                 name[64];
  pidType              pid;
  };

typedef struct mpi_mbox    mpi_mbox_t;
typedef struct mpi_message mpi_message_t;


int mpi_createMbox(char *);
int mpi_destroyMbox(char *);
int mpi_postMessage(char *,uInt32,mpi_message_t *);
int mpi_fetchMessage(char *,mpi_message_t *);
int mpi_spam(uInt32,void *);

#endif

/***
 $Log: mpi.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:42  reddawg
 no message

 Revision 1.8  2004/08/14 11:23:02  reddawg
 Changes

 Revision 1.7  2004/05/28 03:52:56  reddawg
 mpi: took a few suggestions from TCA

 Revision 1.6  2004/05/25 18:33:11  reddawg
 We now use 128byte messages I can increase later

 Revision 1.5  2004/05/25 18:29:57  reddawg
 We now lock onto a pid

 Revision 1.4  2004/05/25 16:52:22  reddawg
 We now have mpiDestroyMbox(char *) This will of course destroy a mail box

 Revision 1.3  2004/05/25 16:28:21  reddawg
 Made mpiFindMbox() static

 Revision 1.2  2004/05/25 15:42:19  reddawg
 Enabled mpiSpam();

 Revision 1.1  2004/05/25 14:07:01  reddawg
 Sorry we can't forget the headers files

 END
 ***/

