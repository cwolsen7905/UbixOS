/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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
#include <ubixos/endtask.h>
#include <ubixos/wait.h>
#include <vfs/mount.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <vmm/vmm.h>
#include <sys/gdt.h>
#include <sys/idt.h>
#include <isa/8259.h>
#include <string.h>
#include <assert.h>
#include <sys/descrip.h>

#include <ubixos/spinlock.h>

static kTask_t *taskList = 0x0;
static kTask_t *delList = 0x0;
static uint32_t nextID = 1;

kTask_t *_current = 0x0;
kTask_t *_usedMath = 0x0;

static struct spinLock schedulerSpinLock = SPIN_LOCK_INITIALIZER;

int need_resched = 0;

/************************************************************************

 Function: int sched_init()

 Description: This function is used to enable the kernel scheduler

 Notes:

 02/20/2004 - Approved for quality

 ************************************************************************/

int sched_init() {
  taskList = (kTask_t *) kmalloc(sizeof(kTask_t));
  if (taskList == 0x0)
    kpanic("Unable to create task list");

  taskList->id = nextID++;

  /* Print out information on scheduler */
  kprintf("sched0 - Address: [0x%X]\n", taskList);

  /* Return so we know everything went well */
  return (0x0);
}

void sched() {
  uint32_t memAddr = 0x0;
  kTask_t *tmpTask = 0x0;
  kTask_t *delTask = 0x0;

  if (spinTryLock(&schedulerSpinLock))
    return;

  tmpTask = ((_current == 0) ? 0 : _current->next);
  schedStart:

  /* Yield the next task from the current prio queue */
  for (; tmpTask != 0x0; tmpTask = tmpTask->next) {
    if (tmpTask->state == FORK)
      tmpTask->state = READY;

    if (tmpTask->state == READY) {
      _current->state = (_current->state == DEAD) ? DEAD : READY;
      _current = tmpTask;
      break;
    }
    else if (tmpTask->state == DEAD) {
      delTask = tmpTask;
      if (delTask->parent != 0x0) {
        delTask->parent->children -= 1;
        delTask->parent->last_exit = delTask->id;
        delTask->parent->state = READY;
      }

      tmpTask = tmpTask->next;

            sched_deleteTask(delTask->id);
            sched_addDelTask(delTask);

      goto schedStart;
    }
  }

  /* Finished all the tasks, restarting the list */
  if (0x0 == tmpTask) {
    tmpTask = taskList;
    goto schedStart;
  }

  if (_current->state == READY || _current->state == RUNNING) {

    if (_current->oInfo.v86Task == 0x1) {
      irqDisable(0x0);
      kprintf("IRQD(%i): 0x%X*0x%X:0x%X@, esp: 0x%X:0x%X, ebp: 0x%X:0x%X ds: 0x%X", _current->id, _current->tss.eflags, _current->tss.cs, _current->tss.eip, _current->tss.ss, _current->tss.esp, _current->tss.ss, _current->tss.ebp,_current->tss.ds);
      kprintf("ss0: 0x%X, esp0: 0x%X", _current->tss.ss0, _current->tss.esp0);
    }

    asm("cli");

    memAddr = (uint32_t) &(_current->tss);
    ubixGDT[4].descriptor.baseLow = (memAddr & 0xFFFF);
    ubixGDT[4].descriptor.baseMed = ((memAddr >> 16) & 0xFF);
    ubixGDT[4].descriptor.baseHigh = (memAddr >> 24);
    ubixGDT[4].descriptor.access = '\x89';

    _current->state = RUNNING;

    spinUnlock(&schedulerSpinLock);

    asm("sti");
    asm("ljmp $0x20,$0");
  }
  else {
    spinUnlock(&schedulerSpinLock);
  }

  return;
}

