/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 per-process user virtual-address layout.  The low 1 GB
 * (0x0–0x3FFFFFFF) is the kernel's identity map (PDPT[0], shared into every user
 * PML4), so every user region lives at or above 0x40000000.  The DYN_,
 * USER_STACK_, BRK_, and MMAP region bases below are the single source of truth
 * for the exec loader (kern/execfile.c), mmap, and brk (kern/syscall_md.c).
 * Sibling of aarch64/vmm_layout.h — the dynamic regions use the same 1 GB-aligned
 * spacing so both arches load PIE binaries identically.
 */
#ifndef _X86_64_VMM_LAYOUT_H
#define _X86_64_VMM_LAYOUT_H

/*
 * Per-process user-VA regions, each a 1 GB-aligned block clear of the others:
 *
 *   0x040000000  EXEC_BASE        static ET_EXEC load base + single-page stack
 *   0x100000000  DYN_MAIN_BASE    PIE main executable load base
 *   0x140000000  DYN_INTERP_BASE  dynamic linker (ld-musl) load base
 *   0x180000000  DYN_STACK_VA     initial user stack (argv/env/auxv in top page)
 *   0x1C0000000  BRK_BASE         program break (brk/sbrk)
 *   0x200000000  MMAP_BASE        anonymous mmap bump allocator
 *
 * PAGE_SIZE must be in scope (via <vmm/vmm.h>) wherever the *_TOP macros expand.
 */
#define DYN_MAIN_BASE 0x100000000UL   /* PIE main exe load base */
#define DYN_INTERP_BASE 0x140000000UL /* dynamic linker load base */

/* Initial user stack for a dynamically-linked program (argv/env/auxv in the top
 * page).  64 pages = 256 KB — the dynamic linker + a real shell need more than
 * one page. */
#define DYN_STACK_VA 0x180000000UL
#define DYN_STACK_PAGES 64
#define DYN_STACK_TOP (DYN_STACK_VA + (u_int64_t)DYN_STACK_PAGES * PAGE_SIZE)

/* brk and anonymous mmap regions (clear of the load/stack blocks above). */
#define BRK_BASE 0x1C0000000UL
#define MMAP_BASE 0x200000000UL

#endif /* _X86_64_VMM_LAYOUT_H */
