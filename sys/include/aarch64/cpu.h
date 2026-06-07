/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 CPU helpers (stub — barriers now; system-register accessors as the
 * generic kernel ports).  Reached via <machine/cpu.h>.
 */
#ifndef _AARCH64_CPU_H
#define _AARCH64_CPU_H

static inline void cpu_dsb(void) { __asm__ volatile("dsb sy" ::: "memory"); }
static inline void cpu_isb(void) { __asm__ volatile("isb" ::: "memory"); }

#endif /* _AARCH64_CPU_H */
