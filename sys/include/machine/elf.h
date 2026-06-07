/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-dependent forwarding header: resolves <machine/elf.h> to the active
 * architecture's <ARCH/elf.h>.  Generic code includes <machine/elf.h>; the build's
 * target compiler selects the arch (__aarch64__ for the aarch64-elf toolchain,
 * else i386).
 */
#ifndef _MACHINE_ELF_H
#define _MACHINE_ELF_H
#if defined(__aarch64__)
#include <aarch64/elf.h>
#else
#include <i386/elf.h>
#endif
#endif /* _MACHINE_ELF_H */
