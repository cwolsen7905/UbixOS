/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Architecture-neutral ELF64 program loader (sys/kern/elf64_load.c).
 *
 * The ELF64 container format is identical across LP64 architectures, so the
 * parse + segment-mapping logic is generic; only three things are
 * machine-dependent and supplied as hooks below: the target machine constant
 * (ELF_TARG_MACH via <machine/elf.h>), mapping a user page into an address
 * space, and instruction-cache synchronization of freshly-written code.  Built
 * for 64-bit targets only (it is in each 64-bit arch's generic source list, not
 * the i386 kernel, which loads ELF32).
 */
#ifndef _SYS_ELF_LOAD_H
#define _SYS_ELF_LOAD_H

#include <sys/types.h>

/**
 * Load a static ET_EXEC ELF64 image into the address space rooted at
 * @aspace_root (the top-level page-table root — aarch64 L1, x86_64 PML4).
 *
 * @param entry_out  set to the program entry point on success.
 * @return 0 on success; -1 on an invalid image or a mapping failure.
 */
int elf64_load(const void *image, u_int64_t *aspace_root, u_int64_t *entry_out);

/* ---- machine-dependent hooks (implemented per 64-bit arch) ---- */

/**
 * Map one 4 KB user page @va -> @pa into @aspace_root.
 * @param executable non-zero for code (EL0/ring-3 executable), zero for data.
 */
void md_map_user_page(u_int64_t *aspace_root, u_int64_t va, u_int64_t pa, int executable);

/**
 * Synchronize the I-cache for [@addr, @addr+@len) after writing code there.
 * No-op on architectures with coherent instruction caches.
 */
void md_sync_icache(uintptr_t addr, u_int64_t len);

#endif /* _SYS_ELF_LOAD_H */
