/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Architecture-neutral ELF64 program loader.
 *
 * Parses a static ET_EXEC ELF64 image and maps its PT_LOAD segments into a
 * caller-supplied address space, copying file contents and zero-filling the BSS
 * tail (p_memsz > p_filesz).  The container format is identical on every LP64
 * architecture; the three machine-dependent operations — the target machine
 * check (ELF_TARG_MACH), user-page mapping, and I-cache sync — are hooks
 * (<sys/elf_load.h>, <machine/elf.h>).  Source-agnostic: it consumes an
 * in-memory image, so an embedded binary now and file-backed execve later use
 * the same code.
 *
 * Frames come from the physical allocator and are assumed identity/direct-mapped
 * in the kernel (true for the 64-bit kernels that use this), so a frame's
 * physical address doubles as the kernel pointer used to populate it.
 */

#include <sys/types.h>
#include <sys/elf_load.h>
#include <sys/elf64.h>
#include <sys/elf_common.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>  /* PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <lib/kprintf.h>
#include <machine/elf.h> /* ELF_TARG_MACH */
#include <string.h>

#define PAGE_DOWN(x) ((x) & ~((u_int64_t)PAGE_SIZE - 1))
#define PAGE_UP(x) (((x) + PAGE_SIZE - 1) & ~((u_int64_t)PAGE_SIZE - 1))

/**
 * Map and populate one PT_LOAD segment @ph from @image into @aspace_root, with
 * every VA shifted by @base (0 for an ET_EXEC at its linked address; the chosen
 * load base for an ET_DYN / PIE).
 *
 * @return 0 on success, -1 on allocation failure.
 */
static int load_segment(const u_int8_t *image, const Elf64_Phdr *ph, u_int64_t *aspace_root, u_int64_t base)
{
	u_int64_t seg_vaddr = base + ph->p_vaddr;
	u_int64_t va_start = PAGE_DOWN(seg_vaddr);
	u_int64_t va_end = PAGE_UP(seg_vaddr + ph->p_memsz);
	int exec = (ph->p_flags & PF_X) != 0;

	for (u_int64_t va = va_start; va < va_end; va += PAGE_SIZE)
	{
		uintptr_t frame = vmm_find_free_page(sysID);
		if (frame == 0)
			return -1;
		memset((void *)frame, 0, PAGE_SIZE); /* zero — covers BSS + partial pages */

		/* Copy the slice of this page overlapping [seg_vaddr, seg_vaddr+p_filesz). */
		u_int64_t seg_file_end = seg_vaddr + ph->p_filesz;
		u_int64_t copy_lo = (va > seg_vaddr) ? va : seg_vaddr;
		u_int64_t copy_hi = (va + PAGE_SIZE < seg_file_end) ? (va + PAGE_SIZE) : seg_file_end;

		if (copy_hi > copy_lo)
		{
			u_int64_t file_off = ph->p_offset + (copy_lo - seg_vaddr);
			memcpy((void *)(frame + (copy_lo - va)), image + file_off, (size_t)(copy_hi - copy_lo));
		}

		if (exec)
			md_sync_icache(frame, PAGE_SIZE);
		md_map_user_page(aspace_root, va, (u_int64_t)frame, exec);
	}
	return 0;
}

/**
 * Load an ELF64 image (ET_EXEC or ET_DYN) at @load_base into @aspace_root,
 * reporting the auxv-relevant info + PT_INTERP location in @info.
 */
int elf64_load_at(const void *image, u_int64_t *aspace_root, u_int64_t load_base, elf64_load_info_t *info)
{
	const u_int8_t *base = (const u_int8_t *)image;
	const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;

	if (eh->e_ident[EI_MAG0] != ELFMAG0 || eh->e_ident[EI_MAG1] != ELFMAG1 || eh->e_ident[EI_MAG2] != ELFMAG2 ||
	    eh->e_ident[EI_MAG3] != ELFMAG3)
	{
		kprintf("elf64: bad magic\n");
		return -1;
	}
	if (eh->e_ident[EI_CLASS] != ELFCLASS64 || eh->e_machine != ELF_TARG_MACH ||
	    (eh->e_type != ET_EXEC && eh->e_type != ET_DYN))
	{
		kprintf("elf64: not a native ELF64 exec/dyn (class=%u mach=%u type=%u)\n",
		        eh->e_ident[EI_CLASS],
		        eh->e_machine,
		        eh->e_type);
		return -1;
	}

	if (info != 0)
	{
		info->entry = load_base + eh->e_entry;
		info->phdr_va = 0; /* resolved from the PT_LOAD that contains the headers */
		info->phnum = eh->e_phnum;
		info->phentsize = eh->e_phentsize;
		info->interp_off = 0;
		info->interp_sz = 0;
		info->is_dyn = (eh->e_type == ET_DYN);
	}

	for (unsigned i = 0; i < eh->e_phnum; i++)
	{
		const Elf64_Phdr *ph = (const Elf64_Phdr *)(base + eh->e_phoff + (u_int64_t)i * eh->e_phentsize);
		if (ph->p_type == PT_INTERP && info != 0)
		{
			info->interp_off = ph->p_offset;
			info->interp_sz = ph->p_filesz;
			continue;
		}
		if (ph->p_type != PT_LOAD || ph->p_memsz == 0)
			continue;
		/* AT_PHDR = the in-memory VA of the program headers.  They live at file
		 * offset e_phoff inside the PT_LOAD that covers it; the in-memory VA is
		 * that segment's load VA + (e_phoff - p_offset).  Computing it from the
		 * segment (not load_base + e_phoff) is correct for ET_EXEC too, whose
		 * first PT_LOAD vaddr is the link address, not 0. */
		if (info != 0 && info->phdr_va == 0 && eh->e_phoff >= ph->p_offset &&
		    eh->e_phoff < ph->p_offset + ph->p_filesz)
			info->phdr_va = load_base + ph->p_vaddr + (eh->e_phoff - ph->p_offset);
		if (load_segment(base, ph, aspace_root, load_base) != 0)
			return -1;
	}

	return 0;
}

/**
 * Load a static ELF64 executable @image into the address space @aspace_root.
 */
int elf64_load(const void *image, u_int64_t *aspace_root, u_int64_t *entry_out)
{
	const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;
	elf64_load_info_t info;

	if (eh->e_type != ET_EXEC)
	{
		kprintf("elf64: elf64_load() requires ET_EXEC (type=%u); use elf64_load_at\n", eh->e_type);
		return -1;
	}
	if (elf64_load_at(image, aspace_root, 0, &info) != 0)
		return -1;

	*entry_out = info.entry;
	return 0;
}
