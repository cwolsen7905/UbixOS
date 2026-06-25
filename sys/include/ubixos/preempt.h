/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * In-kernel preemption primitive: a per-CPU `preempt_count` plus a deferred
 * reschedule flag (`resched_pending`).  A timer tick may reschedule the running
 * kernel thread only when `preempt_count == 0`; a held spinlock raises the
 * count, so "may this tick preempt?" and "is this critical section safe?" derive
 * from the one counter.  See docs/design/in-kernel-preemption-plan.md.
 *
 * Step 1 (scaffolding): the counter + accessors exist but nothing raises the
 * count yet, so behaviour is unchanged.  Later steps gate the timer tick on it,
 * convert the yielding spinlock to a true (preempt-disabling) one, and turn on
 * aarch64 EL1 preemption.
 *
 * NOTE: i386 (frozen on releng/2) is not built on master; its `struct pcpu`
 * would need the same two fields before this header compiles there.
 */

#ifndef _UBIXOS_PREEMPT_H
#define _UBIXOS_PREEMPT_H

#include <sys/types.h>

/* curcpu() + struct pcpu (which holds preempt_count / resched_pending). */
#if defined(__aarch64__)
#include <aarch64/pcpu.h>
#elif defined(__x86_64__)
#include <x86_64/pcpu.h>
#elif defined(__i386__)
#include <i386/pcpu.h>
#endif

/**
 * Enter a non-preemptible region.  A timer tick will not context-switch the
 * running kernel thread while the count is non-zero.  Nests; pair each call with
 * exactly one preempt_enable[_no_resched]().
 */
static inline void preempt_disable(void)
{
	curcpu()->preempt_count++;
}

/**
 * Leave a non-preemptible region without draining a deferred reschedule.  Used
 * where the caller will reschedule by another path; the resched-draining
 * preempt_enable() is wired in a later step (it needs sched(), kept out of this
 * header to avoid a circular include).
 */
static inline void preempt_enable_no_resched(void)
{
	curcpu()->preempt_count--;
}

/** @return non-zero if preemption is currently enabled (count == 0). */
static inline int preempt_enabled(void)
{
	return (curcpu()->preempt_count == 0);
}

/**
 * @return non-zero if a timer tick deferred a reschedule and it is now safe to
 * run (count back to 0).  The tick path sets resched_pending; the drain happens
 * in a later step.
 */
static inline int should_resched(void)
{
	struct pcpu *p = curcpu();
	return (p->preempt_count == 0 && p->resched_pending != 0);
}

#endif /* _UBIXOS_PREEMPT_H */