kTask_t *schedNewTask() {
  int i = 0;

  kTask_t *tmpTask = (kTask_t *) kmalloc(sizeof(kTask_t));

  struct file *fp = 0x0;

  if (tmpTask == 0x0)
    kpanic("Error: schedNewTask() - kmalloc failed trying to initialize a new task struct\n");

  memset(tmpTask, 0x0, sizeof(kTask_t));

  /* Filling in tasks attrs */
  tmpTask->usedMath = 0x0;
  tmpTask->state = NEW;

  memcpy(tmpTask->username, "UbixOS", 6);

  /* HACK */
  for (i = 0; i < 3; i++) {
    fp = (void *) kmalloc(sizeof(struct file));
    //kprintf("DB: [0x%X]\n", (uint32_t) fp);
    tmpTask->td.o_files[i] = (void *) fp;
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

int sched_deleteTask(pidType id) {
  kTask_t *tmpTask = 0x0;

  /* Checking each task from the prio queue */
  for (tmpTask = taskList; tmpTask != 0x0; tmpTask = tmpTask->next) {
    if (tmpTask->id == id) {
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

int sched_addDelTask(kTask_t *tmpTask) {
  tmpTask->next = delList;
  tmpTask->prev = 0x0;
  if (delList != 0x0)
    delList->prev = tmpTask;
  delList = tmpTask;
  return (0x0);
}

kTask_t *sched_getDelTask() {
  kTask_t *tmpTask = 0x0;

  if (delList == 0x0)
    return (0x0);

  tmpTask = delList;
  delList = delList->next;
  return (tmpTask);
}

kTask_t *schedFindTask(uint32_t id) {
  kTask_t *tmpTask = 0x0;

  for (tmpTask = taskList; tmpTask; tmpTask = tmpTask->next) {
    if (tmpTask->id == id)
      return (tmpTask);
  }

  return (0x0);
}

/************************************************************************

 Function: void schedEndTask()

 Description: This function will end a task

 Notes:

 02/20/2004 - Approved for quality

 ************************************************************************/
void schedEndTask(pidType pid) {
  endTask(_current->id);
  sched_yield();
}

/************************************************************************

 Function: int schedEndTask()

 Description: This function will yield a task

 Notes:

 02/20/2004 - Approved for quality

 ************************************************************************/

void sched_yield() {
  sched();
}

/*
 asm(
 ".globl sched_yield \n"
 "sched_yield:       \n"
 "  cli              \n"
 "  call sched       \n"
 );
 */

/************************************************************************

 Function: int sched_setStatus(pidType pid,tState state)

 Description: Change the tasks status

 Notes:

 ************************************************************************/
int sched_setStatus(pidType pid, tState state) {
  kTask_t *tmpTask = schedFindTask(pid);
  if (tmpTask == 0x0)
    return (0x1);
  tmpTask->state = state;
  return (0x0);
}

void add_wait_queue(struct wait_queue ** p, struct wait_queue * wait) {
  unsigned long flags;

  save_flags(flags);
  cli();
  if (!*p) {
    wait->next = wait;
    *p = wait;
  }
  else {
    wait->next = (*p)->next;
    (*p)->next = wait;
  }
  restore_flags(flags);
}

void remove_wait_queue(struct wait_queue ** p, struct wait_queue * wait) {
  unsigned long flags;
  struct wait_queue * tmp;

  save_flags(flags);
  cli();
  if ((*p == wait) && ((*p = wait->next) == wait)) {
    *p = NULL;
  }
  else {
    tmp = wait;
    while (tmp->next != wait) {
      tmp = tmp->next;
    }
    tmp->next = wait->next;
  }
  wait->next = NULL;
  restore_flags(flags);
}

void wake_up_interruptible(struct wait_queue **q) {
  struct wait_queue *tmp;
  kTask_t *p;

  if (!q || !(tmp = *q))
    return;
  do {
    if ((p = tmp->task) != NULL) {
      if (p->state == INTERRUPTIBLE) {
        p->state = RUNNING;
        if (p->counter > _current->counter)
          need_resched = 1;
      }
    }
    if (!tmp->next) {
      kprintf("wait_queue is bad (eip = %08lx)\n", ((unsigned long *) q)[-1]);
      kprintf("        q = %p\n", q);
      kprintf("       *q = %p\n", *q);
      kprintf("      tmp = %p\n", tmp);
      break;
    }
    tmp = tmp->next;
  } while (tmp != *q);
}

void wake_up(struct wait_queue **q) {
  struct wait_queue *tmp;
  kTask_t * p;

  if (!q || !(tmp = *q))
    return;
  do {
    if ((p = tmp->task) != NULL) {
      if ((p->state == UNINTERRUPTIBLE) || (p->state == INTERRUPTIBLE)) {
        p->state = RUNNING;
        if (p->counter > _current->counter)
          need_resched = 1;
      }
    }
    if (!tmp->next) {
      kprintf("wait_queue is bad (eip = %08lx)\n", ((unsigned long *) q)[-1]);
      kprintf("        q = %p\n", q);
      kprintf("       *q = %p\n", *q);
      kprintf("      tmp = %p\n", tmp);
      break;
    }
    tmp = tmp->next;
  } while (tmp != *q);
}
