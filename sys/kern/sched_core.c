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

#include <sys/_null.h>
#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>
#include <ubixos/kpanic.h>
#include <machine/signal.h> /* SIGCHLD — child-stop notification to the parent */
#include <ubixos/spinlock.h>
#include <ubixos/wait.h>
#include <ubixos/cpu_enum.h> /* CPU_ENUM_MAX, smp_cpu_count — per-core CPU accounting */
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#if defined(__aarch64__)
#include <aarch64/pcpu.h> /* curcpu() — per-CPU run queue (CONFIG_SCHED_PERCPU) + MAXCPU */
#elif defined(__x86_64__)
#include <x86_64/pcpu.h>
#elif defined(__i386__)
#include <i386/pcpu.h>
#endif
#include <string.h>
#if defined(__i386__)
#include <i386/pcpu.h> /* smp_processor_id(), curcpu() — which CPU a tick is charged to */
#elif defined(__aarch64__)
#include <aarch64/pcpu.h> /* curcpu() — which CPU a tick is charged to */
#endif
#include <ubixos/preempt.h> /* preempt_count scaffolding (in-kernel-preemption-plan.md step 1) */
#include <assert.h>
#include <sys/descrip.h>
#include <sys/resource.h>
#include <ubixos/vitals.h>

/* Shared with sched_switch.c via sched_internal.h — not static. */
kTask_t *taskList = 0x0;
struct spinLock schedulerSpinLock = SPIN_LOCK_INITIALIZER;

/* Phase 2: 32 per-priority run queues + bitmask (Windows ReadySummary trick). */
/* Run-queue storage.  With CONFIG_SCHED_PERCPU 0 only g_rq[0] is ever used (the
 * single global queue); with 1 each CPU schedules from g_rq[its cpuid]. */
_Static_assert(SCHED_MAX_CPUS >= MAXCPU, "SCHED_MAX_CPUS must cover every CPU");
struct runqueue g_rq[SCHED_MAX_CPUS];

/** @return the run queue this CPU schedules from. */
struct runqueue *this_rq(void)
{
#if CONFIG_SCHED_PERCPU
	return (&g_rq[curcpu()->cpuid]);
#else
	return (&g_rq[0]);
#endif
}

/** @return run queue @cpu (the CPU a task is enqueued on, t->rq_cpu). */
struct runqueue *cpu_rq(unsigned cpu)
{
#if CONFIG_SCHED_PERCPU
	return (&g_rq[cpu]);
#else
	(void)cpu;
	return (&g_rq[0]);
#endif
}

static kTask_t *delList = 0x0;
static u_int32_t nextID = 1;

/* Phase 1.5 — 256-bucket PID hash for O(1) schedFindTask. */
#define SCHED_HASH_BUCKETS 256
static kTask_t *pid_hash[SCHED_HASH_BUCKETS];

static inline void pid_hash_insert(kTask_t *t)
{
	int b = t->id & (SCHED_HASH_BUCKETS - 1);
	t->hash_next = pid_hash[b];
	pid_hash[b] = t;
}

void pid_hash_remove(kTask_t *t)
{
	int b = t->id & (SCHED_HASH_BUCKETS - 1);
	kTask_t **pp = &pid_hash[b];
	while (*pp)
	{
		if (*pp == t)
		{
			*pp = t->hash_next;
			return;
		}
		pp = &(*pp)->hash_next;
	}
}

#if !defined(__i386__) && !defined(__aarch64__) && !defined(__x86_64__)
/* On i386/aarch64/x86_64 _current is per-CPU state in g_pcpu[].current (reached
 * via %gs:8 / TPIDR_EL1+16 / %gs:16); see the get_current()/set_current()
 * accessors in <ubixos/sched.h>.  Other arches (armv6) keep the single global. */
kTask_t *_current = 0x0;
#endif
kTask_t *_usedMath = 0x0;

int need_resched = 0;

/*
 * Scheduler accounting (smp-plan Phase 3.5).  Uniprocessor today: a single pair
 * of CPU counters that collapse to "cpu0".  When SMP Phase 4 lands these move
 * into per-CPU storage indexed by smp_processor_id().
 */
kTask_t *g_idle_task = 0x0; /* set by main.c once the idle thread exists */

/*
 * Per-CPU busy/idle tick counters (smp-plan Phase 3.5 → per-core).  Indexed by
 * the running CPU so the Activity Monitor can draw a graph per core.  On a
 * uniprocessor (or with the APs parked) only index 0 advances; the others stay 0
 * (correctly reported as an idle/offline core).
 */
static u_int64_t g_cpu_busy_ticks[CPU_ENUM_MAX]; /* ticks running a non-idle thread, per CPU */
static u_int64_t g_cpu_idle_ticks[CPU_ENUM_MAX]; /* ticks in the idle thread, per CPU         */

/** Logical id of the CPU charging the current tick (0 on uniprocessor arches). */
static inline unsigned acct_cpu(void)
{
#if defined(__i386__)
	unsigned c = smp_processor_id();
#elif defined(__aarch64__)
	unsigned c = (unsigned)curcpu()->cpuid;
#else
	unsigned c = 0;
#endif
	return (c < CPU_ENUM_MAX) ? c : 0;
}

