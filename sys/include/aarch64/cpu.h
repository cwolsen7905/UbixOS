/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 CPU helpers (stub — barriers now; system-register accessors as the
 * generic kernel ports).  Reached via <machine/cpu.h>.
 */
#ifndef _AARCH64_CPU_H
#define _AARCH64_CPU_H

#include <sys/types.h>

static inline void cpu_dsb(void)
{
	__asm__ volatile("dsb sy" ::: "memory");
}
static inline void cpu_isb(void)
{
	__asm__ volatile("isb" ::: "memory");
}

/**
 * Free-running cycle/tick counter for timing + entropy jitter (i386: the TSC).
 * aarch64: the virtual count register CNTVCT_EL0.
 */
static inline u_int64_t machine_cycles(void)
{
	u_int64_t v;
	__asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
	return v;
}

#endif /* _AARCH64_CPU_H */
