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
 * v2 per-CPU scheduler selector (docs/design/smp-scheduler-v2-plan.md).
 *   0 (default) — one global run queue under schedulerSpinLock (the 2002 design;
 *                 UP + the current x86_64 cooperative SMP).  this_rq()/cpu_rq()
 *                 always resolve to g_rq[0], so behaviour is unchanged.
 *   1           — per-CPU run queues: each CPU schedules from g_rq[cpuid] under its
 *                 own rq_lock.  Built + validated behind this flag before it becomes
 *                 the SMP default.
 */
#define CONFIG_SCHED_PERCPU 0

/* Run-queue storage is an array sized for the worst case; index 0 is the only one
 * used when CONFIG_SCHED_PERCPU is 0.  Must be >= MAXCPU (asserted in sched_core.c). */
#define SCHED_MAX_CPUS 8

/*
 * A run queue: the priority buckets + a summary bitmask for the O(1) pick + (v2) its
 * own lock.  A task lives in exactly one run queue at a time (t->on_rq); t->rq_cpu
 * records which one.  See docs/design/smp-scheduler-v2-plan.md.
 */
struct runqueue
{
	kTask_t *bucket[SCHED_PRIORITIES]; /* circular FIFO list head per priority band */
	u_int32_t ready_mask;              /* bit N set ↔ bucket[N] is non-empty */
	u_int32_t nr_running;              /* v2: tasks currently on this queue (load metric) */
	int online;                        /* v2: 1 once a CPU schedules from this queue (APs set it) */
	struct spinLock rq_lock;           /* v2: per-CPU run-queue lock (unused when flag 0) */
};
extern struct runqueue g_rq[SCHED_MAX_CPUS];

/* Sentinel t->rq_cpu meaning "no home queue chosen yet": rq_enqueue_locked picks the
 * least-loaded online CPU on first enqueue (v2 load distribution).  Set at task creation. */
#define RQ_CPU_UNSET 0xFFFFFFFFu

/* BSP-spill threshold: keep work on the BSP until it already has this many ready tasks
 * queued, then balance the next task to a secondary.  At 1, a lone task stays on the
 * BSP (a quiet desktop = uniprocessor latency) but the SECOND concurrently-ready task
 * spills to the second core — real two-core parallelism under any multitasking, now
 * that secondaries are preemptive (CONFIG_SCHED_PERCPU).  See sched_select_cpu(). */
#define SCHED_SPILL_THRESH 1

/* This CPU's run queue / a specific CPU's run queue.  With CONFIG_SCHED_PERCPU 0 both
 * always return &g_rq[0] (the single global queue). */
struct runqueue *this_rq(void);
struct runqueue *cpu_rq(unsigned cpu);

void pid_hash_remove(kTask_t *t);

/* Run-queue helpers — caller must hold schedulerSpinLock. */
void rq_enqueue_locked(kTask_t *t);

/* SMP: poke other CPUs so an idle one wakes to run newly-enqueued work.  Weak
 * no-op by default (sched_core.c); x86_64 (kern/smp.c) IPIs its online APs. */
void arch_smp_reschedule(void);
/* Targeted SMP wakeup: poke exactly @cpu (per-CPU run queues).  Weak no-op default in
 * sched_core.c; x86_64/aarch64 override to IPI that one CPU. */
void arch_smp_reschedule_cpu(unsigned cpu);
void rq_dequeue_locked(kTask_t *t);

#endif /* _UBIXOS_SCHED_INTERNAL_H */
