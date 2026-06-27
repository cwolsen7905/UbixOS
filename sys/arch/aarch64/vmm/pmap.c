/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 4 KB-granule page-table mapping (pmap), QEMU `virt` bring-up.
 *
 * mmu.c brings the MMU up with a coarse 512 × 1 GB identity map (block
 * descriptors at level 1).  This file adds fine-grained 4 KB mapping: walk the
 * 39-bit / 4 KB translation tree (L1→L2→L3, each level 512 entries covering
 * 1 GB / 2 MB / 4 KB), allocating intermediate tables on demand, and install an
 * L3 page descriptor.  This is the mechanism per-process user address spaces
 * (and the kernel's own fine mappings) are built from once exec/fork land.
 *
 * Table pages come from the physical frame allocator and are identity-mapped, so
 * a frame's physical address doubles as the kernel pointer used to write it.
 */

#include "bringup.h"
#include <vmm/vmm.h>
#include <vmm/paging.h>       /* PAGE_SIZE */
#include <vmm/vm_filecache.h> /* shared file-page cache (PTE_SHARED pages) */
#include <lib/kmalloc.h>      /* sysID */
#include <string.h>           /* memset, memcpy */

/* 39-bit VA, 4 KB granule: level index extractors. */
#define L1_IDX(va) (((va) >> 30) & 0x1FFUL)
#define L2_IDX(va) (((va) >> 21) & 0x1FFUL)
#define L3_IDX(va) (((va) >> 12) & 0x1FFUL)

/* Descriptor encoding. */
#define PTE_VALID (1UL << 0)
#define PTE_TABLE (3UL) /* L1/L2 descriptor pointing to the next level */
#define PTE_PAGE (3UL)  /* L3 page descriptor */
#define PTE_TYPE_MASK (3UL)
#define PTE_AF (1UL << 10)                /* access flag */
#define PTE_SH_INNER (3UL << 8)           /* inner shareable */
#define PTE_ATTR(i) ((u_int64_t)(i) << 2) /* MAIR AttrIndx */
#define PTE_AP_EL0 (1UL << 6)             /* AP[1]: also accessible at EL0 */
#define PTE_AP_RO (1UL << 7)              /* AP[2]: read-only */
#define PTE_PXN (1UL << 53)               /* privileged execute-never */
#define PTE_UXN (1UL << 54)               /* unprivileged execute-never */
#define PTE_COW (1UL << 55)               /* software bit: copy-on-write (frame shared RO) */
#define PTE_SHARED (1UL << 56)            /* software bit: shared file-cache page (RO, refcounted) */
#define PTE_WIRED (1UL << 57)             /* software bit: wired device/shared RW (fork shares verbatim, never freed) */

/* Output-address field of a descriptor (bits [47:12]). */
#define PTE_ADDR_MASK 0x0000FFFFFFFFF000UL

/* MAIR index for Normal write-back memory (matches mmu.c attr0). */
#define ATTR_NORMAL_IDX 0

/* Standard attributes for a kernel data page (Normal, inner-shareable, AF). */
#define PMAP_KERNEL_DATA (PTE_ATTR(ATTR_NORMAL_IDX) | PTE_SH_INNER | PTE_PXN | PTE_UXN)

/**
 * Return the next-level table for @table[@idx], allocating + linking a fresh
 * zeroed table if the slot is empty or currently a block descriptor.
 *
 * @return pointer to the next-level table (identity-mapped, so == its phys addr).
 */
static u_int64_t *table_next(u_int64_t *table, u_int64_t idx)
{
	u_int64_t e = table[idx];

	/* Reuse only an existing table descriptor (type bits == 11).  An invalid
	 * slot or a 1 GB/2 MB block is replaced by a freshly allocated table. */
	if ((e & PTE_VALID) != 0 && (e & PTE_TYPE_MASK) == PTE_TABLE)
		return (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(e & PTE_ADDR_MASK);

	u_int64_t phys = vmm_find_free_page(sysID);
	u_int64_t *next = (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(phys);
	memset(next, 0, PAGE_SIZE);
	table[idx] = (phys & PTE_ADDR_MASK) | PTE_TABLE;
	return next;
}

/**
 * Map one 4 KB page: virtual @va → physical @pa with descriptor @attrs in the
 * translation tree rooted at @l1.  Intermediate L2/L3 tables are allocated as
 * needed.  Invalidates the TLB for @va.
 *
 * @param attrs  lower/upper attribute bits (e.g. PMAP_KERNEL_DATA); PTE_AF and
 *               the L3 page type are added here.
 * @return 0 on success.
 */
int pmap_map_page(u_int64_t *l1, u_int64_t va, u_int64_t pa, u_int64_t attrs)
{
	u_int64_t *l2 = table_next(l1, L1_IDX(va));
	u_int64_t *l3 = table_next(l2, L2_IDX(va));

	l3[L3_IDX(va)] = (pa & PTE_ADDR_MASK) | attrs | PTE_AF | PTE_PAGE;

	__asm__ volatile("dsb ishst; tlbi vaae1is, %0; dsb ish; isb" : : "r"(va >> 12) : "memory");
	return 0;
}

/**
 * Map one 4 KB EL0-accessible (user) page: @va → @pa in the tree at @l1.
 *
 * @param executable  non-zero for user code (EL0-executable, EL1 exec denied);
 *                     zero for user data/stack (execute-never).
 * @return 0 on success.
 */
int pmap_map_user_page(u_int64_t *l1, u_int64_t va, u_int64_t pa, int executable)
{
	u_int64_t attrs = PTE_ATTR(ATTR_NORMAL_IDX) | PTE_SH_INNER | PTE_AP_EL0;

	if (executable)
		attrs |= PTE_PXN; /* runnable at EL0, never at EL1 */
	else
		attrs |= PTE_PXN | PTE_UXN; /* data/stack: never executable */

	return pmap_map_page(l1, va, pa, attrs);
}

/**
 * Map one 4 KB shared file-cache page: @va → @pa, read-only at EL0/EL1 and tagged
 * PTE_SHARED so the frame is refcounted by the file cache (not freed on unmap)
 * and shared as-is across fork.  A write takes a permission fault that
 * pmap_cow_fault turns into a private copy, so sharing is transparent.
 *
 * @param executable  non-zero for a library text page (EL0-executable).
 * @return 0 on success.
 */
int pmap_map_user_page_shared(u_int64_t *l1, u_int64_t va, u_int64_t pa, int executable)
{
	u_int64_t attrs = PTE_ATTR(ATTR_NORMAL_IDX) | PTE_SH_INNER | PTE_AP_EL0 | PTE_AP_RO | PTE_SHARED;

	if (executable)
		attrs |= PTE_PXN; /* runnable at EL0, never at EL1 */
	else
		attrs |= PTE_PXN | PTE_UXN;

	return pmap_map_page(l1, va, pa, attrs);
}

/**
 * Map one 4 KB wired user page: @va → @pa, EL0 read-write, tagged PTE_WIRED.
 *
 * For a frame the process does not own and must keep writing in place — the
 * virtio-gpu scanout framebuffer, a shared-region window buffer — where COW is
 * wrong: a copy-on-write would silently fork the writer off the real frame (the
 * compositor would then paint a private copy the scanout never sees).  fork
 * shares a wired page verbatim (RW, no COW) and teardown never frees it (the
 * device / region owner does), exactly like an MMIO mapping.
 *
 * @return 0 on success.
 */
int pmap_map_user_page_wired(u_int64_t *l1, u_int64_t va, u_int64_t pa)
{
	u_int64_t attrs = PTE_ATTR(ATTR_NORMAL_IDX) | PTE_SH_INNER | PTE_AP_EL0 | PTE_PXN | PTE_UXN | PTE_WIRED;

	return pmap_map_page(l1, va, pa, attrs);
}

/**
 * Resolve user VA @va to its physical frame address in the tree at @l1 by
 * walking L1→L2→L3.  Used by file-backed mmap to write file contents into a
 * mapped page via its identity (physical) alias — PAN-safe, like the ELF loader.
 *
 * @return the physical address @va maps to, or 0 if @va is unmapped.
 */
u_int64_t pmap_extract(u_int64_t *l1, u_int64_t va)
{
	u_int64_t e = l1[L1_IDX(va)];

	if ((e & PTE_VALID) == 0 || (e & PTE_TYPE_MASK) != PTE_TABLE)
		return 0;
	u_int64_t *l2 = (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(e & PTE_ADDR_MASK);
	e = l2[L2_IDX(va)];
	if ((e & PTE_VALID) == 0 || (e & PTE_TYPE_MASK) != PTE_TABLE)
		return 0;
	u_int64_t *l3 = (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(e & PTE_ADDR_MASK);
	e = l3[L3_IDX(va)];
	if ((e & PTE_VALID) == 0)
		return 0;
	return (e & PTE_ADDR_MASK) | (va & 0xFFFUL);
}

/* ---- machine-dependent hooks for the generic ELF64 loader (sys/elf_load.h) ---- */

/**
 * Map a user page into @aspace_root (the aarch64 L1) for the ELF loader.
 */
void md_map_user_page(u_int64_t *aspace_root, u_int64_t va, u_int64_t pa, int executable)
{
	pmap_map_user_page(aspace_root, va, pa, executable);
}

/**
 * Clean D-cache + invalidate I-cache for [@addr, @addr+@len) to the PoU so
 * freshly-loaded code is fetched correctly (64-byte line, QEMU cortex-a72).
 */
void md_sync_icache(uintptr_t addr, u_int64_t len)
{
	uintptr_t p, start = addr & ~63UL, end = addr + len;

	for (p = start; p < end; p += 64)
		__asm__ volatile("dc cvau, %0" : : "r"(p) : "memory");
	__asm__ volatile("dsb ish");
	for (p = start; p < end; p += 64)
		__asm__ volatile("ic ivau, %0" : : "r"(p) : "memory");
	__asm__ volatile("dsb ish; isb");
}

/** Reach a physical frame as a kernel pointer through the TTBR1 physmap
 * (PHYSMAP_BASE + phys).  Valid regardless of what TTBR0 holds, so kernel phys
 * access keeps working once TTBR0 carries only user mappings (Phase 4). */
void *md_phys_to_virt(u_int64_t phys)
{
	return (void *)(uintptr_t)AARCH64_VIRT_OF(phys);
}

/**
 * Return the active TTBR0 translation-table root (the kernel's identity L1).
 */
static u_int64_t *pmap_active_l1(void)
{
	u_int64_t ttbr0;
	__asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));
	return (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(ttbr0 & PTE_ADDR_MASK);
}

/**
 * Create a fresh per-process address space (TTBR0 root).
 *
 * The new L1 starts as a copy of the kernel's identity L1, so kernel code, the
 * page bitmap and the peripherals stay mapped while this address space is
 * active (no kernel-in-TTBR1 relocation yet).  Per-process user mappings are
 * then added with pmap_map_user_page() in VA blocks the kernel does not use.
 *
 * @return the new L1 table (identity-mapped, == its physical address).
 */
u_int64_t *pmap_create_user_space(void)
{
	u_int64_t *l1 = (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(vmm_find_free_page(sysID));

	/* Copy the KERNEL identity L1 (not whatever is active) so this never shares
	 * another process's user sub-tables — critical for fork correctness. */
	memcpy(l1, aarch64_kernel_l1(), PAGE_SIZE); /* 512 entries × 8 = one page */
	return l1;
}

/* User VA region for processes (block 4 = 4 GB and up): below this are the
 * kernel identity blocks (0-1) and bring-up demo scratch — not per-process data,
 * so fork shares them via the kernel-L1 copy rather than duplicating them. */
#define USER_L1_MIN 4

/**
 * Deep-copy the user mappings of @parent into a fresh child address space:
 * every mapped 4 KB user page (L1 index >= USER_L1_MIN) is duplicated into a new
 * frame with the parent's attributes preserved.  This is a full copy (not COW
 * yet) — correct and simple; COW is a later optimization.
 *
 * @return the child L1 root, or NULL on allocation failure.
 */
u_int64_t *pmap_fork_copy(u_int64_t *parent)
{
	u_int64_t *child = pmap_create_user_space(); /* kernel identity */
	u_int64_t i1, i2, i3;

	for (i1 = USER_L1_MIN; i1 < 512; i1++)
	{
		if ((parent[i1] & PTE_VALID) == 0 || (parent[i1] & PTE_TYPE_MASK) != PTE_TABLE)
			continue;
		u_int64_t *l2 = (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(parent[i1] & PTE_ADDR_MASK);

		for (i2 = 0; i2 < 512; i2++)
		{
			if ((l2[i2] & PTE_VALID) == 0 || (l2[i2] & PTE_TYPE_MASK) != PTE_TABLE)
				continue;
			u_int64_t *l3 = (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(l2[i2] & PTE_ADDR_MASK);

			for (i3 = 0; i3 < 512; i3++)
			{
				if ((l3[i3] & PTE_VALID) == 0)
					continue;
				u_int64_t va = (i1 << 30) | (i2 << 21) | (i3 << 12);
				uintptr_t src = (uintptr_t)(l3[i3] & PTE_ADDR_MASK);

				/* MMIO (a frame at/above RAM top, e.g. a device BAR) or a wired
				 * device/shared mapping (the GPU framebuffer, a shared-region buffer):
				 * share verbatim — the child sees the same frame and no COW desyncs the
				 * writer from it.  Wired frames are RAM-backed, so the address test
				 * alone misses them; the PTE_WIRED tag catches them. */
				if ((src >> 12) >= (uintptr_t)count_memory() || (l3[i3] & PTE_WIRED))
				{
					pmap_map_page(child,
					              va,
					              (u_int64_t)src,
					              (l3[i3] & ~PTE_ADDR_MASK) & ~(u_int64_t)(PTE_AF | PTE_PAGE));
					continue;
				}

				/* Shared file-cache page (already read-only): the child shares it
				 * verbatim and takes a cache reference.  No COW — a writer of either
				 * process copies out via pmap_cow_fault's PTE_SHARED path. */
				if (l3[i3] & PTE_SHARED)
				{
					pmap_map_page(child,
					              va,
					              (u_int64_t)src,
					              (l3[i3] & ~PTE_ADDR_MASK) & ~(u_int64_t)(PTE_AF | PTE_PAGE));
					vm_filecache_ref_phys((u_int32_t)src);
					continue;
				}

				/* COW: both parent and child reference the one frame, read-only,
				 * until one writes (the data-abort handler copies then).  Mark the
				 * parent RO+COW the first time the frame is shared; bump the shared
				 * frame's refcount by 2 (parent + child), or by 1 if it was already
				 * COW from an earlier fork. */
				int already = (l3[i3] & PTE_COW) != 0;
				if (!already)
				{
					l3[i3] |= PTE_AP_RO | PTE_COW;
					__asm__ volatile("dsb ishst; tlbi vaae1is, %0; dsb ish; isb"
					                 :
					                 : "r"(va >> 12)
					                 : "memory");
				}
				pmap_map_page(child,
				              va,
				              (u_int64_t)src,
				              (l3[i3] & ~PTE_ADDR_MASK) & ~(u_int64_t)(PTE_AF | PTE_PAGE));
				adjust_cow_counter(src, already ? 1 : 2);
			}
		}
	}
	return child;
}

/**
 * Resolve a write fault at user VA @va in the space rooted at @l1 on a page that
 * is shared read-only — either a copy-on-write fork page (PTE_COW) or a shared
 * file-cache page (PTE_SHARED).  Give this writer a private, writable copy:
 * allocate a frame, copy the shared page into it, point the PTE at the copy with
 * write permission and the share bits cleared, and release this writer's
 * reference to the old frame — decrementing the COW refcount, or dropping the
 * file-cache reference, as appropriate (the frame is freed once the last sharer
 * lets go).  A write fault on a page that is neither COW nor shared (a genuinely
 * read-only text page) is a real access violation — left for the caller's
 * SIGSEGV.
 *
 * @return 0 if the fault was a share fault and was resolved (retry the write);
 *         -1 if @va is not shared (real fault) or no frame was free.
 */
int pmap_cow_fault(u_int64_t *l1, u_int64_t va, pidType pid)
{
	u_int64_t e = l1[L1_IDX(va)];
	u_int64_t *l2, *l3;
	u_int64_t pte, attrs;
	uintptr_t old, neu;

	if (l1 == 0 || (e & PTE_VALID) == 0 || (e & PTE_TYPE_MASK) != PTE_TABLE)
		return -1;
	l2 = (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(e & PTE_ADDR_MASK);
	e = l2[L2_IDX(va)];
	if ((e & PTE_VALID) == 0 || (e & PTE_TYPE_MASK) != PTE_TABLE)
		return -1;
	l3 = (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(e & PTE_ADDR_MASK);
	pte = l3[L3_IDX(va)];

	if ((pte & PTE_VALID) == 0 || (pte & (PTE_COW | PTE_SHARED)) == 0)
		return -1; /* not shared → a real write to a read-only page */

	old = (uintptr_t)(pte & PTE_ADDR_MASK);
	neu = vmm_find_free_page(pid);
	if (neu == 0)
		return -1; /* out of memory → caller terminates the task */

	memcpy((void *)(uintptr_t)AARCH64_VIRT_OF(neu), (const void *)(uintptr_t)AARCH64_VIRT_OF(old), PAGE_SIZE);

	/* Re-point the PTE at the private copy: same attributes, but writable at EL0
	 * (clear AP[2]) and no longer shared. */
	attrs = (pte & ~PTE_ADDR_MASK) & ~(PTE_AP_RO | PTE_COW | PTE_SHARED);
	l3[L3_IDX(va)] = ((u_int64_t)neu & PTE_ADDR_MASK) | attrs;
	__asm__ volatile("dsb ishst; tlbi vaae1is, %0; dsb ish; isb" : : "r"(va >> 12) : "memory");

	/* Release this writer's hold on the old frame: a file-cache page drops a
	 * cache reference (freed at the last unref), a COW page decrements the COW
	 * refcount.  PTE_SHARED frames are always cache pages we sized to 32 bits. */
	if (pte & PTE_SHARED)
		vm_filecache_unref_phys((u_int32_t)old);
	else
		adjust_cow_counter(old, -1);
	return 0;
}

/**
 * Free the user mappings of @l1 and the tables that describe them, returning the
 * physical frames to the allocator.  Only the per-process user region (L1 index
 * >= USER_L1_MIN) is touched — the kernel identity entries (0..3), which point
 * at shared kernel tables, are left alone — and the @l1 top-level page itself is
 * freed last.  free_page() is COW/MMIO-safe: it bounds-checks frames above RAM
 * (the GPU scanout etc. are no-ops) and decrements the COW counter for shared
 * frames rather than freeing them.
 *
 * Used by execve to reclaim the replaced image's pages (which are this process's
 * private fork-copies / load_dynamic frames); without it every exec leaked the
 * whole old address space.  Must run AFTER switching off @l1 (it is no longer
 * the active TTBR0) and never on the shared kernel L1.
 */
void pmap_free_user_space(u_int64_t *l1)
{
	u_int64_t i1, i2, i3;

	if (l1 == 0 || l1 == aarch64_kernel_l1())
		return;

	for (i1 = USER_L1_MIN; i1 < 512; i1++)
	{
		u_int64_t *l2;

		if ((l1[i1] & PTE_VALID) == 0 || (l1[i1] & PTE_TYPE_MASK) != PTE_TABLE)
			continue;
		l2 = (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(l1[i1] & PTE_ADDR_MASK);

		for (i2 = 0; i2 < 512; i2++)
		{
			u_int64_t *l3;

			if ((l2[i2] & PTE_VALID) == 0 || (l2[i2] & PTE_TYPE_MASK) != PTE_TABLE)
				continue;
			l3 = (u_int64_t *)(uintptr_t)AARCH64_VIRT_OF(l2[i2] & PTE_ADDR_MASK);

			for (i3 = 0; i3 < 512; i3++)
			{
				if ((l3[i3] & PTE_VALID) == 0)
					continue;
				/* A wired device/shared page (GPU framebuffer, shared-region buffer)
				 * is owned elsewhere — drop the mapping but never free the frame.  A
				 * shared file-cache page drops a cache reference (freed at the last
				 * unref).  Everything else goes back to the allocator (free_page is
				 * COW-/MMIO-safe). */
				if (l3[i3] & PTE_WIRED)
					; /* owned by the device/region; leave the frame alone */
				else if (l3[i3] & PTE_SHARED)
					vm_filecache_unref_phys((u_int32_t)(l3[i3] & PTE_ADDR_MASK));
				else
					free_page((uintptr_t)(l3[i3] & PTE_ADDR_MASK));
			}

			free_page(AARCH64_PHYS_OF((uintptr_t)l3)); /* the L3 table */
		}
		free_page(AARCH64_PHYS_OF((uintptr_t)l2)); /* the L2 table */
	}
	free_page(AARCH64_PHYS_OF((uintptr_t)l1)); /* the top-level L1 copy */
}

/**
 * Make @l1 the active TTBR0 address space and flush stale translations.
 */
void pmap_switch(u_int64_t *l1)
{
	__asm__ volatile("msr ttbr0_el1, %0; isb; tlbi vmalle1; dsb nsh; isb"
	                 :
	                 : "r"((u_int64_t)AARCH64_PHYS_OF((uintptr_t)l1))
	                 : "memory");
}

/**
 * Demonstrate per-process address-space isolation: map the same user VA to two
 * different frames in two separate address spaces, switch between them, and
 * confirm each sees only its own frame.  Throwaway bring-up scaffolding.
 */
void aarch64_aspace_demo(void)
{
	u_int64_t *kernel_l1 = pmap_active_l1();
	u_int64_t *a, *b;
	uintptr_t fa, fb;
	/* A user VA in a 1 GB block no other demo has touched: blocks 0 (peripherals),
	 * 1 (RAM/kernel), 2 (pmap demo) and 3 (syscall demo) are in use, so a shared
	 * sub-table would otherwise leak across the copied L1s.  Use block 4 (4 GB),
	 * still a clean identity block, so each space allocates its own L2/L3. */
	u_int64_t va = 0x100004000UL;
	u_int64_t in_a, in_b;

	kprintf("aspace demo: per-process address-space isolation...\n");

	a = pmap_create_user_space();
	b = pmap_create_user_space();
	fa = vmm_find_free_page(sysID);
	fb = vmm_find_free_page(sysID);
	*(volatile u_int64_t *)fa = 0xA0A0A0A0UL;
	*(volatile u_int64_t *)fb = 0xB0B0B0B0UL;

	pmap_map_page(a, va, (u_int64_t)fa, PMAP_KERNEL_DATA);
	pmap_map_page(b, va, (u_int64_t)fb, PMAP_KERNEL_DATA);

	pmap_switch(a);
	in_a = *(volatile u_int64_t *)va;
	pmap_switch(b);
	in_b = *(volatile u_int64_t *)va;
	pmap_switch(kernel_l1); /* restore the kernel's identity address space */

	kprintf("  VA 0x%lX: space A reads 0x%lX, space B reads 0x%lX (isolated=%s)\n",
	        va,
	        in_a,
	        in_b,
	        (in_a == 0xA0A0A0A0UL && in_b == 0xB0B0B0B0UL) ? "yes" : "NO");
	kprintf("aspace demo: per-process address spaces work on aarch64.\n");
}

/**
 * Exercise pmap_map_page in the live TTBR0 tree: map a 4 KB page at an unused
 * VA (in a 1 GB slot the kernel doesn't touch — replacing its identity block
 * with a real L2/L3 chain) and confirm the mapping is bidirectional with the
 * frame's identity alias.  Throwaway bring-up scaffolding.
 */
void aarch64_pmap_demo(void)
{
	u_int64_t ttbr0;
	u_int64_t *l1;
	uintptr_t frame;
	u_int64_t via_va;
	const u_int64_t test_va = 0x80000000UL; /* 2 GB: unused 1 GB slot above RAM */
	const u_int64_t pattern = 0x1122334455667788UL;

	__asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));
	l1 = (u_int64_t *)(uintptr_t)(ttbr0 & PTE_ADDR_MASK);

	kprintf("pmap demo: 4KB-granule mapping...\n");

	frame = vmm_find_free_page(sysID);
	*(volatile u_int64_t *)frame = pattern; /* write through the identity alias */

	pmap_map_page(l1, test_va, (u_int64_t)frame, PMAP_KERNEL_DATA);

	via_va = *(volatile u_int64_t *)test_va; /* read through the new mapping */
	kprintf("  VA 0x%lX -> frame 0x%lX; read via VA = 0x%lX (%s)\n",
	        test_va,
	        (u_int64_t)frame,
	        via_va,
	        via_va == pattern ? "match" : "MISMATCH");

	*(volatile u_int64_t *)test_va = 0xFEEDFACEUL; /* write via VA, read via identity */
	kprintf("  wrote via VA; frame identity alias holds 0x%lX (%s)\n",
	        *(volatile u_int64_t *)frame,
	        *(volatile u_int64_t *)frame == 0xFEEDFACEUL ? "match" : "MISMATCH");

	kprintf("pmap demo: 4KB page mapping works on aarch64.\n");
}
