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

 $Id: sched.h 146 2016-01-17 18:55:56Z reddawg $

 *****************************************************************************************/

#ifndef _SCHED_H
#define _SCHED_H

#include <sys/types.h>
//#include <ubixos/elf.h>
#include <ubixos/tty.h>
#include <vfs/file.h>
#include <sys/tss.h>
#include <sys/thread.h>

typedef enum {
  PLACEHOLDER = -2, DEAD = -1, NEW = 0, READY = 1, RUNNING = 2, IDLE = 3, FORK = 4, WAIT = 5
} tState;

struct osInfo {
  uInt8 timer;
  uInt8 v86Task;
  bool v86If;
  uInt32 vmStart;
  uInt32 stdinSize;
  uInt32 controlKeys;
  char *stdin;
  char cwd[1024]; /* current working dir */
};

typedef struct taskStruct {
  pidType id;
  struct taskStruct *prev;
  struct taskStruct *next;
  struct tssStruct tss;
  struct i387Struct i387;
  struct osInfo oInfo;
  fileDescriptor *imageFd;
  tState state;
  u_int32_t gid;
  u_int32_t uid;
  uInt16 usedMath;
  tty_term *term;
  struct thread td;
} kTask_t;

int sched_init();
int sched_setStatus( pidType, tState );
int sched_deleteTask( pidType );
int sched_addDelTask( kTask_t * );
kTask_t *sched_getDelTask();
void sched_yield();
void sched();

void schedEndTask( pidType pid );
kTask_t *schedNewTask();
kTask_t *schedFindTask( uInt32 id );

extern kTask_t *_current;
extern kTask_t *_usedMath;

#endif

/***
 $Log: sched.h,v $
 Revision 1.3  2006/12/15 15:43:46  reddawg
 Changes

 Revision 1.2  2006/10/27 16:42:42  reddawg
 Testing

 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:55  reddawg
 no message

 Revision 1.30  2004/09/11 22:21:11  reddawg
 oInfo.cwd is now an array no longer a pointer..

 Revision 1.29  2004/09/08 23:19:58  reddawg
 hmm

 Revision 1.28  2004/09/08 22:16:02  reddawg
 Fixens

 Revision 1.27  2004/09/08 21:19:32  reddawg
 All good now

 Revision 1.26  2004/09/07 21:54:38  reddawg
 ok reverted back to old scheduling for now....

 Revision 1.20  2004/08/09 12:58:05  reddawg
 let me know when you got the surce

 Revision 1.19  2004/08/06 22:43:04  reddawg
 ok

 Revision 1.18  2004/08/06 22:32:16  reddawg
 Ubix Works Again

 Revision 1.16  2004/08/04 08:17:57  reddawg
 tty: we have primative ttys try f1-f5 so it is easier to use and debug
 ubixos

 Revision 1.15  2004/07/29 21:32:16  reddawg
 My quick lunchs breaks worth of updates....

 Revision 1.14  2004/07/21 17:15:02  reddawg
 removed garbage

 Revision 1.13  2004/07/21 14:43:14  flameshadow
 add: added cwc (current working container) to the osInfo strut

 Revision 1.12  2004/07/19 02:32:21  reddawg
 sched: we now set task status to dead which then makes the scheduler do some clean it could be some minor overhead but i feel this is our most efficient approach right now to prevent corruption of the queues

 Revision 1.11  2004/07/19 02:08:27  reddawg
 Cleaned out the rest of debuging code also temporarily disabled the ip stack to improve boot time

 Revision 1.10  2004/07/18 05:24:15  reddawg
 Fixens

 Revision 1.9  2004/07/09 13:23:20  reddawg
 sched: schedInit to sched_init
 Adjusted initialization routines

 Revision 1.8  2004/06/22 14:02:14  solar
 Added the PLACEHOLDER state for a task

 Revision 1.7  2004/06/18 13:01:47  solar
 Added nice and timeSlice members to the kTask_t type

 Revision 1.6  2004/06/17 02:12:57  reddawg
 Cleaned Out Dead Code

 Revision 1.5  2004/06/16 14:04:51  reddawg
 Renamed a typedef

 Revision 1.4  2004/06/14 12:20:54  reddawg
 notes: many bugs repaired and ld works 100% now.

 Revision 1.3  2004/05/21 15:49:13  reddawg
 The os does better housekeeping now when a task is exited

 Revision 1.2  2004/05/21 15:20:00  reddawg
 Cleaned up


 END
 ***/

