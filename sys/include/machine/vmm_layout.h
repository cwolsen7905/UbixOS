/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-dependent forwarding header: resolves <machine/vmm_layout.h> to the active
 * architecture's <ARCH/vmm_layout.h>.  Generic code includes <machine/vmm_layout.h>; the build's
 * target compiler selects the arch (__aarch64__, __x86_64__, else the frozen
 * i386 layout).  x86_64 used to fall through to i386, so 64-bit code saw
 * 32-bit user bounds (VMM_USER_END 0xBFFFFFFF).
 */
#ifndef _MACHINE_VMM_LAYOUT_H
#define _MACHINE_VMM_LAYOUT_H
#if defined(__aarch64__)
#include <aarch64/vmm_layout.h>
#elif defined(__x86_64__)
#include <x86_64/vmm_layout.h>
#else
#include <i386/vmm_layout.h>
#endif
#endif /* _MACHINE_VMM_LAYOUT_H */
