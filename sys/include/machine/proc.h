/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-dependent forwarding header: resolves <machine/proc.h> to the active
 * architecture's <ARCH/proc.h>.  Generic code includes <machine/proc.h>; the build's
 * target compiler selects the arch (__aarch64__ for the aarch64-elf toolchain,
 * else i386).
 */
#ifndef _MACHINE_PROC_H
#define _MACHINE_PROC_H
#if defined(__aarch64__)
#include <aarch64/proc.h>
#else
#include <i386/proc.h>
#endif
#endif /* _MACHINE_PROC_H */
