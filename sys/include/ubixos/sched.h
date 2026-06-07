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

#ifndef _UBIXOS_SCHED_H
#define _UBIXOS_SCHED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <fs/vfs/file.h>
#include <ubixos/tty.h>
#include <ubixos/callout.h>

#include <machine/proc.h>
#include <sys/thread.h>
#include <vmm/vm_map.h>

#define NO_GROUP -1
#define NR_GROUPS 32

typedef enum {
  PLACEHOLDER = -2, DEAD = -1, NEW = 0, READY = 1, RUNNING = 2, IDLE = 3, WAIT = 5, UNINTERRUPTIBLE = 6, INTERRUPTIBLE = 7,
  STOPPED = 8,
  ZOMBIE  = 9   /* exited, awaiting parent wait() collection */
} tState;

struct osInfo {
    u_int8_t timer;
    u_int8_t v86Task;
    bool v86If;
    u_int32_t vmStart;
    u_int32_t stdinSize;
    u_int32_t controlKeys;
    char *stdin;
    char cwd[1024]; /* current working dir */
    u_int8_t gpf;
};

typedef struct taskStruct {
    pidType id;
    char name[256];
    char cmdline[1024];
    struct taskStruct *prev;
    struct taskStruct *next;
    struct md_proc md;
    struct osInfo oInfo;
    //fileDescriptor *imageFd;
    fileDescriptor_t *files[MAX_OFILES];
    tState state;
    u_int32_t uid, gid;
    u_int16_t euid, suid;
    u_int16_t egid, sgid;
    u_int16_t usedMath;
    tty_term *term;
    struct thread td;
    struct {
        struct inode *pwd;
        struct inode *root;
        struct inode *exec;
    } inodes;
    u_int32_t counter;
    u_int16_t groups[NR_GROUPS];
    pidType ppid;
    u_int32_t pgrp;
    u_int32_t sid;        /* session ID — set by setsid(), inherited by fork */
    tty_term *ct_tty;   /* controlling terminal — set by TIOCSCTTY, cleared by setsid() */
    u_int32_t children; // Hack for WAIT
    u_int32_t last_exit; // Hack For WAIT
    struct taskStruct *parent;
    char username[256];
    u_int32_t *kernelStack;
    struct taskStruct *hash_next; /* PID hash chain — Phase 1.5 */
    u_int8_t   quantum;            /* ticks remaining in current time slice */
    u_int8_t   priority;          /* current scheduling priority (0–31) */
    u_int8_t   base_priority;     /* QoS floor — boosts never go below this */
    u_int8_t   boost_quanta;      /* ticks remaining on temporary I/O priority boost */
    u_int8_t   on_rq;             /* 1 if currently in a run queue */
    struct taskStruct *rq_next;  /* per-priority run queue forward link */
    struct taskStruct *rq_prev;  /* per-priority run queue backward link */
    int       t_stopped_sig;     /* signal that caused STOPPED state (0 if not stopped) */
    void      *wait_chan;        /* sleep/wakeup channel: address slept on, NULL if not blocked */
    struct callout sleep_callout; /* timed-sleep timeout (armed by sched_wait_event_timeout) */
    u_int32_t  last_run_tick;     /* sysTicks when last dispatched (starvation aging) */
    vm_map_t  vm_map;            /* VMA red-black tree — O(log n) mmap/fault lookup */
    pidType    tgid;             /* thread-group id (leader's pid); == id for a normal process.
                                  * Threads created via rfork(RFMEM) share the leader's tgid and
                                  * its address space (cr3); endTask tears the address space down
                                  * only when the last task of a tgid exits. */
    u_int8_t   reap_free_as;     /* 1 = this task's reap frees the (shared) page tables/dir.
                                  * Set by endTask: 1 for a normal process or the last thread of a
                                  * tgid, 0 for a non-last thread (whose siblings still use the AS). */
    u_int32_t  tls_base;         /* per-thread userland TLS base (the %gs:0 self-pointer).
                                  * Installed into the shared LDT[1] descriptor by set_thread_area
                                  * and re-installed by cpu_switch on every resume: all threads in
                                  * an address space share one LDT[1] slot, so each switch must
                                  * restore the resuming thread's own base.  0 = no TLS (kernel
                                  * threads, or a process before its first set_thread_area). */
    int       *clear_tid;        /* CLONE_CHILD_CLEARTID: user address the kernel zeroes and
                                  * futex-wakes when this thread exits (endTask).  musl uses it to
                                  * release the thread-list lock held across pthread_exit.  NULL for
                                  * a normal process or a thread created without the flag. */
} kTask_t;

/*
 * QoS classes — stored as base_priority.  The scheduler never drops a task
 * below its QoS floor.  Inspired by macOS DISPATCH_QOS_CLASS_*.
 */
