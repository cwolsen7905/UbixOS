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

#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>
#include <ubixos/spinlock.h>
#include <ubixos/vitals.h>
#include <ubixos/endtask.h>
#include <isa/atkbd.h>
#include <isa/pit.h>
#include <isa/8259.h>
#include <sys/gdt.h>
#include <sys/shutdown.h>
#include <lib/kprintf.h>

void sched() {
  uint32_t memAddr = 0x0;
  kTask_t *tmpTask = 0x0;
  kTask_t *delTask = 0x0;

  /* Reboot countdown: Ctrl-M sets reboot_at_tick; we print once per second
   * and reboot when time is up. Runs before the spinlock to keep it simple. */
  if (reboot_at_tick != 0) {
    uint32_t now  = systemVitals->sysTicks;
    uint32_t left = (reboot_at_tick > now) ? (reboot_at_tick - now) : 0;
    uint32_t secs = (left + PIT_TIMER - 1) / PIT_TIMER;
    static uint32_t last_printed = 0;

    if (secs != last_printed) {
      last_printed = secs;
      if (secs > 0)
        kprintf("Rebooting in %u...\n", secs);
    }
    if (left == 0) {
      kprintf("Rebooting now.\n");
      reboot_at_tick = 0;
      sys_shutdown(REBOOT);
    }
  }

  if (spinTryLock(&schedulerSpinLock))
    return;

  tmpTask = ((_current == 0) ? 0 : _current->next);
  schedStart:

  for (; tmpTask != 0x0; tmpTask = tmpTask->next) {
    if (tmpTask->state == FORK)
      tmpTask->state = READY;

    if (tmpTask->state == READY) {
      if (_current != NULL)
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
        if (delTask->term != NULL && delTask->term->owner == delTask->id)
          delTask->term->owner = delTask->parent->id;
      }

      tmpTask = tmpTask->next;

      sched_deleteTask(delTask->id);
      sched_addDelTask(delTask);

      goto schedStart;
    }
  }

  if (0x0 == tmpTask) {
    tmpTask = taskList;
    goto schedStart;
  }

  if (_current->state == READY || _current->state == RUNNING) {

    if (_current->oInfo.v86Task == 0x1)
      irqDisable(0x0);  /* mask timer while v86 task runs; irqEnable(0) on INT 0x69 exit */

    asm("cli");

    memAddr = (uint32_t) &(_current->md.md_tss);
    ubixGDT[4].descriptor.baseLow = (memAddr & 0xFFFF);
    ubixGDT[4].descriptor.baseMed = ((memAddr >> 16) & 0xFF);
    ubixGDT[4].descriptor.baseHigh = (memAddr >> 24);
    ubixGDT[4].descriptor.access = '\x89';

    _current->state = RUNNING;

    spinUnlock(&schedulerSpinLock);

    asm("ljmp $0x20,$0");
    /* The outgoing task resumes here on its next scheduling slot.
     * ljmp saved EFLAGS with IF=0 (from cli above) into its TSS, so
     * we must re-enable interrupts explicitly here rather than before
     * the ljmp (which would create a race window). */
    asm("sti");
  }
  else {
    spinUnlock(&schedulerSpinLock);
  }

  return;
}

void schedEndTask(pidType pid) {
  endTask(_current->id);
  sched_yield();
}

void sched_yield() {
  sched();
}
