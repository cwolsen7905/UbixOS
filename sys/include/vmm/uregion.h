/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Architecture-neutral user-space region policy for LP64 kernels
 * (sys/vmm/vmm_uregion.c).  The page-counting / bump-allocation / heap-growth
 * logic is identical across 64-bit architectures; the only machine-dependent
 * pieces are the page-table root type and mapping a page, supplied via the
 * md_map_user_page() hook (see <sys/elf_load.h>).  The VA-layout constants
 * (where the mmap region and brk heap live) are the caller's — i.e. the arch's
 * — choice and are passed in as the initial cursor values.
 *
 * Sibling of the generic ELF64 loader (sys/kern/elf64_load.c): it is built into
 * each 64-bit arch's source list, not the 32-bit i386 kernel (which has its own
 * VMM rooted in recursive paging).
 */
#ifndef _VMM_UREGION_H
#define _VMM_UREGION_H

#include <sys/types.h>

/**
 * Map an anonymous, zero-filled, read-write region into the address space
 * @aspace_root (the LP64 page-table root — aarch64 L1, x86_64 PML4).
 *
 * Backs @len (rounded up to whole pages) with freshly allocated frames.  When
 * @fixed is non-zero the region is placed at @fixed_addr (page-aligned down) and
 * @next is left unchanged; otherwise it is bump-allocated at *@next, which is
 * advanced past the new region.  Models the anonymous-mmap surface mallocng
 * needs (MAP_ANON private; MAP_FIXED honoured for its reserve/guard pages).
 *
 * @param next  in/out bump cursor for non-fixed mappings; ignored when @fixed.
 * @return the region base VA, or 0 on allocation failure.
 */
uintptr_t vmm_uregion_mmap_anon(u_int64_t *aspace_root, uintptr_t *next, size_t len, int fixed, uintptr_t fixed_addr);

/**
 * Grow the program break in @aspace_root from *@cur up to @newbrk, mapping any
 * newly required pages (zero-filled, read-write).  Never shrinks; a request at
 * or below the current break just reports it (Linux brk return semantics: the
 * resulting break is returned, which musl's mallocng glue treats as success).
 *
 * @param cur  in/out current break; must already be initialized to the heap base.
 * @return the resulting program break (== @newbrk on success, == *@cur if a
 *         growth allocation failed).
 */
uintptr_t vmm_uregion_brk(u_int64_t *aspace_root, uintptr_t *cur, uintptr_t newbrk);

#endif /* _VMM_UREGION_H */
