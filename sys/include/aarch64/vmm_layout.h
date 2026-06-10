/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 virtual address-space layout.  TTBR0_EL1 = low half (user),
 * TTBR1_EL1 = high half (kernel).  The VMM_ and STACK_ADDR half-split constants
 * are placeholders (the aarch64 pmap drives the real split); the DYN_,
 * USER_STACK_, BRK_, and MMAP region bases below are live — the single source of
 * truth for the per-process user-VA layout.  Sibling of i386/vmm_layout.h.
 */
#ifndef _AARCH64_VMM_LAYOUT_H
#define _AARCH64_VMM_LAYOUT_H

#define VMM_USER_START 0x0000000000001000UL
#define VMM_USER_END 0x0000007FFFFFEFFFUL
#define STACK_ADDR 0x0000007FFFFFF000UL
#define VMM_KERN_START 0xFFFFFF8000000000UL
#define VMM_KERN_END 0xFFFFFFFFFFFFFFFFUL

/*
 * Per-process user-VA regions handed out by the exec loader, mmap, and brk —
 * the single source of truth (previously magic numbers in kern/syscall.c and
 * bringup/execfile.c).  The low 4 GB (0x0–0xFFFFFFFF) is the kernel's identity
 * map (pmap USER_L1_MIN), so every region lives at or above 0x100000000; each
 * is a 1 GB block aligned to an L1 table index (block N == VA N GB):
 *
 *   idx 4  0x100000000  DYN_MAIN_BASE    PIE main executable load base
 *   idx 5  0x140000000  DYN_INTERP_BASE  dynamic linker (ld-musl) load base
 *   idx 6  0x180000000  DYN_STACK_VA     initial user stack
 *   idx 7  0x1C0000000  BRK_BASE         program break (brk/sbrk)
 *   idx 8  0x200000000  MMAP_BASE        anonymous mmap bump allocator
 *
 * PAGE_SIZE must be in scope (via <vmm/vmm.h>) wherever the *_TOP macros are
 * expanded.
 */
#define DYN_MAIN_BASE 0x100000000UL   /* PIE main exe load base   (L1 idx 4) */
#define DYN_INTERP_BASE 0x140000000UL /* dynamic linker load base (L1 idx 5) */

/* Initial user stack for a dynamically-linked program (argv/env/auxv in the top
 * page).  64 pages = 256 KB — busybox et al. need more than one page. */
#define DYN_STACK_VA 0x180000000UL
#define DYN_STACK_PAGES 64
#define DYN_STACK_TOP (DYN_STACK_VA + (u_int64_t)DYN_STACK_PAGES * PAGE_SIZE)

/* Single-page stack for the standalone static-ELF bring-up path (just above the
 * program segments at DYN_MAIN_BASE). */
#define USER_STACK_VA 0x100200000UL
#define USER_STACK_TOP (USER_STACK_VA + PAGE_SIZE)

/* brk and anonymous mmap regions (clear of the load/stack blocks above). */
#define BRK_BASE 0x1C0000000UL
#define MMAP_BASE 0x200000000UL

#endif /* _AARCH64_VMM_LAYOUT_H */
