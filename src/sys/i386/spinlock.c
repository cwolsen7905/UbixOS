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

 $Id: spinlock.c 54 2016-01-11 01:29:55Z reddawg $

 *****************************************************************************************/

#include <sys/types.h>
#include <ubixos/spinlock.h>
#include <ubixos/sched.h>

#define atomic_xadd(P, V) __sync_fetch_and_add((P), (V))
#define cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#define atomic_inc(P) __sync_add_and_fetch((P), 1)
#define atomic_dec(P) __sync_add_and_fetch((P), -1)
#define atomic_add(P, V) __sync_add_and_fetch((P), (V))
#define atomic_set_bit(P, V) __sync_or_and_fetch((P), 1<<(V))
#define atomic_clear_bit(P, V) __sync_and_and_fetch((P), ~(1<<(V)))

#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")

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

void spinLockInit(spinLock_t lock) {
	memset(lock, 0x0, sizeof(spinLock_t));
}

void spinLock(spinLock_t *lock) {
	struct spinLock me;
	spinLock_t tail;

	/* Fast path - no users  */
	if (!cmpxchg(lock, NULL, LLOCK_FLAG))
		return;

	me.next = LLOCK_FLAG;
	me.locked = 0;

	/* Convert into a wait list */
	tail = xchg_32(lock, &me);

	if (tail) {
		/* Add myself to the list of waiters */
		if (tail == LLOCK_FLAG)
			tail = NULL;

		me.next = tail;

		/* Wait for being able to go */
		while (!me.locked)
			sched_yield();

		return;
	}

	/* Try to convert to an exclusive lock */
	if (cmpxchg(lock, &me, LLOCK_FLAG) == &me)
		return;

	/* Failed - there is now a wait list */
	tail = *lock;

	/* Scan to find who is after me */
	while (1) {
		/* Wait for them to enter their next link */
		while (tail->next == LLOCK_FLAG )
			sched_yield();

		if (tail->next == &me) {
			/* Fix their next pointer */
			tail->next = NULL;

			return;
		}

		tail = tail->next;
	}
}

void spinUnlock(spinLock_t *l)
{
	spinLock_t tail;
	spinLock_t tp;

	while (1)
	{
		tail = *l;

		barrier();

		/* Fast path */
		if (tail == LLOCK_FLAG)
		{
			if (cmpxchg(l, LLOCK_FLAG, NULL) == LLOCK_FLAG) return;

			continue;
		}

		tp = NULL;

		/* Wait for partially added waiter */
		while (tail->next == LLOCK_FLAG) sched_yield();

		/* There is a wait list */
		if (tail->next) break;

		/* Try to convert to a single-waiter lock */
		if (cmpxchg(l, tail, LLOCK_FLAG) == tail)
		{
			/* Unlock */
			tail->locked = 1;

			return;
		}

		sched_yield();
	}

	/* A long list */
	tp = tail;
	tail = tail->next;

	/* Scan wait list */
	while (1)
	{
		/* Wait for partially added waiter */
		while (tail->next == LLOCK_FLAG) sched_yield();

		if (!tail->next) break;

		tp = tail;
		tail = tail->next;
	}

	tp->next = NULL;

	barrier();

	/* Unlock */
	tail->locked = 1;
}

int spinTryLock(spinLock_t *l)
{
	/* Simple part of a spin-lock */
	if (!cmpxchg(l, NULL, LLOCK_FLAG)) return 0;

	/* Failure! */
	return LOCKED;
}

#ifdef _BALLS
void spinLockInit_old(spinLock_t *lock) {
	*lock = SPIN_LOCK_INITIALIZER;
}

void spinUnlock_old(spinLock_t *lock) {
	barrier();
	*lock = 0x0;
}

int spinTryLock_old(spinLock_t *lock) {
	return (xchg_32(lock, LOCKED));
}

void spinLock_old(spinLock_t *lock) {
	while (1) {
		if (!xchg_32(lock, LOCKED))
			return;
		while (*lock == 1)
			sched_yield();
	}
}

void spinLock_scheduler_old(spinLock_t *lock) {
	while (!spinTryLock(lock))
		while (*lock == 1)
			;
}

int spinLockLocked_old(spinLock_t *lock) {
	return (*lock != 0);
}
#endif
