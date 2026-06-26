/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Demand-paged ELF64 loader (machine-independent).
 *
 * Where elf64_load_at() eagerly maps and copies every PT_LOAD page up front,
 * this sets the segments up for LAZY paging: it reads only the ELF headers, then
 * records a file-backed VMA (the bulk of each segment) plus a demand-zero VMA
 * (the BSS tail) in the process's vm_map.  Pages fault in one at a time through
 * vmm_demand_fault() as the program touches them — so a 100 MB binary starts
 * instantly and only the touched pages ever come off disk.
 *
 * Static ET_EXEC only; a dynamic image (ET_DYN / PT_INTERP) returns 1 so the
 * caller falls back to the eager loader (dynamic executables are small).  MI:
 * hardware is reached only through the md_* hooks, the MI VMA tree, and the VFS.
 */

#include <sys/types.h>
#include <sys/elf_load.h>
#include <sys/elf64.h>
#include <sys/elf_common.h>
#include <ubixos/sched.h> /* _current (page-alloc charge for the boundary page) */
#include <vmm/vmm.h>      /* vmm_find_free_page */
#include <vmm/vm_map.h>   /* vm_map_insert_file, vm_map_insert, VM_* */
#include <vmm/paging.h>   /* PAGE_SIZE */
#include <machine/elf.h>  /* ELF_TARG_MACH */
#include <fs/vfs/file.h>  /* fopen, fclose, vfs_pread_locked */
#include <lib/kprintf.h>
#include <string.h>

#define PAGE_DOWN(x) ((x) & ~((u_int64_t)PAGE_SIZE - 1))
#define PAGE_UP(x) (((x) + PAGE_SIZE - 1) & ~((u_int64_t)PAGE_SIZE - 1))

/**
 * Translate ELF p_flags (PF_R/W/X) to a VMA's VM_PROT_* protection.
 */
static u_int32_t prot_of(u_int32_t pflags)
{
	u_int32_t prot = 0;

	if (pflags & PF_R)
		prot |= VM_PROT_READ;
	if (pflags & PF_W)
		prot |= VM_PROT_WRITE;
	if (pflags & PF_X)
		prot |= VM_PROT_EXEC;
	return (prot);
}

