/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Architecture-neutral user-space region policy for LP64 kernels — see
 * <vmm/uregion.h>.  Anonymous mmap and brk growth are pure page-counting plus
 * frame-allocate-zero-map loops; the per-arch page mapping is the
 * md_map_user_page() hook and the frame allocator is the generic
 * vmm_find_free_page().  Freshly allocated frames are identity-accessible (the
 * same assumption the generic ELF64 loader makes when it memset()s a frame),
 * so the zero-fill writes through the physical address directly.
 */

#include <sys/types.h>
#include <sys/elf_load.h> /* md_map_user_page */
#include <vmm/uregion.h>
#include <vmm/vmm.h>     /* vmm_find_free_page */
#include <vmm/paging.h>  /* PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <string.h>      /* memset */

/* 64-bit-safe page mask (paging.h's PAGE_MASK is an int and would sign-extend). */
#define UREGION_PAGE_MASK ((uintptr_t)PAGE_SIZE - 1)

/**
 * Allocate one zero-filled frame and map it read-write data at @va.
 *
 * @return 0 on success, -1 if no free frame was available.
 */
static int map_one_anon_page(u_int64_t *aspace_root, uintptr_t va)
{
	uintptr_t frame = vmm_find_free_page(sysID);

	if (frame == 0)
		return -1;
	/* Zero through the MI phys->virt window (aarch64 physmap / x86_64 P2V) — the
	 * kernel no longer has a low identity once TTBR0 is user-only.  The mapping
	 * still takes the PHYSICAL frame. */
	memset(md_phys_to_virt(frame), 0, PAGE_SIZE); /* anonymous pages read as zero */
	md_map_user_page(aspace_root, (u_int64_t)va, (u_int64_t)frame, 0 /* data, not executable */);
	return 0;
}

uintptr_t vmm_uregion_mmap_anon(u_int64_t *aspace_root, uintptr_t *next, size_t len, int fixed, uintptr_t fixed_addr)
{
	uintptr_t va, npages, i;

	if (aspace_root == 0)
		return 0;

	npages = ((uintptr_t)len + UREGION_PAGE_MASK) / PAGE_SIZE;
	if (npages == 0)
		return 0;

	if (fixed)
		va = fixed_addr & ~UREGION_PAGE_MASK;
	else
		va = *next;

	for (i = 0; i < npages; i++)
	{
		if (map_one_anon_page(aspace_root, va + i * PAGE_SIZE) != 0)
			return 0;
	}

	/* Only advance the bump cursor for non-fixed mappings; a high MAP_FIXED
	 * address must not push the cursor past it. */
	if (!fixed)
		*next = va + npages * PAGE_SIZE;
	return va;
}

uintptr_t vmm_uregion_brk(u_int64_t *aspace_root, uintptr_t *cur, uintptr_t newbrk)
{
	uintptr_t va;

	if (aspace_root == 0)
		return *cur;

	if (newbrk <= *cur) /* query or shrink request: report the current break */
		return *cur;

	for (va = (*cur + UREGION_PAGE_MASK) & ~UREGION_PAGE_MASK; va < newbrk; va += PAGE_SIZE)
	{
		if (map_one_anon_page(aspace_root, va) != 0)
			return *cur; /* OOM: leave the break where it was */
	}
	*cur = newbrk;
	return newbrk;
}
