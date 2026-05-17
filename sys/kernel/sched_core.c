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
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <ubixos/wait.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>
#include <assert.h>
#include <sys/descrip.h>

/* Shared with sched_switch.c via sched_internal.h — not static. */
kTask_t *taskList = 0x0;
struct spinLock schedulerSpinLock = SPIN_LOCK_INITIALIZER;

static kTask_t *delList = 0x0;
static uint32_t nextID = 1;

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
	strncpy(taskList->name, "kernel", sizeof(taskList->name) - 1);

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

	tmpTask->kernelStack = (uint32_t *)kmalloc(4096);
	if (tmpTask->kernelStack == 0x0)
		kpanic("Error: schedNewTask() - kmalloc failed allocating kernel stack\n");
	tmpTask->md.md_tss.esp0 = (uint32_t)tmpTask->kernelStack + 4096;
	tmpTask->md.md_tss.ss0 = 0x10;

	tmpTask->usedMath = 0x0;
	tmpTask->state = NEW;

	memcpy(tmpTask->username, "UbixOS", 6);

	/* HACK */
	for (i = 0; i < 3; i++)
	{
		fp = (void *)kmalloc(sizeof(struct file));
		memset(fp, 0, sizeof(struct file));
		tmpTask->td.o_files[i] = (void *)fp;
		fp->f_flag = 0x4;
	}

	spinLock(&schedulerSpinLock);
	tmpTask->id = nextID++;
	tmpTask->next = taskList;
	tmpTask->prev = 0x0;
	taskList->prev = tmpTask;
	taskList = tmpTask;

	spinUnlock(&schedulerSpinLock);

	return (tmpTask);
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
	kTask_t *tmpTask = 0x0;

	for (tmpTask = taskList; tmpTask != 0x0; tmpTask = tmpTask->next)
	{
		if (tmpTask->id == id)
		{
			if (tmpTask->prev != 0x0)
				tmpTask->prev->next = tmpTask->next;
			if (tmpTask->next != 0x0)
				tmpTask->next->prev = tmpTask->prev;
			if (taskList == tmpTask)
				taskList = tmpTask->next;
			return (0x0);
		}
	}
	return (0x1);
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

	if (delList == 0x0)
		return (0x0);

	tmpTask = delList;
	delList = delList->next;
	return (tmpTask);
}

kTask_t *schedFindTask(uint32_t id)
{
	kTask_t *tmpTask = 0x0;

	for (tmpTask = taskList; tmpTask; tmpTask = tmpTask->next)
	{
		if (tmpTask->id == id)
			return (tmpTask);
	}

	return (0x0);
}

int sched_setStatus(pidType pid, tState state)
{
	kTask_t *tmpTask = schedFindTask(pid);
	if (tmpTask == 0x0)
		return (0x1);
	tmpTask->state = state;
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
		tmp = wait;
		while (tmp->next != wait)
		{
			tmp = tmp->next;
		}
		tmp->next = wait->next;
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
			{
				p->state = RUNNING;
				if (p->counter > _current->counter)
					need_resched = 1;
			}
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
			if ((p->state == UNINTERRUPTIBLE) || (p->state == INTERRUPTIBLE))
			{
				p->state = RUNNING;
				if (p->counter > _current->counter)
					need_resched = 1;
			}
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