/** True if @cur is an idle thread (the global BSP idle or this CPU's per-CPU idle). */
static inline int acct_is_idle(kTask_t *cur)
{
	if (cur == 0x0 || cur == g_idle_task)
		return (1);
#if defined(__i386__) || defined(__aarch64__)
	if (cur == curcpu()->idle)
		return (1);
#endif
	return (0);
}

/**
 * Charge one timer tick to the running thread and to *this CPU's* busy/idle
 * counters.
 *
 * Called from each CPU's timer ISR exactly once per interrupt — before any
 * reschedule, so _current still names the thread that was actually running for
 * the slice just elapsed.  A NULL/idle _current is charged as idle.
 */
void sched_account_tick(void)
{
	kTask_t *cur = _current;
	unsigned cpu = acct_cpu();

	if (!acct_is_idle(cur))
	{
		/* Charge per-process CPU time only to real work: the idle thread is
		 * excluded so procfs (/proc/<pid>/stat utime) reports it at 0% rather
		 * than ~100% whenever the machine is idle. */
		cur->run_ticks++;
		g_cpu_busy_ticks[cpu]++;
#if CONFIG_SCHED_PERCPU
		g_rq[cpu].running_idle = 0;
#endif
	}
	else
	{
		g_cpu_idle_ticks[cpu]++;
#if CONFIG_SCHED_PERCPU
		g_rq[cpu].running_idle = 1;
#endif
	}
}

/** @return busy ticks for CPU @cpu (0 if out of range). */
u_int64_t sched_cpu_busy_ticks_n(unsigned cpu)
{
	return (cpu < CPU_ENUM_MAX) ? g_cpu_busy_ticks[cpu] : 0;
}

/** @return idle ticks for CPU @cpu (0 if out of range). */
u_int64_t sched_cpu_idle_ticks_n(unsigned cpu)
{
	return (cpu < CPU_ENUM_MAX) ? g_cpu_idle_ticks[cpu] : 0;
}

/** @return number of CPUs whose ticks /proc/stat should report (>= 1). */
unsigned sched_cpu_acct_count(void)
{
	unsigned n = smp_cpu_count();
	if (n < 1)
		n = 1;
	return (n < CPU_ENUM_MAX) ? n : CPU_ENUM_MAX;
}

/**
 * @return total (all-CPU) ticks spent running a non-idle thread.  Aggregate kept
 * for existing callers; per-core data is via sched_cpu_busy_ticks_n().
 */
u_int64_t sched_cpu_busy_ticks(void)
{
	u_int64_t sum = 0;
	for (unsigned c = 0; c < CPU_ENUM_MAX; c++)
		sum += g_cpu_busy_ticks[c];
	return sum;
}

/**
 * @return total (all-CPU) ticks spent in the idle thread.
 */
u_int64_t sched_cpu_idle_ticks(void)
{
	u_int64_t sum = 0;
	for (unsigned c = 0; c < CPU_ENUM_MAX; c++)
		sum += g_cpu_idle_ticks[c];
	return sum;
}

int sched_init()
{
	taskList = (kTask_t *)kmalloc(sizeof(kTask_t));
	if (taskList == 0x0)
		kpanic("Unable to create task list");

	memset(taskList, 0x0, sizeof(kTask_t));
	taskList->id = nextID++;
	taskList->quantum = 6;
	taskList->priority = QOS_REALTIME;
	taskList->base_priority = QOS_REALTIME;
	strncpy(taskList->name, "kernel", sizeof(taskList->name) - 1);
	pid_hash_insert(taskList);

	kprintf("sched0: addr=0x%X\n", taskList);

	return (0x0);
}

