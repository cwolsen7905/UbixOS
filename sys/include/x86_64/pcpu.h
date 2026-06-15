/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 per-CPU block (smp-plan / x86_64 M0).  Reached via <x86_64/pcpu.h>.
 */
#ifndef _X86_64_PCPU_H
#define _X86_64_PCPU_H

#include <sys/types.h>

#define MAXCPU 8

struct taskStruct; /* == kTask_t; full type in <ubixos/sched.h> */

/*
 * One per core, reached through the GS segment base.  In long mode a `%gs:`-
 * prefixed memory access adds the GS_BASE MSR (0xC0000101) to the offset — the
 * %gs selector value is irrelevant — so the per-CPU block is installed by
 * writing GS_BASE once per CPU (x86_64_pcpu_install) and stays valid in every
 * kernel context.  (swapgs / KERNEL_GS_BASE only matter once EL0/userland exists;
 * the bring-up kernel runs entirely in the kernel, so plain GS_BASE suffices.)
 *
 * `self` (offset 8) lets curcpu() recover &g_pcpu[cpu] with a bare %gs load;
 * `current` MUST stay at offset 16 — <ubixos/sched.h>'s get_current()/
 * set_current() load/store [%gs:16] directly (no function call), matching
 * aarch64's [TPIDR_EL1+16].  The _Static_asserts below guard the layout.
 */
struct pcpu
{
	u_int32_t cpuid;              /* logical CPU index (0 = boot CPU) — offset 0 */
	u_int32_t apicid;             /* Local APIC id (hardware id) — offset 4 */
	struct pcpu *self;            /* &g_pcpu[cpuid], for curcpu() — offset 8 */
	struct taskStruct *current;   /* thread running on this CPU — offset 16 */
	struct taskStruct *idle;      /* this CPU's idle thread — offset 24 */
	volatile u_int32_t heartbeat; /* liveness counter an AP bumps before it schedules — 32 */
	u_int32_t _pad;               /* 36 */
	u_int64_t kernel_rsp;         /* current task's kernel-stack top — offset 40 (syscall entry) */
	u_int64_t user_rsp_scratch;   /* stash for the user RSP across syscall entry — offset 48 */
};

_Static_assert(__builtin_offsetof(struct pcpu, self) == 8, "struct pcpu.self must be at offset 8 (pcpu.h curcpu asm)");
_Static_assert(__builtin_offsetof(struct pcpu, current) == 16,
               "struct pcpu.current must be at offset 16 (sched.h get_current/set_current asm)");
_Static_assert(__builtin_offsetof(struct pcpu, kernel_rsp) == 40,
               "struct pcpu.kernel_rsp must be at offset 40 (syscall entry asm)");
_Static_assert(__builtin_offsetof(struct pcpu, user_rsp_scratch) == 48,
               "struct pcpu.user_rsp_scratch must be at offset 48 (syscall entry asm)");

extern struct pcpu g_pcpu[MAXCPU];

/**
 * Return this CPU's per-CPU block (reads the self-pointer at %gs:8).  Valid only
 * after x86_64_pcpu_install() has run on the calling CPU.
 */
static inline struct pcpu *curcpu(void)
{
	struct pcpu *p;
	__asm__ __volatile__("movq %%gs:8, %0" : "=r"(p));
	return (p);
}

/**
 * Point GS_BASE at g_pcpu[id], stamp its identity (cpuid + self), and clear its
 * `current`.  Call once per CPU before any _current access (BSP: top of
 * kmain_x86_64).
 */
void x86_64_pcpu_install(u_int32_t id);

#endif /* _X86_64_PCPU_H */
