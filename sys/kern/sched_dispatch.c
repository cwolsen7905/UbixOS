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

/*
 * Arch-neutral scheduler dispatch: policy only.
 *
 * This is the machine-independent half of the scheduler, extracted from the old
 * i386 sched_switch.c.  It owns the dispatch policy — starvation aging, quantum
 * accounting, priority decay/boost, dead-task cleanup, callout expiry and the
 * O(1) ready-mask pick — operating entirely on the generic run-queue and
 * kTask_t state defined in sched_core.c.
 *
 * The only machine-dependent seams are:
 *   - switch_to(prev, next)        register-level context switch (machine/proc.h)
 *   - md_sched_pre_switch(next)     per-arch hook just before switch_to
 *   - cli()/sti()/save_flags/...    arch-aware IRQ macros (ubixos/wait.h)
 *   - SCHED_HZ                      scheduler tick rate (machine/proc.h)
 */

#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>
#if defined(__i386__)
#include <i386/pcpu.h> /* curcpu() — per-CPU idle thread (smp-plan Phase 3) */
#elif defined(__aarch64__)
#include <aarch64/pcpu.h> /* curcpu() — per-CPU idle thread (smp-plan M3) */
#endif
#include <ubixos/spinlock.h>
#include <ubixos/vitals.h>
#include <ubixos/endtask.h>
#include <ubixos/random.h>
#include <ubixos/wait.h>
#include <sys/shutdown.h>
#include <machine/signal.h>
#include <machine/proc.h>
#include <lib/kprintf.h>

/* Starvation aging: scan every AGING_INTERVAL ticks (~50 ms at 200 Hz).
 * Boost tasks that haven't run in > AGING_STARVE ticks (~200 ms). */
#define AGING_INTERVAL 10
#define AGING_STARVE 40

/**
 * Map a priority band to its time-slice length (in scheduler ticks).
 */
static inline u_int8_t quantum_for_priority(u_int8_t pri)
{
	if (pri >= 31)
		return 0; /* Realtime only: unlimited, never preempted */
	if (pri >= 24)
		return 20; /* High (kernel threads): long but finite quantum */
	if (pri >= 16)
		return 10; /* Interactive */
	if (pri >= 8)
		return 6; /* Normal */
	if (pri >= 1)
		return 2; /* Background */
	return 1;         /* Idle */
}

/**
 * The scheduler: age, reap, and dispatch the highest-priority ready task.
 *
 * Called from the timer tick and from voluntary yields.  Picks the next task
 * via the O(1) ready_mask, then performs the machine-dependent switch_to().
 */
