/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 virtual address-space layout (stub).  TTBR0_EL1 = low half (user),
 * TTBR1_EL1 = high half (kernel).  Placeholders refined with the aarch64 vmm
 * port; sibling of i386/vmm_layout.h.
 */
#ifndef _AARCH64_VMM_LAYOUT_H
#define _AARCH64_VMM_LAYOUT_H

#define VMM_USER_START 0x0000000000001000UL
#define VMM_USER_END   0x0000007FFFFFEFFFUL
#define STACK_ADDR     0x0000007FFFFFF000UL
#define VMM_KERN_START 0xFFFFFF8000000000UL
#define VMM_KERN_END   0xFFFFFFFFFFFFFFFFUL

#endif /* _AARCH64_VMM_LAYOUT_H */
