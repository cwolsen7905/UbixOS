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

#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <ubixos/endtask.h>
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
static uInt32 nextID = -1;

kTask_t *_current = 0x0;
kTask_t *_usedMath = 0x0;

static struct spinLock schedulerSpinLock = SPIN_LOCK_INITIALIZER;

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
  uInt32 memAddr = 0x0;
  kTask_t *tmpTask = 0x0;
  kTask_t *delTask = 0x0;

  if (!spinTryLock(&schedulerSpinLock))
    return;

  tmpTask = _current->next;
  //outportByte(0xE9,_current->id + '0');
  schedStart:

  /* Yield the next task from the current prio queue */
  for (; tmpTask != 0x0; tmpTask = tmpTask->next) {
    if (tmpTask->state > 0x0) {
      _current = tmpTask;
      if (_current->state == FORK)
        _current->state = READY;
      break;
    }
    else if (tmpTask->state == DEAD) {
      delTask = tmpTask;
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

  if (_current->state > 0x0) {
    if (_current->oInfo.v86Task == 0x1)
      irqDisable(0x0);
    asm("cli");
    memAddr = (uInt32) &(_current->tss);
    ubixGDT[4].descriptor.baseLow = (memAddr & 0xFFFF);
    ubixGDT[4].descriptor.baseMed = ((memAddr >> 16) & 0xFF);
    ubixGDT[4].descriptor.baseHigh = (memAddr >> 24);
    ubixGDT[4].descriptor.access = '\x89';
    spinUnlock(&schedulerSpinLock);
    asm("sti");
    asm("ljmp $0x20,$0\n");
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

  /* HACK */
  /* XXX MrOlsen (2020-01-28): Why did I allocate them? This is not needed just waste of memory?
  for (i = 0; i < 3; i++) {
    fp = (void *) kmalloc(sizeof(struct file));
    tmpTask->td.o_files[i] = (uint32_t) fp;
    fp->f_flag = 0x4;
  }
   */

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

kTask_t *
schedFindTask(uInt32 id) {
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

/***
 END
 ***/

