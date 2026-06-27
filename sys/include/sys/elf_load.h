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

struct vm_map; /* sys/include/vmm/vm_map.h — opaque here (demand loader's target tree) */

/**
 * Load a static ET_EXEC ELF64 image into the address space rooted at
 * @aspace_root (the top-level page-table root — aarch64 L1, x86_64 PML4).
 *
 * @param entry_out  set to the program entry point on success.
 * @return 0 on success; -1 on an invalid image or a mapping failure.
 */
int elf64_load(const void *image, u_int64_t *aspace_root, u_int64_t *entry_out);

/**
 * Info reported by elf64_load_at() — what the SysV initial-stack auxv vector
 * needs (AT_PHDR/PHENT/PHNUM/BASE/ENTRY) plus the PT_INTERP path location so the
 * caller can load the dynamic linker.
 */
typedef struct elf64_load_info
{
	u_int64_t entry;      /* load_base + e_entry */
	u_int64_t phdr_va;    /* in-memory VA of the program headers (load_base + e_phoff) */
	u_int64_t phnum;      /* e_phnum */
	u_int64_t phentsize;  /* e_phentsize */
	u_int64_t interp_off; /* file offset of the PT_INTERP path string, 0 if none */
	u_int64_t interp_sz;  /* PT_INTERP p_filesz (path length incl. NUL) */
	int is_dyn;           /* non-zero for ET_DYN (PIE / shared object) */
} elf64_load_info_t;

/**
 * Load an ELF64 image's PT_LOAD segments at @load_base (added to each p_vaddr;
 * pass 0 for an ET_EXEC at its linked address).  Accepts ET_EXEC and ET_DYN, so
 * it handles both a PIE main executable and the dynamic linker.
 *
 * @param info  out: entry/phdr/phnum/phentsize + PT_INTERP location.
 * @return 0 on success; -1 on an invalid image or mapping failure.
 */
int elf64_load_at(const void *image, u_int64_t *aspace_root, u_int64_t load_base, elf64_load_info_t *info);

/**
 * Set up a static ET_EXEC's PT_LOAD segments for DEMAND PAGING into @aspace_root,
 * recording file-backed + demand-zero VMAs in @map instead of eagerly copying
 * pages.  Reads only the ELF headers (not the whole file); the segment pages
 * fault in lazily through vmm_demand_fault().  Each file-backed VMA owns its own
 * fopen(@path); one eager page is written per segment at the file/BSS boundary.
 *
 * @param info  out: entry/phdr/phnum/phentsize (same fields as elf64_load_at).
 * @return 0 if set up for demand paging; 1 if the image is dynamic (ET_DYN or
 *         has PT_INTERP) and the caller should fall back to the eager loader;
 *         -1 on an invalid image or a setup failure.
 */
int elf64_load_demand(const char *path, u_int64_t *aspace_root, struct vm_map *map, elf64_load_info_t *info);

/* ---- machine-dependent hooks (implemented per 64-bit arch) ---- */

/**
 * Map one 4 KB user page @va -> @pa into @aspace_root.
 * @param executable non-zero for code (EL0/ring-3 executable), zero for data.
 */
void md_map_user_page(u_int64_t *aspace_root, u_int64_t va, u_int64_t pa, int executable);

/**
 * @return non-zero if user VA @va already has a valid mapping in @aspace_root.
 * Used by the demand-fault clustered read-ahead to avoid remapping (and leaking)
 * a page a prior cluster already materialised, since faults are not strictly
 * sequential.
 */
int md_user_mapped(u_int64_t *aspace_root, u_int64_t va);

/**
 * Synchronize the I-cache for [@addr, @addr+@len) after writing code there.
 * No-op on architectures with coherent instruction caches.
 */
void md_sync_icache(uintptr_t addr, u_int64_t len);

/**
 * Map a physical frame to a kernel-writable virtual pointer, so the loader can
 * fill a freshly-allocated frame before it is mapped into the user space.
 * aarch64 identity-maps all RAM (pointer == phys); x86_64 reaches RAM only
 * through the physmap (P2V), since its low identity covers just the first 1 GB —
 * a frame allocated above 1 GB (after memory fills) is unreachable as a raw
 * pointer.  Using this hook (not a bare cast) is what lets a loaded image be
 * correct regardless of where its frames land.
 */
void *md_phys_to_virt(u_int64_t phys);

#endif /* _SYS_ELF_LOAD_H */
