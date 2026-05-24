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

#include <machine/proc.h>
#include <sys/thread.h>

#define NO_GROUP -1
#define NR_GROUPS 32

typedef enum {
  PLACEHOLDER = -2, DEAD = -1, NEW = 0, READY = 1, RUNNING = 2, IDLE = 3, WAIT = 5, UNINTERRUPTIBLE = 6, INTERRUPTIBLE = 7,
  STOPPED = 8,
  ZOMBIE  = 9   /* exited, awaiting parent wait() collection */
} tState;

struct osInfo {
    uInt8 timer;
    uInt8 v86Task;
    bool v86If;
    uInt32 vmStart;
    uInt32 stdinSize;
    uInt32 controlKeys;
    char *stdin;
    char cwd[1024]; /* current working dir */
    uint8_t gpf;
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
    uint32_t uid, gid;
    uint16_t euid, suid;
    uint16_t egid, sgid;
    uInt16 usedMath;
    tty_term *term;
    struct thread td;
    struct {
        struct inode *pwd;
        struct inode *root;
        struct inode *exec;
    } inodes;
    uint32_t counter;
    uint16_t groups[NR_GROUPS];
    pidType ppid;
    uint32_t pgrp;
    uint32_t sid;        /* session ID — set by setsid(), inherited by fork */
    tty_term *ct_tty;   /* controlling terminal — set by TIOCSCTTY, cleared by setsid() */
    uint32_t children; // Hack for WAIT
    uint32_t last_exit; // Hack For WAIT
    struct taskStruct *parent;
    char username[256];
    uint32_t *kernelStack;
    struct taskStruct *hash_next; /* PID hash chain — Phase 1.5 */
    uint8_t   quantum;            /* ticks remaining in current time slice */
    uint8_t   priority;          /* current scheduling priority (0–31) */
    uint8_t   base_priority;     /* QoS floor — boosts never go below this */
    uint8_t   boost_quanta;      /* ticks remaining on temporary I/O priority boost */
    uint8_t   on_rq;             /* 1 if currently in a run queue */
    struct taskStruct *rq_next;  /* per-priority run queue forward link */
    struct taskStruct *rq_prev;  /* per-priority run queue backward link */
    int       t_stopped_sig;     /* signal that caused STOPPED state (0 if not stopped) */
    uint32_t  last_run_tick;     /* sysTicks when last dispatched (starvation aging) */
} kTask_t;

/*
 * QoS classes — stored as base_priority.  The scheduler never drops a task
 * below its QoS floor.  Inspired by macOS DISPATCH_QOS_CLASS_*.
 */
typedef enum {
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
void sched_pi_boost(kTask_t *t, uint8_t pri);  /* PI: raise t to pri if higher      */
void sched_pi_restore(kTask_t *t);             /* PI: drop t back to base_priority  */

void schedEndTask(pidType pid);
kTask_t *schedNewTask();
kTask_t *schedFindTask(uInt32 id);

extern kTask_t *_current;
extern kTask_t *_usedMath;

#ifdef __cplusplus
}
#endif


#endif
