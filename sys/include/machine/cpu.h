/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-dependent forwarding header: resolves <machine/cpu.h> to the active
 * architecture's <ARCH/cpu.h>.  Generic code includes <machine/cpu.h>; the build's
 * target compiler selects the arch (__aarch64__ for the aarch64-elf toolchain,
 * else i386).
 */
#ifndef _MACHINE_CPU_H
#define _MACHINE_CPU_H
#if defined(__aarch64__)
#include <aarch64/cpu.h>
#else
#include <i386/cpu.h>
#endif
#endif /* _MACHINE_CPU_H */
