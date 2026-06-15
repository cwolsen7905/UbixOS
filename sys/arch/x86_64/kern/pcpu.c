/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 per-CPU state (smp-plan / x86_64 M0).  Holds the g_pcpu[] array and
 * installs the GS segment base so `_current` (and curcpu()) become per-CPU,
 * replacing the single global used during early bring-up.  Sibling of i386's
 * %gs/GDT pcpu code and aarch64's TPIDR_EL1 install.
 */

#include "../x86_64.h"
#include <x86_64/pcpu.h>

#define MSR_GS_BASE 0xC0000101 /* IA32_GS_BASE — base for %gs: accesses in long mode */

struct pcpu g_pcpu[MAXCPU];

/**
 * Point GS_BASE at g_pcpu[id], stamp its identity, and clear its current task.
 * After this, [%gs:8] is &g_pcpu[id] (curcpu) and [%gs:16] is its current thread
 * (sched.h get_current/set_current).
 */
void x86_64_pcpu_install(u32 id)
{
	struct pcpu *p = &g_pcpu[id];
	u64 base;
	u32 lo, hi;

	p->cpuid = id;
	p->self = p;
	p->current = 0;

	base = (u64)(unsigned long)p;
	lo = (u32)base;
	hi = (u32)(base >> 32);
	__asm__ __volatile__("wrmsr" : : "c"(MSR_GS_BASE), "a"(lo), "d"(hi));
}
