/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-dependent forwarding header: resolves <machine/limits.h> to the active
 * architecture's <ARCH/limits.h>.  Generic code includes <machine/limits.h>; the build's
 * target compiler selects the arch (__aarch64__ for the aarch64-elf toolchain,
 * else i386).
 */
#ifndef _MACHINE_LIMITS_H
#define _MACHINE_LIMITS_H
#if defined(__aarch64__)
#include <aarch64/limits.h>
#elif defined(__x86_64__)
#include <x86_64/limits.h>
#else
#include <i386/limits.h>
#endif
#endif /* _MACHINE_LIMITS_H */
