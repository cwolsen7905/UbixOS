/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-dependent forwarding header: resolves <machine/ansi.h> to the active
 * architecture's <ARCH/ansi.h>.  Generic code includes <machine/ansi.h>; the build's
 * target compiler selects the arch (__aarch64__ for the aarch64-elf toolchain,
 * else i386).
 */
#ifndef _MACHINE_ANSI_H
#define _MACHINE_ANSI_H
#if defined(__aarch64__)
#include <aarch64/ansi.h>
#elif defined(__x86_64__)
#include <x86_64/ansi.h>
#else
#include <i386/ansi.h>
#endif
#endif /* _MACHINE_ANSI_H */