kTask_t *schedNewTask()
{
	int i = 0;

	kTask_t *tmpTask = (kTask_t *)kmalloc(sizeof(kTask_t));

	struct file *fp = 0x0;

	if (tmpTask == 0x0)
		kpanic("Error: schedNewTask() - kmalloc failed trying to initialize a new task struct\n");

	memset(tmpTask, 0x0, sizeof(kTask_t));

	/* 64 KB per-thread kernel stack (was 8 KB).  A process that reads/writes a
	 * ubixfs pool (e.g. exec off a ubixfs root) runs the lite-ZFS core on this
	 * stack, and the core nests several 4 KB block[BS] frames — 8 KB would
	 * overflow, the same failure already fixed on the boot/kmain stacks.  Keep in
	 * sync with i386 esp0 (context_switch.c) + aarch64 KSTACK_SIZE /
	 * INITIAL_KSTACK_SIZE (all must equal this allocation size). */
	tmpTask->kernelStack = (u_int32_t *)kmalloc(65536);
	if (tmpTask->kernelStack == 0x0)
		kpanic("Error: schedNewTask() - kmalloc failed allocating kernel stack\n");
	/* Arch-specific md init (i386: TSS ring-0 stack; aarch64: clear sw-context). */
	md_new_task(tmpTask);

	tmpTask->usedMath = 0x0;
	tmpTask->state = NEW;

	memcpy(tmpTask->username, "UbixOS", 6);

	/* HACK */
	for (i = 0; i < 3; i++)
	{
		fp = (void *)kmalloc(sizeof(struct file));
		if (fp == 0x0)
			kpanic("schedNewTask: kmalloc failed allocating stdio file\n");
		memset(fp, 0, sizeof(struct file));
		tmpTask->td.o_files[i] = (void *)fp;
		fp->f_flag = 0x4;
	}

	/* RLIMIT_NOFILE: FreeBSD=8, Linux/musl=7 — set both so either lookup works */
	tmpTask->td.rlim[7].rlim_cur = 64;
	tmpTask->td.rlim[7].rlim_max = 64;
	tmpTask->td.rlim[RLIMIT_NOFILE].rlim_cur = 64;
	tmpTask->td.rlim[RLIMIT_NOFILE].rlim_max = 64;

	tmpTask->priority = 12; /* QOS_DEFAULT — mid Normal band */
	tmpTask->base_priority = 12;
	tmpTask->boost_quanta = 0;
	/* Initialize to now so starvation aging can fire immediately if the
	 * task never gets its first dispatch (last_run_tick==0 skips aging). */
	tmpTask->last_run_tick = systemVitals ? systemVitals->sysTicks : 1;
	tmpTask->on_rq = 0;
	tmpTask->rq_cpu = RQ_CPU_UNSET; /* home queue chosen on first enqueue (v2 distribution) */

	spinLock(&schedulerSpinLock);
	tmpTask->id = nextID++;
	tmpTask->tgid = tmpTask->id; /* its own thread group until rfork(RFMEM) overrides */
	tmpTask->reap_free_as = 1;   /* a normal process owns/frees its own address space */
	tmpTask->quantum = 6;
	tmpTask->next = taskList;
	tmpTask->prev = 0x0;
	taskList->prev = tmpTask;
	taskList = tmpTask;
	pid_hash_insert(tmpTask);
	spinUnlock(&schedulerSpinLock);

	return (tmpTask);
}

/* -----------------------------------------------------------------------
 * Phase 2: run-queue helpers — caller must hold schedulerSpinLock.
 * ----------------------------------------------------------------------- */

#if CONFIG_SCHED_PERCPU
/**
 * Pick the home CPU for a task being made runnable for the first time.
 *
 * Policy: BSP-biased spill.  The BSP (cpu 0) is preemptive and low-latency; the
 * secondaries are cooperative (no per-CPU preemption timer yet), so an interactive
 * task pinned to one renders with poor latency.  A quiet desktop therefore stays
 * entirely on the BSP (behaving like uniprocessor — its known-good speed) and work
 * spills to a secondary only when the BSP has a real backlog of ready tasks
 * (nr_running >= SCHED_SPILL_THRESH).  Among the secondaries the least-loaded online
 * one wins, and only if it is shorter than the BSP.  Caller holds schedulerSpinLock.
 * @return the chosen cpuid.
 */
static unsigned sched_select_cpu(void)
{
	unsigned best = 0; /* BSP is always online — the default home */
	u_int32_t best_n = g_rq[0].nr_running;
	unsigned c;

	/* Keep light (interactive) load on the preemptive BSP; only balance to a
	 * secondary once the BSP is genuinely backed up. */
	if (best_n < SCHED_SPILL_THRESH)
		return (0);

	for (c = 1; c < MAXCPU; c++)
	{
		if (!g_rq[c].online)
			continue;
		if (g_rq[c].nr_running < best_n)
		{
			best = c;
			best_n = g_rq[c].nr_running;
		}
	}
	return (best);
}
#endif

void rq_enqueue_locked(kTask_t *t)
{
	int pri;
	kTask_t *head;
	kTask_t *tail;
	struct runqueue *rq;

	if (t == NULL || t->on_rq)
		return;

	/* Choose the home run queue and record it so a remote dequeue (a waker on
	 * another CPU) finds the right queue.  First enqueue picks the least-loaded
	 * online CPU; thereafter the task keeps that home (cache affinity).  With
	 * CONFIG_SCHED_PERCPU 0 every task lives on cpu 0 / g_rq[0]. */
#if CONFIG_SCHED_PERCPU
	if (t->rq_cpu == RQ_CPU_UNSET)
		t->rq_cpu = sched_select_cpu();
#else
	t->rq_cpu = 0;
#endif
	rq = cpu_rq(t->rq_cpu);

	pri = (int)t->priority;
	head = rq->bucket[pri];

	if (head == NULL)
	{
		/* First task at this priority — circular singleton. */
		t->rq_next = t;
		t->rq_prev = t;
		rq->bucket[pri] = t;
		rq->ready_mask |= (1u << pri);
	}
	else
	{
		/* Append at the real tail (= just before head) for FIFO round-robin. */
		tail = head->rq_prev;
		t->rq_next = head;
		t->rq_prev = tail;
		tail->rq_next = t;
		head->rq_prev = t;
		/* rq->bucket[pri] stays pointing at head for O(1) dequeue. */
	}
	t->on_rq = 1;
	rq->nr_running++;

	/* SMP: a task just became runnable — wake the CPU whose queue it landed on so an
	 * idle one re-runs the scheduler and picks it up. */
#if CONFIG_SCHED_PERCPU
	/* Per-CPU queues: poke ONLY the target CPU, and only when it is not us (the task
	 * is on our own queue → we will dispatch it ourselves on the way out; no IPI).
	 * This kills the old broadcast-on-every-enqueue storm that interrupted the BSP on
	 * every secondary enqueue (and vice versa) — a major source of the SMP slowdown. */
	if (t->rq_cpu != (u_int32_t)curcpu()->cpuid)
		arch_smp_reschedule_cpu(t->rq_cpu);
#else
	/* Single global queue: any idle AP may run it (no per-CPU home). */
	arch_smp_reschedule();
#endif
}

