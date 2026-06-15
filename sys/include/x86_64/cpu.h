/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 CPU helpers.  Reached via <machine/cpu.h>.  Sibling of i386/cpu.h and
 * aarch64/cpu.h.
 */
#ifndef _X86_64_CPU_H
#define _X86_64_CPU_H

#include <sys/types.h>

/* Full memory barrier (i386: a locked op; aarch64: dsb). */
static inline void cpu_dsb(void)
{
	__asm__ volatile("mfence" ::: "memory");
}

/* Instruction-stream serialize. */
static inline void cpu_isb(void)
{
	__asm__ volatile("lfence" ::: "memory");
}

/**
 * Free-running cycle counter for timing + entropy jitter.  x86-64: the TSC.
 */
static inline u_int64_t machine_cycles(void)
{
	u_int32_t lo, hi;
	__asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return ((u_int64_t)hi << 32) | lo;
}

#endif /* _X86_64_CPU_H */
