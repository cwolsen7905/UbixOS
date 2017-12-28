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

 $Id: spinlock.h 79 2016-01-11 16:21:27Z reddawg $

 *****************************************************************************************/

#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#include <sys/types.h>

#define LOCKED   1
#define UNLOCKED 0
#define SPIN_LOCK_INITIALIZER   {NULL, 0}
#define LLOCK_FLAG (void *)1

//typedef volatile int spinLock_t;

struct spinLock {
    struct spinLock *next;
    int locked;
};

typedef struct spinLock *spinLock_t;

extern struct spinLock Master;

void spinLockInit(spinLock_t);
void spinUnlock(spinLock_t);
int spinTryLock(spinLock_t);
void spinLock(spinLock_t *);

void spinLock_scheduler(spinLock_t *); /* Only use this spinlock in the sched. */

int spinLockLocked(spinLock_t *);

/* Atomic exchange (of various sizes) */
static inline u_long xchg_64(volatile uint32_t *ptr, u_long x) {
  __asm__ __volatile__("xchgq %1,%0"
    :"+r" (x),
    "+m" (*ptr));

  return x;
}

static inline unsigned xchg_32(volatile uint32_t *ptr, uint32_t x) {
  __asm__ __volatile__("xchgl %1,%0"
    :"+r" (x),
    "+m" (*ptr));

  return x;
}

static inline unsigned short xchg_16(volatile uint32_t *ptr, uint16_t x) {
  __asm__ __volatile__("xchgw %1,%0"
    :"+r" (x),
    "+m" (*ptr));

  return x;
}

/* Test and set a bit */
static inline char atomic_bitsetandtest(void *ptr, int x) {
  char out;
  __asm__ __volatile__("lock; bts %2,%1\n"
    "sbb %0,%0\n"
    :"=r" (out), "=m" (*(volatile long long *)ptr)
    :"Ir" (x)
    :"memory");

  return out;
}

#endif