/* Default: no SMP wakeup (single-CPU scheduling, or an arch whose APs are parked).
 * x86_64 provides a strong override (kern/smp.c) that IPIs its online APs. */
__attribute__((weak)) void arch_smp_reschedule(void)
{
}

/* Default targeted wakeup: no-op (UP, or an arch whose APs are parked).  x86_64
 * (kern/smp.c) and aarch64 (kern/apsmp.c) override to IPI exactly @cpu. */
__attribute__((weak)) void arch_smp_reschedule_cpu(unsigned cpu)
{
	(void)cpu;
}

void rq_dequeue_locked(kTask_t *t)
{
	int pri;
	struct runqueue *rq;

	if (t == NULL || !t->on_rq)
		return;

	rq = cpu_rq(t->rq_cpu); /* the queue this task was enqueued on */
	pri = (int)t->priority;

	if (t->rq_next == t)
	{
		/* Only task in this queue. */
		rq->bucket[pri] = NULL;
		rq->ready_mask &= ~(1u << pri);
	}
	else
	{
		t->rq_prev->rq_next = t->rq_next;
		t->rq_next->rq_prev = t->rq_prev;
		if (rq->bucket[pri] == t)
			rq->bucket[pri] = t->rq_next;
	}
	t->rq_next = NULL;
	t->rq_prev = NULL;
	t->on_rq = 0;
	if (rq->nr_running > 0)
		rq->nr_running--;
}

#if CONFIG_SCHED_PERCPU
/**
 * Periodic load balancer: migrate one queued (READY, not running) task from a busy
 * core to an idle one.
 *
 * rq_cpu is a soft home (cache affinity), not a hard lock — like a modern scheduler,
 * the balancer overrides it under imbalance.  This is what makes a few CPU-bound
 * tasks that all started on one core actually spread to the others: placement at
 * creation only sees the ready-queue depth, so two CPU-bound apps launched while a
 * core looked idle both pin to it and time-share one core while another sits idle.
 *
 * Runs on the BSP only, at SCHED_BALANCE_INTERVAL, with schedulerSpinLock held (called
 * from the dispatcher's maintenance pass).  Moves exactly one task per pass to avoid
 * thrashing.  Migrating a queued task is safe: it is not running, so no other CPU can
 * be mid-switch on it.  running_idle / nr_running are read as best-effort hints.
 */
