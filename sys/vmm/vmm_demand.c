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
 * address we materialise the page (a read-ahead cluster for file backings) and
 * the access is retried; otherwise it is a genuine SIGSEGV.
 *
 * Two backings:
 *   - VM_MAP_FILE: read a run of consecutive pages from the VMA's backing fd in
 *     one positional read (read-ahead, since binaries have high locality).
 *     READ-ONLY file pages are shared + cached through the file-page cache
 *     (vm_filecache) — one physical copy of a binary's text/rodata across every
 *     process, retained across execs so a warm re-exec faults them in with no disk
 *     I/O (this is what makes a dynamic binary like clang cold≈warm).  WRITABLE
 *     file pages (a PIE's data/GOT the dynamic linker relocates) stay private.
 *   - VM_MAP_ANON: demand-zero a single page.
 *
 * Reaches the hardware only through the md_* hooks (<sys/elf_load.h>) and the MI
 * VMA tree, so it is identical on aarch64 and x86_64.  The address-space root is
 * supplied by the (arch-specific) caller, which knows where it lives.
 */

#include <sys/types.h>
#include <ubixos/sched.h>     /* _current */
#include <vmm/vmm.h>          /* vmm_find_free_page, free_page */
#include <vmm/vm_map.h>       /* vm_map_lookup, VM_MAP_*, vm_map_entry_t */
#include <vmm/paging.h>       /* PAGE_SIZE */
#include <sys/elf_load.h>     /* md_phys_to_virt, md_map_user_page, md_map_user_page_shared */
#include <vmm/vm_filecache.h> /* shared file-page cache (read-only cross-process sharing) */
#include <fs/vfs/file.h>      /* fileDescriptor_t, vfs_pread_locked */
#include <string.h>           /* memset */

/* Pages pulled per file-backed demand fault (read-ahead cluster).  16 = 64 KB per
 * virtio-blk read — enough to amortise the per-read latency that dominates the
 * on-device toolchain load, small enough not to waste much I/O on low-locality
 * access. */
#define DEMAND_CLUSTER 16

/**
 * Resolve a not-present fault at user virtual address @far for the current
 * process by materialising the page that covers it — plus a read-ahead cluster
 * of following pages for a file-backed VMA (one read instead of one-per-fault).
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
	kTask_t *owner;
	int exec;

	if (_current == NULL)
		return (-1);

	/* The VMA tree describes the ADDRESS SPACE, but it is stored per-task.  rfork
	 * threads share one address space (TTBR0) yet each got its own (empty) vm_map,
	 * so a worker faulting a demand-paged page its leader exec'd would find no VMA
	 * and take a fatal SIGSEGV.  Resolve faults against the thread-group leader's
	 * vm_map (id == tgid); a normal process is its own leader. */
	owner = _current;
	if (_current->tgid != 0 && _current->tgid != (u_int32_t)_current->id)
	{
		kTask_t *leader = schedFindTask(_current->tgid);
		if (leader != NULL)
			owner = leader;
	}

	vma = vm_map_lookup(&owner->vm_map, (uintptr_t)pg);
	if (vma == NULL)
		return (-1); /* not a managed region — a genuine fault */

	exec = (vma->vm_prot & VM_PROT_EXEC) != 0;

	if (vma->vm_flags & VM_MAP_FILE)
	{
		fileDescriptor_t *bfd = (fileDescriptor_t *)vma->vm_vnode;
		uintptr_t base;
		void *kbase;
		off_t foff;
		u_int32_t n, i;
		int cacheable;

		if (bfd == NULL)
			return (-1);

		/* A read-only, file-identified page is shared through the file-page cache:
		 * one physical copy of a binary's text/rodata across every process, retained
		 * across execs so a warm re-exec re-maps it with no disk I/O.  Writable file
		 * pages (a PIE's data/GOT the dynamic linker relocates) stay private, so fork
		 * COW and per-process writes are correct. */
		cacheable = ((vma->vm_prot & VM_PROT_WRITE) == 0) && bfd->ino != 0;
		foff = vma->vm_offset + (off_t)(pg - vma->vm_start);

		/* Cache hit on the faulting page: map the one shared frame and return — no
		 * read, no cluster.  The warm path (re-exec of a cached binary) is entirely
		 * single-page cache hits, so it never touches the disk. */
		if (cacheable)
		{
			uintptr_t hit = (uintptr_t)vm_filecache_lookup_ref(bfd->mp, bfd->ino, foff);

			if (hit != 0)
			{
				md_map_user_page_shared(aspace_root, pg, (u_int64_t)hit, exec);
				if (exec)
					md_sync_icache((uintptr_t)md_phys_to_virt(hit), PAGE_SIZE);
				return (0);
			}
		}

		/* Miss: read-ahead cluster — a single demand fault pulls a run of up to
		 * DEMAND_CLUSTER consecutive file pages in ONE positional read, instead of
		 * one 4 KB page per fault.  Measured: clang/cc1 demand-loads the toolchain
		 * binary almost entirely through here (~93% of all faults), one page at a
		 * time, so the dominant cold cost is thousands of single-page reads — each
		 * paying the virtio-blk + cooperative-scheduler round-trip.  A cluster
		 * amortises that latency across many pages (binary code/rodata has high
		 * locality).  Bounded by the VMA end so we never read outside this mapping. */
		n = DEMAND_CLUSTER;
		if (pg + (u_int64_t)n * PAGE_SIZE > vma->vm_end)
			n = (u_int32_t)((vma->vm_end - pg) / PAGE_SIZE);
		if (n == 0)
			n = 1;

		/* Physically-contiguous frames so the whole cluster fills in one read; fall
		 * back to a single page if a contiguous run is unavailable (fragmentation). */
		base = (n > 1) ? vmm_find_free_pages_contig(n, _current->id) : 0;
		if (base == 0)
		{
			n = 1;
			base = vmm_find_free_page(_current->id);
		}
		if (base == 0)
			return (-1);

		kbase = md_phys_to_virt(base);
		memset(kbase, 0, (size_t)n * PAGE_SIZE); /* tail past EOF stays demand-zero */
		/* Positional, vfs_io_lock-serialised read — same lock fread() uses — so a
		 * demand-paged read can't race a concurrent FS read on another CPU. */
		vfs_pread_locked(bfd, kbase, foff, (size_t)n * PAGE_SIZE);
		if (exec)
			md_sync_icache((uintptr_t)kbase, (u_int64_t)n * PAGE_SIZE);

		/* Map each cluster page.  Skip one a prior (non-sequential) cluster already
		 * materialised (return its frame rather than leak it).  Read-only pages are
		 * published to the file-page cache and mapped SHARED; writable pages private. */
		for (i = 0; i < n; i++)
		{
			u_int64_t v = pg + (u_int64_t)i * PAGE_SIZE;
			u_int64_t fr = (u_int64_t)base + (u_int64_t)i * PAGE_SIZE;

			if (md_user_mapped(aspace_root, v))
			{
				free_page((uintptr_t)fr);
				continue;
			}

			/* Cacheable and the frame fits the 32-bit cache key: publish it shared. */
			if (cacheable && (fr >> 32) == 0)
			{
				off_t vfoff = vma->vm_offset + (off_t)(v - vma->vm_start);
				u_int32_t winner = 0;

				if (vm_filecache_insert(bfd->mp, bfd->ino, vfoff, (u_int32_t)fr, &winner) == 0)
				{
					md_map_user_page_shared(aspace_root, v, fr, exec);
					continue;
				}
				if (winner != 0)
				{
					/* Lost the insert race: share the winner (already referenced for
					 * us by vm_filecache_insert), return our just-read frame. */
					md_map_user_page_shared(aspace_root, v, (u_int64_t)winner, exec);
					if (exec)
						md_sync_icache((uintptr_t)md_phys_to_virt(winner), PAGE_SIZE);
					free_page((uintptr_t)fr);
					continue;
				}
				/* Insert failed (OOM): fall through to a private mapping. */
			}

			md_map_user_page(aspace_root, v, fr, exec);
		}
	}
	else
	{
		/* VM_MAP_ANON: a single demand-zero page (no backing file to cluster). */
		uintptr_t frame = vmm_find_free_page(_current->id);

		if (frame == 0)
			return (-1);
		memset(md_phys_to_virt(frame), 0, PAGE_SIZE);
		md_map_user_page(aspace_root, pg, (u_int64_t)frame, exec);
	}
	return (0);
}
