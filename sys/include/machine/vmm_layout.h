/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-dependent forwarding header: resolves <machine/vmm_layout.h> to the active
 * architecture's <ARCH/vmm_layout.h>.  Generic code includes <machine/vmm_layout.h>; the build's
 * target compiler selects the arch (__aarch64__ for the aarch64-elf toolchain,
 * else i386).
 */
#ifndef _MACHINE_VMM_LAYOUT_H
#define _MACHINE_VMM_LAYOUT_H
#if defined(__aarch64__)
#include <aarch64/vmm_layout.h>
#else
#include <i386/vmm_layout.h>
#endif
#endif /* _MACHINE_VMM_LAYOUT_H */