void sched_balance_locked(void)
{
	static u_int32_t last_balance = 0;
	u_int32_t now = systemVitals ? systemVitals->sysTicks : 0;
	unsigned recip = ~0u; /* an online, idle core to receive work */
	unsigned donor = ~0u; /* the busiest core with a queued task to give */
	u_int32_t donor_load = 0;
	unsigned c;
	struct runqueue *drq;
	kTask_t *t = NULL;
	u_int32_t mask;

	if (acct_cpu() != 0) /* BSP only — one balancer, no two cores migrating at once */
		return;
	if (now < SCHED_BALANCE_WARMUP) /* let the desktop finish its startup handshake */
		return;
	if (now - last_balance < SCHED_BALANCE_INTERVAL)
		return;
	last_balance = now;

	/* Recipient: the first online core that ran idle last tick with an empty queue. */
	for (c = 0; c < MAXCPU; c++)
	{
		if (c != 0 && !g_rq[c].online)
			continue;
		if (g_rq[c].running_idle && g_rq[c].nr_running == 0)
		{
			recip = c;
			break;
		}
	}
	if (recip == ~0u)
		return; /* no idle core to offload onto */

	/* Donor: the online core (not the recipient) with the most queued tasks. */
	for (c = 0; c < MAXCPU; c++)
	{
		if (c == recip)
			continue;
		if (c != 0 && !g_rq[c].online)
			continue;
		if (g_rq[c].nr_running > donor_load)
		{
			donor_load = g_rq[c].nr_running;
			donor = c;
		}
	}
	if (donor == ~0u || donor_load < 1)
		return; /* nothing waiting anywhere — no imbalance to fix */

	/* Pick a victim from the donor, scanning its migratable ready bands from least to
	 * most urgent.  Two filters:
	 *   - Never migrate kernel/realtime threads (band >= 24): they may carry per-CPU
	 *     assumptions, and moving a system daemon is rarely the win.  (A task's priority
	 *     equals its bucket index, so masking off bands >= 24 enforces this.)
	 *   - Skip any task still in its post-migration cooldown — this is what stops a
	 *     CPU-bound task from bouncing back and forth on consecutive passes when the
	 *     recipient keeps going idle (donor load regrows from bursty work).
	 * Within a band the circular bucket list is walked head→tail; the first eligible
	 * task wins.  If every queued task is high-priority or cooling down, skip this pass. */
	drq = &g_rq[donor];
	mask = drq->ready_mask & ((1u << 24) - 1); /* migratable bands only (priority < 24) */
	while (mask != 0)
	{
		int b = __builtin_ctz(mask);
		kTask_t *head = drq->bucket[b];
		kTask_t *it = head;

		if (it != NULL)
		{
			do
			{
				if ((u_int32_t)(now - it->last_migrate_tick) >= SCHED_MIGRATE_COOLDOWN)
				{
					t = it;
					break;
				}
				it = it->rq_next;
			} while (it != head);
		}
		if (t != NULL)
			break;
		mask &= ~(1u << b);
	}
	if (t == NULL)
		return; /* nothing eligible — every queued task is high-priority or in cooldown */

	rq_dequeue_locked(t);       /* off the donor queue */
	t->rq_cpu = recip;          /* re-home (soft affinity follows the task) */
	t->last_migrate_tick = now; /* start the cooldown so it can't bounce straight back */
	rq_enqueue_locked(t);       /* onto the recipient; also IPIs it (rq_cpu != BSP) */
#if SCHED_BALANCE_DEBUG
	kprintf("[bal] pid %i cpu%u->cpu%u (donor_load=%u)\n", t->id, donor, recip, donor_load);
#endif
}
#else
void sched_balance_locked(void)
{
}
#endif

void sched_killTree(pidType id)
{
	kTask_t *t;
	for (t = taskList; t != 0x0; t = t->next)
	{
		if (t->parent != 0x0 && t->parent->id == id && t->state != DEAD)
			sched_killTree(t->id);
	}
	sched_setStatus(id, DEAD);
}

/**
 * Count tasks in thread group @tgid, other than @self, that are still using the
 * shared address space — i.e. not exited (DEAD/ZOMBIE) or a placeholder.
 *
 * endTask() uses this to decide whether the exiting task is the last thread of
 * its group and must tear the shared address space down.
 *
 * @return number of other live threads sharing the address space.
 */
int sched_tgid_others_alive(pidType tgid, pidType self)
{
	int n = 0;
	kTask_t *t;

	for (t = taskList; t != NULL; t = t->next)
	{
		if (t->id == (u_int32_t)self || t->tgid != tgid)
			continue;
		if (t->state == DEAD || t->state == ZOMBIE || t->state == PLACEHOLDER)
			continue;
		n++;
	}
	return (n);
}

int sched_deleteTask(pidType id)
{
	kTask_t *tmpTask = schedFindTask(id);

	if (tmpTask == 0x0)
		return (0x1);
	if (tmpTask->prev != 0x0)
		tmpTask->prev->next = tmpTask->next;
	if (tmpTask->next != 0x0)
		tmpTask->next->prev = tmpTask->prev;
	if (taskList == tmpTask)
		taskList = tmpTask->next;
	pid_hash_remove(tmpTask);
	return (0x0);
}

int sched_addDelTask(kTask_t *tmpTask)
{
	tmpTask->next = delList;
	tmpTask->prev = 0x0;
	if (delList != 0x0)
		delList->prev = tmpTask;
	delList = tmpTask;
	return (0x0);
}

kTask_t *sched_getDelTask()
{
	kTask_t *tmpTask = 0x0;

	spinLock(&schedulerSpinLock);
	if (delList != 0x0)
	{
		tmpTask = delList;
		delList = delList->next;
	}
	spinUnlock(&schedulerSpinLock);
	return (tmpTask);
}

kTask_t *schedFindTask(u_int32_t id)
{
	kTask_t *t = pid_hash[id & (SCHED_HASH_BUCKETS - 1)];
	for (; t; t = t->hash_next)
		if (t->id == id)
			return (t);
	return (0x0);
}

int sched_setStatus(pidType pid, tState state)
{
	kTask_t *tmpTask = schedFindTask(pid);
	if (tmpTask == 0x0)
		return (0x1);
	if (state == DEAD)
		sched_dead(tmpTask);
	else if (state == READY)
		sched_ready(tmpTask);
	else
		sched_sleep(tmpTask, state);
	return (0x0);
}

void add_wait_queue(struct wait_queue **p, struct wait_queue *wait)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	if (!*p)
	{
		wait->next = wait;
		*p = wait;
	}
	else
	{
		wait->next = (*p)->next;
		(*p)->next = wait;
	}
	restore_flags(flags);
}

