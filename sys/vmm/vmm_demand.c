/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Machine-independent demand-fault resolver.
 *
 * The first lazy-paging path shared by the 64-bit kernels.  When a user access
 * (or the kernel acting on a user's behalf mid-syscall) takes a not-present
 * fault, the arch fault handler (aarch64 exceptions.c / x86_64 idt.c) calls
 * vmm_demand_fault().  If a VMA in the faulting process's vm_map covers the
 * address we materialise exactly that one page and the access is retried;
 * otherwise it is a genuine SIGSEGV.
 *
 * Two backings, both producing a PRIVATE page for now (cross-process sharing of
 * read-only file pages through the file-page cache is a later optimisation —
 * see docs/design/demand-paged-exec-plan.md):
 *   - VM_MAP_FILE: read the faulting page from the VMA's backing fd.
 *   - VM_MAP_ANON: demand-zero.
 *
 * Reaches the hardware only through the md_* hooks (<sys/elf_load.h>) and the MI
 * VMA tree, so it is identical on aarch64 and x86_64.  The address-space root is
 * supplied by the (arch-specific) caller, which knows where it lives.
 */

#include <sys/types.h>
#include <ubixos/sched.h>  /* _current */
#include <vmm/vmm.h>       /* vmm_find_free_page, free_page */
#include <vmm/vm_map.h>    /* vm_map_lookup, VM_MAP_*, vm_map_entry_t */
#include <vmm/paging.h>    /* PAGE_SIZE */
#include <sys/elf_load.h>  /* md_phys_to_virt, md_map_user_page */
#include <fs/vfs/file.h>   /* fileDescriptor_t, vfs_pread_locked */
#include <string.h>        /* memset */

/**
 * Resolve a not-present fault at user virtual address @far for the current
 * process by materialising the single page that covers it.
 *
 * @param aspace_root  the faulting process's address-space root, passed by the
 *                     arch fault handler (aarch64 L1 / x86_64 PML4).
 * @return 0 if a page was mapped (caller retries the faulting access); -1 if no
 *         VMA covers @far (the caller delivers SIGSEGV).
 */
int vmm_demand_fault(u_int64_t *aspace_root, uintptr_t far)
{
	vm_map_entry_t *vma;
	u_int64_t pg = (u_int64_t)far & ~((u_int64_t)PAGE_SIZE - 1);
	uintptr_t frame;
	void *kframe;
	int exec;

	if (_current == NULL)
		return (-1);

	vma = vm_map_lookup(&_current->vm_map, (uintptr_t)pg);
	if (vma == NULL)
		return (-1); /* not a managed region — a genuine fault */

	frame = vmm_find_free_page(_current->id);
	if (frame == 0)
		return (-1);
	kframe = md_phys_to_virt(frame);
	memset(kframe, 0, PAGE_SIZE); /* covers anon demand-zero + any unread tail */

	if (vma->vm_flags & VM_MAP_FILE)
	{
		fileDescriptor_t *bfd = (fileDescriptor_t *)vma->vm_vnode;
		off_t foff;

		if (bfd == NULL)
		{
			free_page(frame);
			return (-1);
		}
		foff = vma->vm_offset + (off_t)(pg - vma->vm_start);
		/* Positional, vfs_io_lock-serialised read — same lock fread() uses — so a
		 * demand-paged read can't race a concurrent FS read on another CPU. */
		vfs_pread_locked(bfd, kframe, foff, PAGE_SIZE);
	}
	/* else VM_MAP_ANON: the zeroed frame above is the demand-zero result. */

	exec = (vma->vm_prot & VM_PROT_EXEC) != 0;
	md_map_user_page(aspace_root, pg, (u_int64_t)frame, exec);
	return (0);
}
