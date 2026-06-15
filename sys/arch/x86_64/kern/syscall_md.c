/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 machine-dependent syscall implementations (Phase 5e).  The MI POSIX
 * table dispatches most calls to arch-neutral code; the ones needing x86_64 VM /
 * register glue live here.  This batch is the mmap/VM surface (the prerequisite
 * for musl's allocator + TLS); exec/fork/signals follow.  Sibling of aarch64's
 * kern/syscall_md.c — both wrap the MI uregion policy (sys/vmm/vmm_uregion.c)
 * with the arch's page mapper + VA layout.
 */

#include "../x86_64.h"
#include <ubixos/sched.h>
#include <sys/thread.h>
#include <sys/sysproto_posix.h>
#include <vmm/vmm.h>     /* vmm_find_free_page, PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <string.h>      /* memset */

/* User-VA layout for the dynamic regions (above the ELF/stack area, in the clean
 * user low half).  Matches aarch64's choices for cross-arch consistency. */
#define MMAP_BASE 0x200000000UL /* anonymous mmap bump allocator */
#define BRK_BASE 0x1C0000000UL  /* program break (brk/sbrk) */

#define MSR_FS_BASE 0xC0000100

/**
 * Map @npages zeroed, writable, user pages at @va in the current address space.
 * Zero-fill goes through the physmap (P2V), so it works regardless of which CR3
 * is active (a syscall runs under the user CR3, which has no low identity for the
 * fresh frames).  @return 0 on success, -1 on OOM.
 */
static int map_anon(u64 va, u64 npages)
{
	u64 pml4 = _current->md.md_cr3;
	u64 i;

	for (i = 0; i < npages; i++)
	{
		uintptr_t frame = vmm_find_free_page(sysID);
		if (frame == 0)
			return -1;
		memset(P2V(frame), 0, PAGE_SIZE);
		x86_64_map_user_page_to(pml4, va + i * PAGE_SIZE, (u64)frame, 1 /* writable */);
	}
	return 0;
}

/**
 * mmap(addr, len, prot, flags) — anonymous private mapping (the case musl's
 * allocator uses).  File-backed mappings are treated as anonymous + zero for now
 * (static binaries don't need them).  Honours MAP_FIXED (0x10) with a non-zero
 * addr.  @return the mapped base (0 on failure).
 *
 * Returns the address DIRECTLY (not via td_retval): the syscall return slot
 * td_retval[] is int (32-bit), which cannot hold a 64-bit user VA, so the syscall
 * entry intercepts mmap/brk and puts this result straight in rax.
 */
u64 x86_64_user_mmap(u64 addr, u64 len, u64 prot, u64 flags)
{
	u64 base;
	u64 npages = (len + PAGE_SIZE - 1) / PAGE_SIZE;

	(void)prot;
	if (npages == 0 || _current->md.md_cr3 == 0)
		return 0;

	/* MAP_FIXED (0x10) with a non-zero hint maps there; otherwise bump-allocate
	 * from md_mmap_next (initialised to MMAP_BASE, above the identity window). */
	if ((flags & 0x10) && addr != 0)
	{
		base = addr & ~(u64)(PAGE_SIZE - 1);
	}
	else
	{
		if (_current->md.md_mmap_next == 0)
			_current->md.md_mmap_next = MMAP_BASE;
		base = _current->md.md_mmap_next;
	}

	if (map_anon(base, npages) != 0)
		return 0;

	if (!((flags & 0x10) && addr != 0))
		_current->md.md_mmap_next = base + npages * PAGE_SIZE;
	return base;
}

/**
 * brk(newbrk) — set/query the program break, Linux/musl-style (returns the
 * resulting break; brk(0) queries).  Grows by mapping anonymous pages from the
 * current break up to @newbrk; shrinking just lowers the cursor.  @return the
 * resulting break (returned directly, like mmap — see x86_64_user_mmap).
 */
u64 x86_64_user_brk(u64 newbrk)
{
	u64 cur;

	if (_current->md.md_brk == 0)
		_current->md.md_brk = BRK_BASE;
	cur = _current->md.md_brk;

	if (newbrk > cur)
	{
		u64 from = (cur + PAGE_SIZE - 1) & ~(u64)(PAGE_SIZE - 1);
		u64 to = (newbrk + PAGE_SIZE - 1) & ~(u64)(PAGE_SIZE - 1);
		if (to > from && map_anon(from, (to - from) / PAGE_SIZE) != 0)
			return cur; /* OOM: break unchanged */
		cur = newbrk;
	}
	else if (newbrk >= BRK_BASE)
	{
		cur = newbrk; /* shrink: drop the cursor (pages kept mapped, harmless) */
	}

	_current->md.md_brk = cur;
	return cur;
}

/* munmap/madvise/msync: the bump allocator does not reclaim yet, so these are
 * accepted as no-ops (musl tolerates this for its arena lifetime). */
int sys_munmap(struct thread *td, struct sys_munmap_args *uap)
{
	(void)uap;
	td->td_retval[0] = 0;
	return (0);
}

int sys_madvise(struct thread *td, struct sys_madvise_args *uap)
{
	(void)uap;
	td->td_retval[0] = 0;
	return (0);
}

int sys_msync(struct thread *td, struct sys_msync_args *uap)
{
	(void)uap;
	td->td_retval[0] = 0;
	return (0);
}

/**
 * Set the user TLS base.  musl's amd64 startup points %fs at its thread-control
 * block via arch_prctl(SET_FS); on UbixOS that arrives as machine_set_tls.  Write
 * the FS_BASE MSR and remember it so switch_to can restore it per task.
 */
void machine_set_tls(struct thread *td, uintptr_t base)
{
	(void)td;
	if (_current != 0)
		_current->md.md_fsbase = (u64)base;
	__asm__ __volatile__("wrmsr" : : "c"(MSR_FS_BASE), "a"((u32)base), "d"((u32)((u64)base >> 32)));
}

/** Page-attribute hook (mmap of shared/device pages).  No-op for now. */
int vmm_set_page_attributes(uintptr_t va, u_int32_t attr)
{
	(void)va;
	(void)attr;
	return (0);
}

/** sysGetFreePage: report the free physical page count. */
int sysGetFreePage(struct thread *td, u_int32_t *count)
{
	(void)count;
	td->td_retval[0] = (register_t)vmm_mem_free_pages();
	return (0);
}