void remove_wait_queue(struct wait_queue **p, struct wait_queue *wait)
{
	unsigned long flags;
	struct wait_queue *tmp;

	save_flags(flags);
	cli();
	if ((*p == wait) && ((*p = wait->next) == wait))
	{
		*p = NULL;
	}
	else
	{
		struct wait_queue *head = *p;
		tmp = head;
		do
		{
			if (tmp->next == wait)
			{
				tmp->next = wait->next;
				break;
			}
			tmp = tmp->next;
		} while (tmp != head);
	}
	wait->next = NULL;
	restore_flags(flags);
}

void wake_up_interruptible(struct wait_queue **q)
{
	struct wait_queue *tmp;
	kTask_t *p;

	if (!q || !(tmp = *q))
		return;
	do
	{
		if ((p = tmp->task) != NULL)
		{
			if (p->state == INTERRUPTIBLE)
				sched_ready(p);
		}
		if (!tmp->next)
		{
			kprintf("wait_queue is bad (eip = %08lx)\n", ((unsigned long *)q)[-1]);
			kprintf("        q = %p\n", q);
			kprintf("       *q = %p\n", *q);
			kprintf("      tmp = %p\n", tmp);
			break;
		}
		tmp = tmp->next;
	} while (tmp != *q);
}

void wake_up(struct wait_queue **q)
{
	struct wait_queue *tmp;
	kTask_t *p;

	if (!q || !(tmp = *q))
		return;
	do
	{
		if ((p = tmp->task) != NULL)
		{
			if (p->state == UNINTERRUPTIBLE || p->state == INTERRUPTIBLE)
				sched_ready(p);
		}
		if (!tmp->next)
		{
			kprintf("wait_queue is bad (eip = %08lx)\n", ((unsigned long *)q)[-1]);
			kprintf("        q = %p\n", q);
			kprintf("       *q = %p\n", *q);
			kprintf("      tmp = %p\n", tmp);
			break;
		}
		tmp = tmp->next;
	} while (tmp != *q);
}

/* -----------------------------------------------------------------------
 * Scheduler state-transition API — Phase 2: run-queue management.
 * ----------------------------------------------------------------------- */

void sched_ready(kTask_t *t)
{
	u_int32_t flags;
	if (t == NULL)
		return;

	/*
	 * First time this task becomes runnable, build its software-switch initial
	 * kernel-stack frame from the now-populated md_tss.  Guarded to fire once
	 * (md_kstack stays 0 until built) and skipped for v86 tasks, which keep
	 * hardware ljmp switching in the hybrid.  Touches only the task's own kernel
	 * stack, so it is safe outside the scheduler lock.  Dormant until sched()
	 * is flipped from ljmp to switch_to.
	 */
	if (t->md.md_kstack == 0)
		md_setup_initial_frame(t);

	save_flags(flags);
	cli();
	spinLock(&schedulerSpinLock);
	if (t->state != READY)
	{
		t->state = READY;
		rq_enqueue_locked(t);
	}
	spinUnlock(&schedulerSpinLock);
	restore_flags(flags);
}

void sched_dead(kTask_t *t)
{
	u_int32_t flags;
	if (t == NULL)
		return;
	save_flags(flags);
	cli();
	spinLock(&schedulerSpinLock);
	rq_dequeue_locked(t);
	t->state = DEAD;
	spinUnlock(&schedulerSpinLock);
	restore_flags(flags);
}

void sched_sleep(kTask_t *t, tState s)
{
	u_int32_t flags;
	if (t == NULL)
		return;
	save_flags(flags);
	cli();
	spinLock(&schedulerSpinLock);
	rq_dequeue_locked(t);
	t->state = s;
	spinUnlock(&schedulerSpinLock);
	restore_flags(flags);
}

void sched_wakeup(kTask_t *t)
{
	/* Called when _current resumes after its own sleep — it's already
	 * the running task so it doesn't need to be re-enqueued. */
	if (t != NULL)
		t->state = RUNNING;
}

/*
 * sched_zombie — called by endTask() when a process exits normally.
 * Removes the task from the run queue but keeps it in taskList so
 * wait_find_child() can collect it.  sched() transitions ZOMBIE→DEAD
 * after notifying the parent; wait_find_child() removes from taskList.
 */
void sched_zombie(kTask_t *t)
{
	u_int32_t flags;
	if (t == NULL)
		return;
	save_flags(flags);
	cli();
	spinLock(&schedulerSpinLock);
	rq_dequeue_locked(t);
	t->state = ZOMBIE;
	spinUnlock(&schedulerSpinLock);
	restore_flags(flags);
}