int elf64_load_demand(const char *path, u_int64_t *aspace_root, struct vm_map *map, elf64_load_info_t *info)
{
	fileDescriptor_t *hfd;
	Elf64_Ehdr eh;
	Elf64_Phdr ph;
	unsigned i;

	hfd = fopen(path, "r");
	if (hfd == NULL)
		return (-1);

	if (vfs_pread_locked(hfd, &eh, 0, sizeof(eh)) != sizeof(eh))
	{
		fclose(hfd);
		return (-1);
	}

	/* Native ELF64? */
	if (eh.e_ident[EI_MAG0] != ELFMAG0 || eh.e_ident[EI_MAG1] != ELFMAG1 || eh.e_ident[EI_MAG2] != ELFMAG2 ||
	    eh.e_ident[EI_MAG3] != ELFMAG3 || eh.e_ident[EI_CLASS] != ELFCLASS64 || eh.e_machine != ELF_TARG_MACH)
	{
		fclose(hfd);
		return (-1);
	}
	if (eh.e_type != ET_EXEC)
	{
		fclose(hfd);
		return (1); /* PIE / ET_DYN — caller uses the eager dynamic path */
	}

	/* A PT_INTERP makes this dynamic — hand back to the eager loader. */
	for (i = 0; i < eh.e_phnum; i++)
	{
		if (vfs_pread_locked(hfd, &ph, (off_t)(eh.e_phoff + (u_int64_t)i * eh.e_phentsize), sizeof(ph)) !=
		    sizeof(ph))
		{
			fclose(hfd);
			return (-1);
		}
		if (ph.p_type == PT_INTERP)
		{
			fclose(hfd);
			return (1);
		}
	}

	info->entry = eh.e_entry;
	info->phdr_va = 0;
	info->phnum = eh.e_phnum;
	info->phentsize = eh.e_phentsize;
	info->interp_off = 0;
	info->interp_sz = 0;
	info->is_dyn = 0;

	/* Tracks the page-aligned end of the previous PT_LOAD: if a segment's first page
	 * overlaps the previous segment's last page (two segments packed into one page)
	 * the simple per-segment VMA scheme can't represent it — fall back to the eager
	 * loader.  Normally-linked binaries (clang included) gap-pad segments onto
	 * separate pages, so this guard never trips for them. */
	u_int64_t prev_page_end = 0;

	for (i = 0; i < eh.e_phnum; i++)
	{
		u_int64_t seg_vaddr, file_end, va_page, off_page, file_pages_end, bss_start, bss_end;
		u_int32_t prot;

		if (vfs_pread_locked(hfd, &ph, (off_t)(eh.e_phoff + (u_int64_t)i * eh.e_phentsize), sizeof(ph)) !=
		    sizeof(ph))
		{
			fclose(hfd);
			return (-1);
		}
		if (ph.p_type != PT_LOAD || ph.p_memsz == 0)
			continue;

		seg_vaddr = ph.p_vaddr; /* ET_EXEC: link address (load base 0) */
		prot = prot_of(ph.p_flags);

		/* AT_PHDR = in-memory VA of the program headers (in the segment covering e_phoff). */
		if (info->phdr_va == 0 && eh.e_phoff >= ph.p_offset && eh.e_phoff < ph.p_offset + ph.p_filesz)
			info->phdr_va = seg_vaddr + (eh.e_phoff - ph.p_offset);

		/* p_offset is congruent to p_vaddr (mod page) for a loadable segment, so the
		 * page-aligned file offset corresponds to the page-aligned VA.  The leading
		 * sub-page slice (va_page .. seg_vaddr) is inter-segment padding — it reads
		 * harmless bytes and the program never touches it. */
		va_page = PAGE_DOWN(seg_vaddr);
		off_page = PAGE_DOWN(ph.p_offset);
		file_end = seg_vaddr + ph.p_filesz;
		file_pages_end = PAGE_DOWN(file_end);

		if (va_page < prev_page_end)
		{
			kprintf("elf64_demand: %s segments share a page; eager fallback\n", path);
			fclose(hfd);
			return (1);
		}
		prev_page_end = PAGE_UP(seg_vaddr + ph.p_memsz);

		/* (a) Whole file-backed pages → a VMA that demand-reads full pages.  Owns its
		 *     own fd so vm_map teardown (fclose at vm_map.c:144) releases it. */
		if (file_pages_end > va_page)
		{
			fileDescriptor_t *vfd = fopen(path, "r");

			if (vfd == NULL)
			{
				fclose(hfd);
				return (-1);
			}
			if (vm_map_insert_file(map, va_page, file_pages_end, prot, 0, vfd, (off_t)off_page) != 0)
			{
				fclose(vfd);
				fclose(hfd);
				return (-1);
			}
		}

		/* (b) Boundary page (partial file data + start of BSS): one eager page, so the
		 *     resolver only ever reads whole pages. */
		if (file_end > file_pages_end)
		{
			uintptr_t frame = vmm_find_free_page(_current->id);
			void *kframe;
			u_int64_t boff = off_page + (file_pages_end - va_page);
			size_t nbytes = (size_t)(file_end - file_pages_end);

			if (frame == 0)
			{
				fclose(hfd);
				return (-1);
			}
			kframe = md_phys_to_virt(frame);
			memset(kframe, 0, PAGE_SIZE);
			vfs_pread_locked(hfd, kframe, (off_t)boff, nbytes);
			md_map_user_page(aspace_root, file_pages_end, (u_int64_t)frame, (prot & VM_PROT_EXEC) != 0);
			if (prot & VM_PROT_EXEC)
				md_sync_icache((uintptr_t)frame, PAGE_SIZE);
		}

		/* (c) BSS pages past the boundary page → demand-zero anonymous VMA. */
		bss_start = PAGE_UP(file_end);
		bss_end = PAGE_UP(seg_vaddr + ph.p_memsz);
		if (bss_end > bss_start)
		{
			if (vm_map_insert(map, bss_start, bss_end, prot, VM_MAP_ANON) != 0)
			{
				fclose(hfd);
				return (-1);
			}
		}
	}

	fclose(hfd);
	return (0);
}
