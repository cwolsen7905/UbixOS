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
 * Where an ELF image's bytes come from: an in-memory copy, or (for large
 * executables) the file itself, read on demand so the kernel never needs a
 * buffer the size of the program.  @read copies @len bytes at file offset @off
 * into @dst and returns 0, or -1 on a short read.
 */
typedef struct elf64_src
{
	int (*read)(struct elf64_src *src, u_int64_t off, void *dst, u_int64_t len);
	const void *mem; /* memory-backed source: the image */
	u_int64_t size;  /* image/file size in bytes */
	void *ctx;       /* file-backed source: the caller's file handle */
} elf64_src_t;

/**
 * Initialise @src to read from the in-memory @image of @size bytes.
 */
void elf64_src_mem(elf64_src_t *src, const void *image, u_int64_t size);

/**
 * elf64_load_at() for any source: reads the headers, then each PT_LOAD page's
 * bytes straight into its frame.
 *
 * @return 0 on success; -1 on an invalid image, a read error or mapping failure.
 */
int elf64_load_src(elf64_src_t *src, u_int64_t *aspace_root, u_int64_t load_base, elf64_load_info_t *info);

/* ---- machine-dependent hooks (implemented per 64-bit arch) ---- */

/**
 * Map one 4 KB user page @va -> @pa into @aspace_root.
 * @param executable non-zero for code (EL0/ring-3 executable), zero for data.
 */
void md_map_user_page(u_int64_t *aspace_root, u_int64_t va, u_int64_t pa, int executable);

/**
 * Reserve user page @va in @aspace_root as demand-zero (backed on first touch),
 * replacing any existing mapping there.
 * @param executable non-zero for an EL0/ring-3 executable page.
 */
void md_reserve_user_page(u_int64_t *aspace_root, u_int64_t va, int executable);

/**
 * Remove the user mappings in [@va, @va+@len) and release their frames.
 */
void md_unmap_user_range(u_int64_t *aspace_root, u_int64_t va, u_int64_t len);

/**
 * Synchronize the I-cache for [@addr, @addr+@len) after writing code there.
 * No-op on architectures with coherent instruction caches.
 */
void md_sync_icache(uintptr_t addr, u_int64_t len);

#endif /* _SYS_ELF_LOAD_H */