void sched_stop(kTask_t *t, int sig)
{
	u_int32_t flags;
	if (t == NULL)
		return;
	save_flags(flags);
	cli();
	spinLock(&schedulerSpinLock);
	rq_dequeue_locked(t);
	t->state = STOPPED;
	t->t_stopped_sig = sig;
	/* POSIX: a child stopping generates SIGCHLD for the parent, exactly as an
	 * exit does (see the reaper in sched_dispatch.c).  Without it a job-control
	 * shell blocked in sigsuspend never wakes to collect the stop via
	 * wait4(WUNTRACED), so the terminal wedges.  Post SIGCHLD and make the parent
	 * runnable so it delivers the signal and re-polls wait4. */
	if (t->parent != NULL && t->parent->state != DEAD && t->parent->state != ZOMBIE)
	{
		t->parent->td.sig_pending |= (1u << (SIGCHLD - 1));
		if (t->parent->state != READY)
		{
			t->parent->state = READY;
			rq_enqueue_locked(t->parent);
		}
	}
	spinUnlock(&schedulerSpinLock);
	restore_flags(flags);
}

/*
 * sched_io_wakeup — called when a task that was waiting for I/O becomes
 * runnable.  Boosts its scheduling priority by +4 (capped at 23, floored at
 * base_priority) for 2 quanta, then enqueues it as READY.
 *
 * Rewards I/O-bound tasks that yield the CPU willingly (FreeBSD ULE style).
 */
/*
 * sched_pi_boost — priority inheritance: raise t's scheduling priority to
 * pri if pri is higher than t's current priority.  Safe for any task state:
 * dequeues/re-enqueues when READY (preserving run-queue integrity), changes
 * in-place for RUNNING or sleeping tasks (not in any queue).
 */
void sched_pi_boost(kTask_t *t, u_int8_t pri)
{
	u_int32_t flags;

	if (t == NULL || pri <= t->priority)
		return;
	save_flags(flags);
	cli();
	spinLock(&schedulerSpinLock);
	if (pri > t->priority)
	{
		if (t->state == READY)
		{
			rq_dequeue_locked(t);
			t->priority = pri;
			rq_enqueue_locked(t);
		}
		else
		{
			t->priority = pri;
		}
	}
	spinUnlock(&schedulerSpinLock);
	restore_flags(flags);
}

/*
 * sched_pi_restore — drop t's priority back to its QoS floor (base_priority)
 * after releasing a mutex that had a PI boost applied.  Called after the lock
 * is released so the newly-unblocked high-priority waiter runs immediately.
 */
void sched_pi_restore(kTask_t *t)
{
	u_int32_t flags;

	if (t == NULL || t->priority <= t->base_priority)
		return;
	save_flags(flags);
	cli();
	spinLock(&schedulerSpinLock);
	if (t->priority > t->base_priority)
	{
		if (t->state == READY)
		{
			rq_dequeue_locked(t);
			t->priority = t->base_priority;
			rq_enqueue_locked(t);
		}
		else
		{
			t->priority = t->base_priority;
		}
	}
	spinUnlock(&schedulerSpinLock);
	restore_flags(flags);
}

void sched_io_wakeup(kTask_t *t)
{
	u_int32_t flags;
	u_int8_t boosted;

	if (t == NULL)
		return;
	save_flags(flags);
	cli();
	spinLock(&schedulerSpinLock);

	boosted = (u_int8_t)(t->base_priority + 4);
	if (boosted > 23)
		boosted = 23;

	if (t->state == READY)
	{
		/*
		 * Must dequeue before changing priority: rq_dequeue_locked
		 * keys on t->priority to find the right bucket, so changing
		 * priority in-place while enqueued corrupts that queue.
		 */
		rq_dequeue_locked(t);
		if (boosted > t->priority)
			t->priority = boosted;
		t->boost_quanta = 2;
		rq_enqueue_locked(t);
	}
	else if (t->state != RUNNING && t->state != DEAD)
	{
		if (boosted > t->priority)
			t->priority = boosted;
		t->boost_quanta = 2;
		t->state = READY;
		rq_enqueue_locked(t);
	}
	else
	{
		/* RUNNING: not in any queue — in-place change is safe. */
		if (boosted > t->priority)
			t->priority = boosted;
		t->boost_quanta = 2;
	}

	spinUnlock(&schedulerSpinLock);
	restore_flags(flags);
}

/**
 * Permanently set a task's QoS floor and current priority.
 *
 * Re-homes the task in the run queue when it is enqueued (rq_dequeue_locked
 * keys on the old priority, so the bucket must be corrected by dequeue →
 * change → enqueue).  Used at boot to drop the idle thread to QOS_IDLE.
 */
void sched_set_priority(kTask_t *t, u_int8_t pri)
{
	u_int32_t flags;
	int was_ready;

	if (t == NULL)
		return;
	save_flags(flags);
	cli();
	spinLock(&schedulerSpinLock);
	was_ready = (t->state == READY);
	if (was_ready)
		rq_dequeue_locked(t);
	t->base_priority = pri;
	t->priority = pri;
	t->boost_quanta = 0;
	if (was_ready)
		rq_enqueue_locked(t);
	spinUnlock(&schedulerSpinLock);
	restore_flags(flags);
}

