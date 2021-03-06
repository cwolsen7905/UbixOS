/*****************************************************************************************
 Copyright (c) 2002 The UbixOS Project
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

 $Log: ubthread.c,v $
 Revision 1.7  2004/04/13 16:16:44  reddawg
 Changed our copyright, it is all now under a BSD-Style license



 $Id: ubthread.c,v 1.7 2004/04/13 16:16:44 reddawg Exp $

*****************************************************************************************/

#include <ubixos/ubthread.h>
#include <ubixos/vitals.h>
#include <ubixos/exec.h>
#include <ubixos/sched.h>
#include <ubixos/vitals.h>
#include <ubixos/time.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>

struct ubthread_cond_list *conds = 0x0;
struct ubthread_mutex_list *mutex = 0x0;

kTask_t *ubthread_self() {
  return(_current);
  }

int ubthread_cond_init(ubthread_cond_t *cond,const uInt32 attr) { 
  ubthread_cond_t  ubcond = kmalloc(sizeof(struct ubthread_cond),sysID);
  ubcond->id     = (int)cond;
  ubcond->locked = UNLOCKED;
  *cond = ubcond;
  return(0x0);
  }

int ubthread_mutex_init(ubthread_mutex_t *mutex,const uInt32 attr) { 
  ubthread_mutex_t ubmutex = kmalloc(sizeof(struct ubthread_mutex),sysID);
  ubmutex->id     = (int)mutex;
  ubmutex->locked = UNLOCKED;
  *mutex = ubmutex;
  return(0x0);
  }

int ubthread_cond_destroy(ubthread_cond_t *cond) {
  kfree(*cond);
  *cond = 0x0;
  return(0x0);
  }

int ubthread_mutex_destroy(ubthread_mutex_t *mutex) {
  kfree(*mutex);
  *mutex = 0x0;
  return(0x0);
  }

int ubthread_create(kTask_t **thread,const uInt32 *attr,void *start_routine, void *arg) {
  *thread = (void *)execThread((void *)start_routine,(uInt32)(kmalloc(0x4000,sysID)+0x4000),arg,"TCP/IP Thread");
  return(0x0);
  }

int ubthread_mutex_lock(ubthread_mutex_t *mutex) {
  ubthread_mutex_t ubmutex = *mutex;
  if (ubmutex->locked == LOCKED) {
    kprintf("Mutex Already Lock By %x Trying To Be Relocked By %x\n",ubmutex->pid,_current->id);
    }
  while (ubmutex->locked == LOCKED);
  ubmutex->locked = LOCKED;
  ubmutex->pid    = _current->id;
  return(0x0);
  }

int ubthread_mutex_unlock(ubthread_mutex_t *mutex) {
  ubthread_mutex_t ubmutex = *mutex;
 if (ubmutex->pid == _current->id) {
    ubmutex->locked = UNLOCKED;
    return(0x0);
    }
  else {
    kprintf("Trying To Unlock Mutex From No Locking Thread\n");
    ubmutex->locked = UNLOCKED;
    return(-1);
    }
  }

int ubthread_cond_timedwait(ubthread_cond_t *cond, ubthread_mutex_t *mutex, const struct timespec *abstime) {
  ubthread_cond_t  ubcond  = *cond;
  ubthread_mutex_t ubmutex = *mutex;
  uInt32 enterTime = systemVitals->sysUptime+20;
  while (enterTime > systemVitals->sysUptime) {
    if (ubcond->locked == UNLOCKED) break;
    schedYield();
    }
  ubmutex->locked = UNLOCKED;
  return(0x0);
  }

int ubthread_cond_wait(ubthread_cond_t *cond, ubthread_mutex_t *mutex) {
  ubthread_cond_t  ubcond  = *cond;
  ubthread_mutex_t ubmutex = *mutex;
  while (ubcond->locked == 0x0);
  ubmutex->locked = UNLOCKED;
  return(0x0);
  }

int ubthread_cond_signal(ubthread_cond_t *cond) {
  ubthread_cond_t ubcond = *cond;
  ubcond->locked = UNLOCKED;
  return(0x0);
  }

/***
 END
 ***/
