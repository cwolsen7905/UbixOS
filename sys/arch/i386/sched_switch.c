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
#include <i386/signal.h>
#include <ubixos/spinlock.h>
#include <ubixos/vitals.h>
#include <ubixos/endtask.h>
#include <isa/atkbd.h>
#include <isa/pit.h>
#include <isa/8259.h>
#include <sys/gdt.h>
#include <sys/shutdown.h>
#include <lib/kprintf.h>

/* Starvation aging: scan every AGING_INTERVAL ticks (~50 ms at 200 Hz).
 * Boost tasks that haven't run in > AGING_STARVE ticks (~200 ms). */
#define AGING_INTERVAL  10
#define AGING_STARVE    40

static inline uint8_t
quantum_for_priority(uint8_t pri)
{
  if (pri >= 24) return 0;   /* High/Realtime: unlimited */
  if (pri >= 16) return 10;  /* Interactive */
  if (pri >= 8)  return 6;   /* Normal */
  if (pri >= 1)  return 2;   /* Background */
  return 1;                  /* Idle */
}

void sched() {
  uint32_t memAddr = 0x0;
  kTask_t *delTask = 0x0;
  kTask_t *next    = 0x0;
  kTask_t *t       = 0x0;

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

  /* --- Phase 3.4: starvation aging — scan every ~50 ms --- */
  {
    static uint32_t aging_last = 0;
    uint32_t        now = systemVitals->sysTicks;
    if (now - aging_last >= AGING_INTERVAL) {
      kTask_t *tmp;
      aging_last = now;
      for (tmp = taskList; tmp != NULL; tmp = tmp->next) {
        uint8_t cap;
        if (tmp->state != READY || !tmp->on_rq || tmp->last_run_tick == 0)
          continue;
        if (now - tmp->last_run_tick < AGING_STARVE)
          continue;
        /* +1 boost, capped at base_priority + 8, never into interactive band.
         * Must dequeue/re-enqueue because rq_dequeue_locked keyes on
         * t->priority — changing it in-place corrupts the run-queue buckets. */
        cap = tmp->base_priority + 8;
        if (cap > 23) cap = 23;
        if (tmp->priority < cap) {
          rq_dequeue_locked(tmp);
          tmp->priority++;
          tmp->state = READY;
          rq_enqueue_locked(tmp);
        }
      }
    }
  }

  /* --- Phase 2: dead-task cleanup (separate from dispatch) --- */
  t = taskList;
  while (t != 0x0) {
    if (t->state == ZOMBIE) {
      /*
       * ZOMBIE: task exited normally via endTask.  Notify the parent and
       * transition to DEAD — but leave the task in taskList so the parent's
       * wait_find_child() can collect it and free it.
       */
      delTask = t;
      t = t->next;

      if (delTask->parent != 0x0) {
        delTask->parent->children -= 1;
        delTask->parent->last_exit = delTask->id;
        if (delTask->parent->state != DEAD && delTask->parent->state != ZOMBIE) {
          delTask->parent->td.sig_pending |= (1u << (SIGCHLD - 1));
          if (delTask->parent->state != READY) {
            delTask->parent->state = READY;
            rq_enqueue_locked(delTask->parent);
          }
        }
        if (delTask->term != NULL && delTask->term->owner == delTask->id)
          delTask->term->owner = delTask->parent->id;
        if (delTask->term != NULL &&
            delTask->term->t_pgrp == (pid_t)delTask->pgrp)
          delTask->term->t_pgrp = 0;
      }
      /* ZOMBIE → DEAD: wait_find_child() will splice from taskList. */
      delTask->state = DEAD;

    } else if (t->state == DEAD) {
      /*
       * DEAD with no living parent (fault-killed, exec-error, or after
       * wait_find_child already spliced the task out of parent's view).
       * Only auto-collect when the parent is gone; otherwise wait_find_child
       * handles collection so we don't race with it.
       */
      delTask = t;
      t = t->next;
      if (delTask->parent == 0x0 ||
          delTask->parent->state == DEAD ||
          delTask->parent->state == ZOMBIE) {
        if (delTask->prev != 0x0) delTask->prev->next = delTask->next;
        else                      taskList             = delTask->next;
        if (delTask->next != 0x0) delTask->next->prev  = delTask->prev;
        pid_hash_remove(delTask);
        sched_addDelTask(delTask);
      }

    } else {
      t = t->next;
    }
  }

  /* --- Phase 2: O(1) dispatch via ready_mask --- */

  if (_current != NULL && _current->state == RUNNING) {
    uint8_t pri = _current->priority;

    if (pri >= 24) {
      /* High/Realtime: never preempt — runs until it voluntarily blocks. */
      spinUnlock(&schedulerSpinLock);
      return;
    }

    /* Decrement time slice; skip switch if the task still has ticks left. */
    if (_current->quantum > 0)
      _current->quantum--;
    if (_current->quantum > 0) {
      spinUnlock(&schedulerSpinLock);
      return;
    }

    /* Quantum expired — apply priority adjustments before re-enqueue. */
    if (_current->boost_quanta > 0) {
      /* I/O boost is still active: count down. */
      _current->boost_quanta--;
      if (_current->boost_quanta == 0)
        /* Boost expired: drop back to the QoS floor. */
        _current->priority = _current->base_priority;
    } else if (_current->priority > _current->base_priority) {
      /* CPU-bound decay: −2 per expired quantum (floor: base_priority). */
      uint8_t decayed = (uint8_t)(_current->priority - 2);
      _current->priority = (decayed > _current->base_priority)
          ? decayed : _current->base_priority;
    }
    _current->quantum = quantum_for_priority(_current->priority);
    _current->state   = READY;
    rq_enqueue_locked(_current);
  }

  /* Nothing ready — return without switching. */
  if (ready_mask == 0) {
    spinUnlock(&schedulerSpinLock);
    return;
  }

  /* Pick the highest-priority ready task (highest set bit). */
  {
    int pri = 31 - __builtin_clz(ready_mask);
    /* run_queue[pri] is a circular list; head is the next to run. */
    next = run_queue[pri];
    /* Dequeue (removes next and rotates head to next->rq_next). */
    rq_dequeue_locked(next);
  }

  _current = next;
  _current->last_run_tick = systemVitals->sysTicks;

  /* Give the newly dispatched task a fresh time slice if it has none left. */
  if (_current->quantum == 0)
    _current->quantum = quantum_for_priority(_current->priority);

  if (_current->oInfo.v86Task == 0x1)
    irqDisable(0x0);  /* mask timer while v86 task runs */

  asm("cli");

  memAddr = (uint32_t) &(_current->md.md_tss);
  ubixGDT[4].descriptor.baseLow  = (memAddr & 0xFFFF);
  ubixGDT[4].descriptor.baseMed  = ((memAddr >> 16) & 0xFF);
  ubixGDT[4].descriptor.baseHigh = (memAddr >> 24);
  ubixGDT[4].descriptor.access   = '\x89';

  _current->state = RUNNING;

  spinUnlock(&schedulerSpinLock);

  asm("ljmp $0x20,$0");
  /* The outgoing task resumes here on its next scheduling slot.
   * ljmp saved EFLAGS with IF=0 (from cli above) into its TSS, so
   * we must re-enable interrupts explicitly here. */
  asm("sti");

  return;
}

void schedEndTask(pidType pid) {
  endTask(_current->id);
  sched_yield();
}

void sched_yield() {
  /* Force an immediate switch by expiring the current quantum. */
  if (_current != NULL)
    _current->quantum = 0;
  sched();
}
