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

#ifndef _UBIXOS_SCHED_INTERNAL_H
#define _UBIXOS_SCHED_INTERNAL_H

#include <ubixos/sched.h>
#include <ubixos/spinlock.h>

/*
 * Shared scheduler state owned by sched_core.c, consumed by sched_switch.c.
 * Not part of the public sched.h API.
 */
extern kTask_t *taskList;
extern struct spinLock schedulerSpinLock;

/* Phase 2: priority run queues. */
#define SCHED_PRIORITIES 32

/*
 * A run queue: the priority buckets + a summary bitmask for the O(1) pick.  Today
 * there is a single global instance (g_rq) shared under schedulerSpinLock — the
 * 2002 uniprocessor design.  The per-CPU scheduler (v2) gives each CPU its own
 * struct runqueue with its own lock; see docs/design/smp-scheduler-v2-plan.md.
 * Either way a task lives in exactly one run queue at a time (t->on_rq).
 */
struct runqueue
{
	kTask_t  *bucket[SCHED_PRIORITIES]; /* circular FIFO list head per priority band */
	u_int32_t ready_mask;               /* bit N set ↔ bucket[N] is non-empty */
};
extern struct runqueue g_rq;

void pid_hash_remove(kTask_t *t);

/* Run-queue helpers — caller must hold schedulerSpinLock. */
void rq_enqueue_locked(kTask_t *t);

/* SMP: poke other CPUs so an idle one wakes to run newly-enqueued work.  Weak
 * no-op by default (sched_core.c); x86_64 (kern/smp.c) IPIs its online APs. */
void arch_smp_reschedule(void);
void rq_dequeue_locked(kTask_t *t);

#endif /* _UBIXOS_SCHED_INTERNAL_H */
