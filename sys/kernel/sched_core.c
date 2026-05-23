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
#include <ubixos/spinlock.h>
#include <ubixos/wait.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>
#include <assert.h>
#include <sys/descrip.h>
#include <sys/resource.h>

/* Shared with sched_switch.c via sched_internal.h — not static. */
kTask_t *taskList = 0x0;
struct spinLock schedulerSpinLock = SPIN_LOCK_INITIALIZER;

/* Phase 2: 32 per-priority run queues + bitmask (Windows ReadySummary trick). */
kTask_t  *run_queue[SCHED_PRIORITIES];
uint32_t  ready_mask = 0;

static kTask_t *delList = 0x0;
static uint32_t nextID = 1;

/* Phase 1.5 — 256-bucket PID hash for O(1) schedFindTask. */
#define SCHED_HASH_BUCKETS 256
static kTask_t *pid_hash[SCHED_HASH_BUCKETS];

static inline void pid_hash_insert(kTask_t *t) {
	int b = t->id & (SCHED_HASH_BUCKETS - 1);
	t->hash_next = pid_hash[b];
	pid_hash[b] = t;
}

void pid_hash_remove(kTask_t *t) {
	int b = t->id & (SCHED_HASH_BUCKETS - 1);
	kTask_t **pp = &pid_hash[b];
	while (*pp) {
		if (*pp == t) { *pp = t->hash_next; return; }
		pp = &(*pp)->hash_next;
	}
}

kTask_t *_current = 0x0;
kTask_t *_usedMath = 0x0;

int need_resched = 0;

int sched_init()
{
	taskList = (kTask_t *)kmalloc(sizeof(kTask_t));
	if (taskList == 0x0)
		kpanic("Unable to create task list");

	memset(taskList, 0x0, sizeof(kTask_t));
	taskList->id = nextID++;
	taskList->quantum = 6;
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

	tmpTask->kernelStack = (uint32_t *)kmalloc(8192);
	if (tmpTask->kernelStack == 0x0)
		kpanic("Error: schedNewTask() - kmalloc failed allocating kernel stack\n");
	tmpTask->md.md_tss.esp0 = (uint32_t)tmpTask->kernelStack + 8192;
	tmpTask->md.md_tss.ss0 = 0x10;

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

	tmpTask->priority      = 12;  /* QOS_DEFAULT — mid Normal band */
	tmpTask->base_priority = 12;
	tmpTask->on_rq         = 0;

	spinLock(&schedulerSpinLock);
	tmpTask->id = nextID++;
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

void
rq_enqueue_locked(kTask_t *t)
{
	int pri;
	kTask_t *head;
	kTask_t *tail;

	if (t == NULL || t->on_rq)
		return;

	pri = (int)t->priority;
	head = run_queue[pri];

	if (head == NULL) {
		/* First task at this priority — circular singleton. */
		t->rq_next     = t;
		t->rq_prev     = t;
		run_queue[pri] = t;
		ready_mask    |= (1u << pri);
	} else {
		/* Append at the real tail (= just before head) for FIFO round-robin. */
		tail           = head->rq_prev;
		t->rq_next     = head;
		t->rq_prev     = tail;
		tail->rq_next  = t;
		head->rq_prev  = t;
		/* run_queue[pri] stays pointing at head for O(1) dequeue. */
	}
	t->on_rq = 1;
}

void
rq_dequeue_locked(kTask_t *t)
{
	int pri;

	if (t == NULL || !t->on_rq)
		return;

	pri = (int)t->priority;

	if (t->rq_next == t) {
		/* Only task in this queue. */
		run_queue[pri] = NULL;
		ready_mask    &= ~(1u << pri);
	} else {
		t->rq_prev->rq_next = t->rq_next;
		t->rq_next->rq_prev = t->rq_prev;
		if (run_queue[pri] == t)
			run_queue[pri] = t->rq_next;
	}
	t->rq_next = NULL;
	t->rq_prev = NULL;
	t->on_rq   = 0;
}

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
	if (delList != 0x0) {
		tmpTask = delList;
		delList = delList->next;
	}
	spinUnlock(&schedulerSpinLock);
	return (tmpTask);
}

kTask_t *schedFindTask(uint32_t id)
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
		do {
			if (tmp->next == wait) {
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
	uint32_t flags;
	if (t == NULL)
		return;
	save_flags(flags);
	cli();
	spinLock(&schedulerSpinLock);
	if (t->state != READY) {
		t->state = READY;
		rq_enqueue_locked(t);
	}
	spinUnlock(&schedulerSpinLock);
	restore_flags(flags);
}

void sched_dead(kTask_t *t)
{
	uint32_t flags;
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
	uint32_t flags;
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