void sched()
{
	kTask_t *prev = 0x0;
	kTask_t *delTask = 0x0;
	kTask_t *next = 0x0;
	kTask_t *t = 0x0;
	u_int32_t flags;

	/* Stir the CSPRNG with timer-tick jitter (lock-free, every tick). */
	krandom_stir(0);

	/* Reboot countdown: the platform reboot affordance sets reboot_at_tick; we
	 * print once per second and reboot when time is up.  Runs before the
	 * spinlock to keep it simple. */
	if (reboot_at_tick != 0)
	{
		u_int32_t now = systemVitals->sysTicks;
		u_int32_t left = (reboot_at_tick > now) ? (reboot_at_tick - now) : 0;
		u_int32_t secs = (left + SCHED_HZ - 1) / SCHED_HZ;
		static u_int32_t last_printed = 0;

		if (secs != last_printed)
		{
			last_printed = secs;
			if (secs > 0)
				kprintf("Rebooting in %u...\n", secs);
		}
		if (left == 0)
		{
			kprintf("Rebooting now.\n");
			reboot_at_tick = 0;
			sys_shutdown(REBOOT);
		}
	}

	/*
	 * Hold schedulerSpinLock with interrupts disabled.  sched_wakeup_chan() is
	 * called from interrupt context (e.g. the NIC ISR) and takes this same lock;
	 * if sched() held it with IF set, an IRQ landing mid-critical-section would
	 * spin/yield on the held lock and re-enter the scheduler.  Disabling IRQs
	 * across the locked section closes that window (and the timer's spinTryLock
	 * bail handles the genuinely-reentrant timer case).
	 */
	save_flags(flags);
	cli();
	if (spinTryLock(&schedulerSpinLock))
	{
		restore_flags(flags);
		return;
	}

	/* --- Phase 3.4: starvation aging — scan every ~50 ms --- */
	{
		static u_int32_t aging_last = 0;
		u_int32_t now = systemVitals->sysTicks;
		if (now - aging_last >= AGING_INTERVAL)
		{
			kTask_t *tmp;
			aging_last = now;
			for (tmp = taskList; tmp != NULL; tmp = tmp->next)
			{
				u_int8_t cap;
				if (tmp->state != READY || !tmp->on_rq || tmp->last_run_tick == 0)
					continue;
				if (now - tmp->last_run_tick < AGING_STARVE)
					continue;
				/* +1 boost, capped at base_priority + 8, never into interactive band.
				 * Must dequeue/re-enqueue because rq_dequeue_locked keyes on
				 * t->priority — changing it in-place corrupts the run-queue buckets. */
				cap = tmp->base_priority + 8;
				if (cap > 23)
					cap = 23;
				if (tmp->priority < cap)
				{
					rq_dequeue_locked(tmp);
					tmp->priority++;
					tmp->state = READY;
					rq_enqueue_locked(tmp);
				}
			}
		}
	}

	/* --- Timed-sleep / timer expiry: fire any callouts whose deadline elapsed
	 * (sched_wait_event_timeout arms one per timed sleep; lwIP protocol timers
	 * ride these via tcpip_thread).  O(1) — only the expiry-sorted list head is
	 * inspected.  Runs under schedulerSpinLock; callbacks re-enqueue tasks. --- */
	callout_run_expired(systemVitals->sysTicks);

	/* --- Phase 2: dead-task cleanup (separate from dispatch) --- */
	t = taskList;
	while (t != 0x0)
	{
		if (t->state == ZOMBIE)
		{
			/*
			 * ZOMBIE: task exited normally via endTask.  Notify the parent and
			 * transition to DEAD — but leave the task in taskList so the parent's
			 * wait_find_child() can collect it and free it.
			 */
			delTask = t;
			t = t->next;

			if (delTask->parent != 0x0)
			{
				/* children is decremented by wait_find_child when collected, not here.
				 * Decrementing here races with WNOHANG's early-exit check in sys_wait4
				 * and causes ECHILD before the parent can collect the dead task. */
				delTask->parent->last_exit = delTask->id;
				if (delTask->parent->state != DEAD && delTask->parent->state != ZOMBIE)
				{
					delTask->parent->td.sig_pending |= (1u << (SIGCHLD - 1));
					if (delTask->parent->state != READY)
					{
						delTask->parent->state = READY;
						rq_enqueue_locked(delTask->parent);
					}
				}
				if (delTask->term != NULL && delTask->term->owner == delTask->id)
					delTask->term->owner = delTask->parent->id;
				if (delTask->term != NULL && delTask->term->t_pgrp == (pid_t)delTask->pgrp)
					delTask->term->t_pgrp = 0;
			}
			/* ZOMBIE → DEAD: wait_find_child() will splice from taskList. */
			delTask->state = DEAD;
		}
		else if (t->state == DEAD)
		{
			/*
			 * DEAD with no living parent (fault-killed, exec-error, or after
			 * wait_find_child already spliced the task out of parent's view).
			 * Only auto-collect when the parent is gone; otherwise wait_find_child
			 * handles collection so we don't race with it.
			 */
			delTask = t;
			t = t->next;
			if (delTask->parent == 0x0 || delTask->parent->state == DEAD ||
			    delTask->parent->state == ZOMBIE)
			{
				if (delTask->prev != 0x0)
					delTask->prev->next = delTask->next;
				else
					taskList = delTask->next;
				if (delTask->next != 0x0)
					delTask->next->prev = delTask->prev;
				pid_hash_remove(delTask);
				sched_addDelTask(delTask);
			}
		}
		else
		{
			t = t->next;
		}
	}

	/* --- Phase 2: O(1) dispatch via ready_mask --- */

	/*
	 * Per-CPU idle thread (smp-plan Phase 3).  On i386 each CPU owns an idle
	 * thread pinned at curcpu()->idle and deliberately kept OUT of the shared run
	 * queue: an *enqueued* idle lets a second CPU dequeue the very same task (or
	 * two CPUs fight over one global idle), corrupting the queue — the failure
	 * that crashed login when idle threads were enqueued.  It is dispatched
	 * explicitly when ready_mask == 0 and is never enqueued or preempted by
	 * quantum.  On aarch64 the idle thread is still a normal enqueued task, so
	 * cpu_idle is NULL there and every path below is byte-for-byte unchanged.
	 */
#if defined(__i386__) || defined(__aarch64__)
	kTask_t *cpu_idle = curcpu()->idle;
#else
	kTask_t *cpu_idle = NULL;
#endif

	if (_current != NULL && _current->state == RUNNING && _current != cpu_idle)
	{
		u_int8_t pri = _current->priority;

		if (pri >= 31)
		{
			/* Realtime only: never preempt — runs until it voluntarily blocks. */
			spinUnlock(&schedulerSpinLock);
			restore_flags(flags);
			return;
		}

		/* Decrement time slice; skip switch if the task still has ticks left. */
		if (_current->quantum > 0)
			_current->quantum--;
		if (_current->quantum > 0)
		{
			spinUnlock(&schedulerSpinLock);
			restore_flags(flags);
			return;
		}

		/* Quantum expired — apply priority adjustments before re-enqueue. */
		if (_current->boost_quanta > 0)
		{
			/* I/O boost is still active: count down. */
			_current->boost_quanta--;
			if (_current->boost_quanta == 0)
				/* Boost expired: drop back to the QoS floor. */
				_current->priority = _current->base_priority;
		}
		else if (_current->priority > _current->base_priority)
		{
			/* CPU-bound decay: −2 per expired quantum (floor: base_priority). */
			u_int8_t decayed = (u_int8_t)(_current->priority - 2);
			_current->priority = (decayed > _current->base_priority) ? decayed : _current->base_priority;
		}
		_current->quantum = quantum_for_priority(_current->priority);
		_current->state = READY;
		rq_enqueue_locked(_current);
	}

	if (ready_mask == 0)
	{
		/*
		 * Nothing in the shared run queue.  On i386 run this CPU's pinned idle
		 * thread (never enqueued); if we are already on it there is nothing to
		 * switch to.  On aarch64 cpu_idle is NULL and the idle thread is a normal
		 * enqueued task, so this path is the original "return without switching".
		 */
		if (cpu_idle != NULL && _current != cpu_idle)
		{
			next = cpu_idle;
		}
		else
		{
			spinUnlock(&schedulerSpinLock);
			restore_flags(flags);
			return;
		}
	}
	else
	{
		/* Pick the highest-priority ready task (highest set bit). */
		int pri = 31 - __builtin_clz(ready_mask);
		/* run_queue[pri] is a circular list; head is the next to run. */
		next = run_queue[pri];
		/* Dequeue (removes next and rotates head to next->rq_next). */
		rq_dequeue_locked(next);
	}

	prev = _current;   /* outgoing task — saved by switch_to */
	set_current(next); /* per-CPU store: g_pcpu[cpu].current = next (%gs:8) */
	_current->last_run_tick = systemVitals->sysTicks;

	/* Give the newly dispatched task a fresh time slice if it has none left. */
	if (_current->quantum == 0)
		_current->quantum = quantum_for_priority(_current->priority);

	/* Machine-dependent pre-switch hook (i386 masks the timer for VM86 tasks). */
	md_sched_pre_switch(_current);

	cli();

	_current->state = RUNNING;

	spinUnlock(&schedulerSpinLock);

	/*
	 * Software context switch (replaces the hardware `ljmp $0x20`).  switch_to
	 * saves prev's kernel context and resumes next; prev resumes here on its next
	 * scheduling slot with IF cleared (cpu_switch restored the EFLAGS saved under
	 * the cli above), so re-enable interrupts explicitly.
	 *
	 * Skip when the highest-priority runnable task is the one already running
	 * (prev == next): there is nothing to save or restore, and cpu_switch's
	 * self-switch is unsafe — it reads next_ksp before saving prev, so for
	 * prev == next it would discard the live frame and resume a stale one.  This
	 * happens when a just-woken, I/O-boosted task (e.g. the NIC RX thread) is the
	 * sole task at the top priority.
	 */
	if (prev != _current)
		switch_to(prev, _current);

#if defined(__aarch64__)
	/*
	 * aarch64 runs a non-preemptible kernel: EL1 syscall/exception handlers run
	 * with IRQs masked, and only EL0 is preemptible (its SPSR enables IRQ).  A
	 * forced sti() here would unmask IRQs *inside* a syscall handler after its
	 * first cooperative sched_yield(), making the handler preemptible mid-
	 * critical-section.  Restore the caller's IRQ state instead: cooperative
	 * yields from a (masked) syscall stay masked; an EL0 preemption unwinds to
	 * its el1_irq KERNEL_EXIT, whose ERET restores the EL0 (IRQ-enabled) SPSR.
	 */
	restore_flags(flags);
#else
	/*
	 * i386 runs a preemptible kernel (syscalls execute with IF set).  cpu_switch
	 * restored prev's EFLAGS saved under the cli above (IF cleared), so re-enable
	 * interrupts explicitly.
	 */
	sti();
#endif

	return;
}

/**
 * Terminate the current task and yield the CPU.
 */
void schedEndTask(pidType pid)
{
	endTask(_current->id);
	sched_yield();
}

/**
 * Voluntarily relinquish the CPU.
 */
void sched_yield()
{
	u_int32_t flags;

	if (_current == NULL)
		return;

	_current->quantum = 0;

	/*
	 * High/Realtime tasks (priority >= 24) are never preempted by the
	 * normal quantum path in sched(), which returns early for them.
	 * For a voluntary yield we must enqueue _current as READY so the
	 * early-exit check is bypassed and another task can be dispatched.
	 */
	if (_current->priority >= 31)
	{
		save_flags(flags);
		cli();
		spinLock(&schedulerSpinLock);
		_current->state = READY;
		rq_enqueue_locked(_current);
		spinUnlock(&schedulerSpinLock);
		restore_flags(flags);
	}

	sched();
}
