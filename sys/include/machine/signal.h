/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-dependent forwarding header: resolves <machine/signal.h> to the active
 * architecture's <ARCH/signal.h>.  Generic code includes <machine/signal.h>; the build's
 * target compiler selects the arch (__aarch64__ for the aarch64-elf toolchain,
 * else i386).
 */
#ifndef _MACHINE_SIGNAL_H
#define _MACHINE_SIGNAL_H
#if defined(__aarch64__)
#include <aarch64/signal.h>
#elif defined(__x86_64__)
#include <x86_64/signal.h>
#else
#include <i386/signal.h>
#endif
#endif /* _MACHINE_SIGNAL_H */