/**
 * Block _current on a wait channel until a condition holds.
 *
 * Replaces the sched_yield() busy-wait loops: instead of spinning READY (and
 * burning a CPU / starving lower-priority work like the compositor), the task
 * leaves the run queue (state WAIT) so the CPU can run something else or idle.
 *
 * The condition is re-tested with interrupts disabled immediately before each
 * sleep, and _current is marked WAIT under schedulerSpinLock — the same lock
 * sched_wakeup_chan() takes — so there is no lost-wakeup window against a waker
 * that does "set condition; sched_wakeup_chan(chan)", even an ISR waker.  On UP,
 * IRQs-off + no-preemption makes the cond() test atomic with the sleep.
 *
 * @param chan  Opaque address identifying the wait queue; the matching
 *              sched_wakeup_chan() must pass the same address.
 * @param cond  Returns nonzero once the awaited condition is satisfied.
 */
/**
 * Timeout-callout callback: re-enqueue a task whose timed sleep expired.
 * Runs under schedulerSpinLock (callout_run_expired() is invoked from sched()
 * with the lock held), so it manipulates the run queue directly.
 */
static void sleep_wake_cb(void *arg)
{
	kTask_t *t = (kTask_t *)arg;

	if (t->state == WAIT || t->state == UNINTERRUPTIBLE || t->state == INTERRUPTIBLE)
	{
		t->wait_chan = NULL;
		t->state = READY;
		rq_enqueue_locked(t);
	}
}

int sched_wait_event_timeout(void *chan, int (*cond)(void *arg), void *arg, u_int32_t ticks)
{
	u_int32_t flags;
	u_int32_t deadline;
	int timed_out = 0;

	if (_current == NULL)
		return 0;

	deadline = systemVitals->sysTicks + ticks;

	save_flags(flags);
	cli();
	for (;;)
	{
		if (cond(arg))
		{
			timed_out = 0;
			break;
		}
		if (ticks != 0 && (int32_t)(systemVitals->sysTicks - deadline) >= 0)
		{
			timed_out = 1;
			break;
		}
		spinLock(&schedulerSpinLock);
		rq_dequeue_locked(_current);
		_current->wait_chan = chan;
		/* Arm a one-shot timeout that re-enqueues us at the deadline.  IRQs are
		 * off, so sysTicks can't advance between the check above and here —
		 * deadline - now is positive.  Signal wakeups cancel this via
		 * sched_wakeup_chan(); a stale firing is a no-op (sleep_wake_cb checks
		 * the state). */
		if (ticks != 0)
			callout_reset(&_current->sleep_callout,
			              (u_int32_t)(deadline - systemVitals->sysTicks),
			              sleep_wake_cb,
			              _current);
		_current->state = WAIT;

		/* Switch away WITHOUT releasing schedulerSpinLock first.  sched_block() keeps
		 * the lock held continuously from this WAIT mark across the context switch off
		 * our stack (the resumer releases it) — closing the SMP sleep race where a
		 * waker on another CPU re-enqueues this WAIT task and a third CPU runs it on
		 * our kernel stack before we have left it.  Returns once a wakeup (signal or
		 * the timeout callout) re-enqueues us and we are re-dispatched, with the lock
		 * released and IRQs still disabled — ready to re-test the condition. */
		sched_block();
	}

	/* Cancel any still-armed timeout so it cannot fire after we return (or
	 * spuriously wake an unrelated later sleep on the same task). */
	if (ticks != 0)
	{
		spinLock(&schedulerSpinLock);
		callout_stop(&_current->sleep_callout);
		spinUnlock(&schedulerSpinLock);
	}
	restore_flags(flags);
	return timed_out;
}

void sched_wait_event(void *chan, int (*cond)(void *arg), void *arg)
{
	(void)sched_wait_event_timeout(chan, cond, arg, 0);
}

/**
 * Wake every task sleeping on `chan` (paired with sched_wait_event()).
 *
 * Safe from interrupt context: sched() and all run-queue mutators hold
 * schedulerSpinLock with interrupts disabled, so when an ISR runs the lock is
 * free and the spinLock() below never yields.  Woken tasks get the same +4 I/O
 * priority boost as sched_io_wakeup() so a just-satisfied waiter preempts the
 * spinning/idle work that was running while it slept.
 */
void sched_wakeup_chan(void *chan)
{
	u_int32_t flags;
	kTask_t *t;

	if (chan == NULL)
		return;

	save_flags(flags);
	cli();
	spinLock(&schedulerSpinLock);

	for (t = taskList; t != NULL; t = t->next)
	{
		if (t->wait_chan != chan)
			continue;

		t->wait_chan = NULL;
		callout_stop(&t->sleep_callout); /* cancel pending timeout — woken by signal */

		/* Mirror sched_io_wakeup()'s boost, but inline so the whole scan
		 * runs under a single lock acquisition. */
		if (t->state == WAIT || t->state == UNINTERRUPTIBLE || t->state == INTERRUPTIBLE)
		{
			u_int8_t boosted = (u_int8_t)(t->base_priority + 4);
			if (boosted > 23)
				boosted = 23;
			if (boosted > t->priority)
				t->priority = boosted;
			t->boost_quanta = 2;
			t->state = READY;
			rq_enqueue_locked(t);
		}
	}

	spinUnlock(&schedulerSpinLock);
	restore_flags(flags);
}
