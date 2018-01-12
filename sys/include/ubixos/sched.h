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

#ifndef _UBIXOS_SCHED_H
#define _UBIXOS_SCHED_H

#include <sys/types.h>
#include <vfs/file.h>
#include <ubixos/tty.h>

#include <sys/tss.h>
#include <sys/thread.h>

#define NO_GROUP -1
#define NR_GROUPS 32

typedef enum {
  PLACEHOLDER = -2, DEAD = -1, NEW = 0, READY = 1, RUNNING = 2, IDLE = 3, FORK = 4, WAIT = 5, UNINTERRUPTIBLE = 6, INTERRUPTIBLE = 7
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
    char name[256];
    struct taskStruct *prev;
    struct taskStruct *next;
    struct tssStruct tss;
    struct i387Struct i387;
    struct osInfo oInfo;
    //fileDescriptor *imageFd;
    fileDescriptor *files[MAX_OFILES];
    tState state;
    uint32_t uid, gid;
    uint16_t euid, suid;
    uint16_t egid, sgid;
    uInt16 usedMath;
    tty_term *term;
    struct thread td;
    struct {
        struct inode *pwd;
        struct inode *root;
        struct inode *exec;
    } inodes;
    uint32_t counter;
    uint16_t groups[NR_GROUPS];
} kTask_t;

int sched_init();
int sched_setStatus(pidType, tState);
int sched_deleteTask(pidType);
int sched_addDelTask(kTask_t *);
kTask_t *sched_getDelTask();
void sched_yield();
void sched();

void schedEndTask(pidType pid);
kTask_t *schedNewTask();
kTask_t *schedFindTask(uInt32 id);

extern kTask_t *_current;
extern kTask_t *_usedMath;

#endif
