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

 $Id: ubthread.h 79 2016-01-11 16:21:27Z reddawg $

 *****************************************************************************************/

#ifndef _UBTHREAD_H
#define _UBTHREAD_H

#include <sys/types.h>
#include <stdatomic.h>
#include <ubixos/sched.h>
#include <ubixos/time.h>

#ifndef ETIMEDOUT
#define ETIMEDOUT -1
#endif

#define LOCKED     1
#define UNLOCKED   0

typedef struct ubthread *ubthread_t;
typedef struct ubthread_cond *ubthread_cond_t;
typedef struct ubthread_mutex *ubthread_mutex_t;

struct ubthread {
    kTask_t *task;
};

struct ubthread_cond {
    int id;
    bool lock;
};

struct ubthread_mutex {
    int id;
    bool lock;
    pidType pid;
};

struct ubthread_list {
    struct ubthread_list *next;
    ubthread_t thread;
};

struct ubthread_cond_list {
    struct ubthread_cond_list *next;
    ubthread_cond_t *cond;
};

struct ubthread_mutex_list {
    struct ubthread_mutex_list *next;
    ubthread_mutex_t *mutex;
};

kTask_t *ubthread_self();
int ubthread_cond_init(ubthread_cond_t *cond, const uInt32 attr);
int ubthread_mutex_init(ubthread_mutex_t *mutex, const uInt32 attr);
int ubthread_cond_destroy(ubthread_cond_t *cond);
int ubthread_mutex_destroy(ubthread_mutex_t *mutex);
int ubthread_create(kTask_t **thread, const uInt32 *attr, void (*tproc)(void), void *arg);
int ubthread_mutex_lock(ubthread_mutex_t *mutex);
int ubthread_mutex_unlock(ubthread_mutex_t *mutex);
int ubthread_cond_timedwait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, const struct timespec *abstime);
int ubthread_cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex);
int ubthread_cond_signal(ubthread_cond_t *cond);
int ubthread_cond_broadcast(ubthread_cond_t *cond);

#endif