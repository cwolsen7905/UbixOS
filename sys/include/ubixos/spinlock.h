/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _UBIXOS_SPINLOCK_H
#define _UBIXOS_SPINLOCK_H

#include <sys/types.h>

#define LOCKED 1
#define UNLOCKED 0
#define SPIN_LOCK_INITIALIZER {NULL, 0}
#define LLOCK_FLAG 1

// typedef volatile int spinLock_t;

struct spinLock
{
	struct spinLock *next;
	u_int32_t locked;
};

typedef struct spinLock *spinLock_t;

extern struct spinLock Master;

void spinLockInit(spinLock_t);
void spinUnlock(spinLock_t);
int spinTryLock(spinLock_t);
void spinLock(spinLock_t);

void spinLock_scheduler(spinLock_t *); /* Only use this spinlock in the sched. */

int spinLockLocked(spinLock_t); /* nonzero if the lock is currently held */

/* True spinlock for SMP: busy-wait acquire with interrupts disabled while held.
 * For short cross-CPU critical sections (scheduler/run queue) where the holder
 * must never sleep.  Returns saved EFLAGS; pass it back to spinUnlockIrq.  Do
 * NOT mix this discipline with the yielding spinLock() on the same lock. */
u_int32_t spinLockIrq(spinLock_t);
void spinUnlockIrq(spinLock_t, u_int32_t flags);

/* Atomic exchange (of various sizes).  i386 uses the native xchg instructions;
 * other arches (aarch64) use the compiler's __atomic builtins, which lower to
 * the appropriate LL/SC or swap sequence. */
#if defined(__i386__)
static inline u_long xchg_64(volatile u_int32_t *ptr, u_long x)
{
	__asm__ __volatile__("xchgq %1,%0" : "+r"(x), "+m"(*ptr));

	return x;
}

static inline unsigned xchg_32(volatile u_int32_t *ptr, u_int32_t x)
{
	__asm__ __volatile__("xchgl %1,%0" : "+r"(x), "+m"(*ptr));

	return x;
}

static inline unsigned short xchg_16(volatile u_int32_t *ptr, u_int16_t x)
{
	__asm__ __volatile__("xchgw %1,%0" : "+r"(x), "+m"(*ptr));

	return x;
}

/* Test and set a bit */
static inline char atomic_bitsetandtest(void *ptr, int x)
{
	char out;
	__asm__ __volatile__("lock; bts %2,%1\n"
	                     "sbb %0,%0\n"
	                     : "=r"(out), "=m"(*(volatile long long *)ptr)
	                     : "Ir"(x)
	                     : "memory");

	return out;
}
#else /* portable (aarch64): __atomic builtins */
static inline u_long xchg_64(volatile u_int32_t *ptr, u_long x)
{
	return __atomic_exchange_n(ptr, (u_int32_t)x, __ATOMIC_SEQ_CST);
}

static inline unsigned xchg_32(volatile u_int32_t *ptr, u_int32_t x)
{
	return __atomic_exchange_n(ptr, x, __ATOMIC_SEQ_CST);
}

static inline unsigned short xchg_16(volatile u_int32_t *ptr, u_int16_t x)
{
	return (unsigned short)__atomic_exchange_n(ptr, x, __ATOMIC_SEQ_CST);
}

/* Test and set a bit; returns -1 (all ones) if the bit was already set, else 0
 * (matching the x86 sbb result above). */
static inline char atomic_bitsetandtest(void *ptr, int x)
{
	u_int32_t mask = 1u << (x & 31);
	u_int32_t old = __atomic_fetch_or((volatile u_int32_t *)ptr, mask, __ATOMIC_SEQ_CST);
	return (old & mask) ? (char)-1 : 0;
}
#endif

#endif
