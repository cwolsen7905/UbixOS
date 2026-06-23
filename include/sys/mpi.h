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

/*
 * MUST stay byte-identical to the kernel's struct mpi_message (sys/include/mpi/mpi.h):
 * the MPI syscalls hand this user buffer straight to the kernel with no copyin/out, so
 * the kernel reads/writes these fields at their struct offsets in the caller's buffer.
 * The envelope (pid/msg_id/in_reply_to) lets a reply be matched to its request — see
 * mpi_call()/mpi_reply().
 */
struct mpi_message
{
	char data[MESSAGE_LENGTH];
	uint32_t header;
	int pid;              /* src pid — kernel-stamped on post */
	uint32_t msg_id;      /* kernel-stamped id, written back into the poster's buffer */
	uint32_t in_reply_to; /* a replier sets this to the request's msg_id (0 = not a reply) */
	struct mpi_message *next;
};

typedef struct mpi_message mpi_message_t;

int mpi_createMbox(const char *);
int mpi_destroyMbox(const char *);
int mpi_postMessage(const char *, uint32_t, mpi_message_t *);
int mpi_fetchMessage(const char *, mpi_message_t *);
int mpi_waitMessage(const char *, mpi_message_t *, uint32_t); /* blocking fetch; timeout in ticks (0 = forever) */
int mpi_fpam(uint32_t type, void *);

/* Request/reply over MPI with envelope correlation (see lib/ubix_api/mpi_rpc.c). */
int mpi_call(const char *dest,
             const char *reply_mbox,
             mpi_message_t *req,
             uint32_t want_header,
             mpi_message_t *reply,
             uint32_t timeout);
int mpi_reply(const char *reply_mbox, mpi_message_t *req, uint32_t header, mpi_message_t *rsp);

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