typedef enum {
    QOS_IDLE             =  0,  /* per-system idle thread — runs only when nothing else is ready */
    QOS_BACKGROUND       =  4,  /* maintenance work, automountd */
    QOS_UTILITY          =  8,  /* compilation, long-running tools */
    QOS_DEFAULT          = 12,  /* default — inherited from parent */
    QOS_USER_INITIATED   = 18,  /* action the user explicitly started */
    QOS_USER_INTERACTIVE = 22,  /* direct UI interaction */
    QOS_KERNEL           = 24,  /* kernel threads and device drivers */
    QOS_REALTIME         = 31,  /* hard-deadline (fixed, never decayed) */
} qos_class_t;

int sched_init();
int sched_setStatus(pidType, tState);
void sched_killTree(pidType);
int sched_deleteTask(pidType);
int sched_addDelTask(kTask_t *);
kTask_t *sched_getDelTask();
int sched_tgid_others_alive(pidType tgid, pidType self); /* other live threads sharing the AS */
void sched_yield();
void sched();

/* Scheduler state-transition API — always use these instead of direct
 * task->state assignments.  Phase 2 will add run-queue management here. */
void sched_ready(kTask_t *t);           /* READY  — task is runnable        */
void sched_dead(kTask_t *t);            /* DEAD   — task has exited          */
void sched_sleep(kTask_t *t, tState s); /* WAIT / UNINTERRUPTIBLE — blocked  */
void sched_wakeup(kTask_t *t);          /* RUNNING — unblocked, back to work */
void sched_stop(kTask_t *t, int sig);   /* STOPPED — suspended by signal     */
void sched_zombie(kTask_t *t);          /* ZOMBIE  — exited, awaiting wait() */
void sched_io_wakeup(kTask_t *t);       /* I/O done: boost +4, re-enqueue    */
void sched_pi_boost(kTask_t *t, u_int8_t pri);  /* PI: raise t to pri if higher      */

/*
 * Wait-channel sleep/wakeup (replaces sched_yield() busy-wait loops).
 *
 * sched_wait_event(chan, cond, arg): block _current on the address `chan` until
 * cond(arg) returns nonzero, truly leaving the run queue (CPU goes idle instead
 * of spinning).  cond is re-checked with interrupts disabled before each sleep,
 * so it is race-free against a waker that sets the condition then calls
 * sched_wakeup_chan(chan) — including a waker in interrupt context.
 *
 * sched_wakeup_chan(chan): make every task sleeping on `chan` runnable again
 * (I/O-boosted).  Safe to call from an ISR: it never yields (schedulerSpinLock
 * is only ever held with interrupts disabled, so it is free when an IRQ fires).
 */
void sched_wait_event(void *chan, int (*cond)(void *arg), void *arg);
void sched_wakeup_chan(void *chan);

/*
 * Like sched_wait_event() but also returns after `ticks` scheduler ticks even
 * if cond never holds (the timer's per-tick scan wakes timed sleepers).  Needed
 * for lwIP's tcpip_thread, which waits with a deadline to drive protocol timers.
 * @return 0 if cond was satisfied, 1 if the timeout expired first.
 */
int sched_wait_event_timeout(void *chan, int (*cond)(void *arg), void *arg, u_int32_t ticks);

/* Permanently set a task's QoS floor + current priority (re-homing it in the
 * run queue if enqueued).  Used to drop the idle thread to QOS_IDLE. */
void sched_set_priority(kTask_t *t, u_int8_t pri);
void sched_pi_restore(kTask_t *t);             /* PI: drop t back to base_priority  */

void schedEndTask(pidType pid);
kTask_t *schedNewTask();
kTask_t *schedFindTask(u_int32_t id);

/*
 * _current — the thread running on the calling CPU.
 *
 * On i386 this is per-CPU state living in g_pcpu[cpu].current and reached
 * through %gs (SEL_PCPU): %gs:8 == &g_pcpu[cpu].current (offset 8 — see
 * <i386/pcpu.h>).  Every kernel entry stub loads %gs = SEL_PCPU and
 * cpu_switch() re-establishes it on each switch, so %gs:8 is valid in every
 * kernel context.  Reads go through get_current(); the single write site
 * (sched()) uses set_current().  This is a bare %gs-relative load/store, NOT a
 * macro that injects a function call into asm paths — an earlier curcpu()->current
 * attempt was reverted for exactly that reason.
 *
 * Bootstrap: pcpu_install_gs(0) patches the SEL_PCPU GDT base to &g_pcpu[0] at
 * the very top of kmain, before any _current access, and g_pcpu is zeroed BSS so
 * current starts NULL.
 */
#ifdef __i386__
static inline kTask_t *get_current(void)
{
	kTask_t *p;
	__asm__ __volatile__("movl %%gs:8, %0" : "=r"(p));
	return p;
}

static inline void set_current(kTask_t *t)
{
	__asm__ __volatile__("movl %0, %%gs:8" : : "r"(t) : "memory");
}

#define _current (get_current())
#else
extern kTask_t *_current;
#define set_current(t) (_current = (t))
#endif

extern kTask_t *_usedMath;

#ifdef __cplusplus
}
#endif


#endif
