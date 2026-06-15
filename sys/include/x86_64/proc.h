/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 machine-dependent per-process state embedded in kTask_t (the 'md'
 * field).  No hardware TSS-based task switching (unlike i386): a software
 * context frame, like aarch64.  Reached via <machine/proc.h>.  Minimal during
 * bring-up; grows as the scheduler / context switch are ported.
 */
#ifndef _X86_64_PROC_H
#define _X86_64_PROC_H

#include <sys/types.h>

struct md_proc
{
	u_int64_t md_kstack;    /* saved kernel RSP (callee-saved frame; cpu_switch)        */
	u_int64_t md_cr3;       /* user address-space root (CR3/PML4 phys), 0 for kernel    */
	u_int64_t md_entry;     /* initial RIP: kernel-thread entry, or ring-3 entry        */
	u_int64_t md_usp;       /* user stack pointer; non-zero marks a ring-3 (user) task  */
	u_int64_t md_mmap_next; /* bump pointer for anonymous mmap (0 = uninit)             */
	u_int64_t md_brk;       /* current program break for brk()/sbrk() (0 = uninit)      */
	u_int64_t md_fsbase;    /* user TLS base (FS.base); saved/restored per task         */
	u_int64_t md_arg;       /* first-dispatch arg for a kernel thread (passed in rdi)   */
};

struct taskStruct; /* == kTask_t */

void md_new_task(struct taskStruct *t);
void md_setup_initial_frame(struct taskStruct *t);
void switch_to(struct taskStruct *prev, struct taskStruct *next);
void md_sched_pre_switch(struct taskStruct *t);

/* Scheduler tick rate (Hz) — must match the timer programming (Phase 4). */
#define SCHED_HZ 100

/* Re-establish per-CPU register base (i386: %gs; x86_64 SMP: GS.base).  No-op
 * during uniprocessor bring-up. */
static inline void machine_pcpu_reload(void)
{
}

/* Halt the CPU until the next interrupt. */
static inline void machine_idle(void)
{
	__asm__ __volatile__("hlt");
}

#endif /* _X86_64_PROC_H */
