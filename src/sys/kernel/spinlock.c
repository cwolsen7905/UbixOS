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

 $Id$

*****************************************************************************************/

#include <ubixos/spinlock.h>
#include <ubixos/sched.h>

void spinLockInit(spinLock_t *lock) {
  *lock = SPIN_LOCK_INITIALIZER;
  }

void spinUnlock(spinLock_t *lock) {
  //*lock = 0x0;
  register int unlocked;
  asm volatile(
    "xchgl %0, %1"
    : "=&r" (unlocked), "=m" (*lock) : "0" (0)
    );
  }

int spinTryLock(spinLock_t *lock) {
  register int locked;
  asm volatile("xchgl %0, %1"
    : "=&r" (locked), "=m" (*lock) : "0" (1)
    );
  return(!locked);
  }

void spinLock(spinLock_t *lock) {
  while (!spinTryLock(lock)) {
    while (*lock == 1)
      sched_yield();
    }
  }

void spinLock_scheduler(spinLock_t *lock) {
  while (!spinTryLock(lock))
    while (*lock == 1);
  }

int spinLockLocked(spinLock_t *lock) {
  return(*lock != 0);
  }

/***
 END
 ***/

