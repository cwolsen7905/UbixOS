/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 per-CPU block (smp-plan Phase 3 / M0).  Reached via <aarch64/pcpu.h>.
 */
#ifndef _AARCH64_PCPU_H
#define _AARCH64_PCPU_H

#include <sys/types.h>

#define MAXCPU 8

struct taskStruct; /* == kTask_t; full type in <ubixos/sched.h> */

/*
 * One per core, reached through the banked system register TPIDR_EL1.  The
 * kernel owns TPIDR_EL1 at EL1 and it persists across EL0<->EL1 transitions, so
 * it is set ONCE per CPU (the BSP at the top of kmain_aarch64; each AP at its
 * entry) and stays valid in every EL1 context — no per-entry reload, unlike
 * i386's %gs.
 *
 * `current` MUST stay at offset 16: <ubixos/sched.h>'s get_current()/set_current()
 * load/store [TPIDR_EL1 + 16] with a bare mrs+ldr (no function call), the aarch64
 * analog of i386's %gs:8.  The _Static_assert below guards the layout.
 */
struct pcpu
{
	u_int32_t cpuid; /* logical CPU index (0 = boot CPU) */
	u_int32_t _pad;
	u_int64_t mpidr;              /* MPIDR_EL1 affinity (hardware id) */
	struct taskStruct *current;   /* thread running on this CPU (offset 16) */
	struct taskStruct *idle;      /* this CPU's idle thread (pinned, never enqueued) */
	volatile u_int32_t heartbeat; /* liveness counter an AP bumps before it schedules */
};

_Static_assert(__builtin_offsetof(struct pcpu, current) == 16,
               "struct pcpu.current must be at offset 16 (sched.h get_current/set_current asm)");

extern struct pcpu g_pcpu[MAXCPU];

/**
 * Return this CPU's per-CPU block (reads TPIDR_EL1).  Valid only after
 * aarch64_pcpu_install() has run on the calling CPU.
 */
static inline struct pcpu *curcpu(void)
{
	struct pcpu *p;
	__asm__ __volatile__("mrs %0, tpidr_el1" : "=r"(p));
	return (p);
}

/**
 * Point TPIDR_EL1 at g_pcpu[id], stamp its identity, and clear its `current`.
 * Call once per CPU before any _current access (BSP: top of kmain_aarch64).
 */
void aarch64_pcpu_install(u_int32_t id, u_int64_t mpidr);

#endif /* _AARCH64_PCPU_H */
