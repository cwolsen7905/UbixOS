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
#include <sys/sysproto.h>      /* sys_disk_query_args (native ABI) */
#include <dev/disk.h>          /* disk_query — block-device enumeration syscall 68 */
#include <vmm/vmm.h>           /* vmm_find_free_page, PAGE_SIZE */
#include <lib/kmalloc.h>       /* sysID */
#include <string.h>            /* memset */
#include <x86_64/vmm_layout.h> /* MMAP_BASE, BRK_BASE (single source of truth) */
#include <sys/descrip.h>       /* getfd, struct file (file-backed mmap) */
#include <fs/vfs/file.h>       /* fileDescriptor_t, fread (file-backed mmap) */
#include <vmm/vm_filecache.h>  /* shared read-only file-page cache */
#include <ubixos/vitals.h>     /* systemVitals — uptime/freePages for sys_sysinfo */

void x86_64_map_user_page_shared(u64 pml4_phys, u64 va, u64 phys);

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
 * Map @npages at @va, each populated from @fd at byte offset @file_off + page*4K
 * (short reads leave the page-tail zero — covers a segment's .bss tail).  This is
 * how the dynamic linker loads a shared library's segments.  Pages are private
 * (no shared file-cache yet — correctness over the aarch64 optimisation).
 * @return 0 on success, -1 on OOM.
 */
static int map_file(u64 va, u64 npages, fileDescriptor_t *fd, u64 file_off, int writable)
{
	u64 i;
	/* A read-only, file-identified page is shared through the MI file-page cache so a
	 * library's text/rodata is ONE physical copy across every process (the dynamic
	 * linker's hot path: libc.so / libc++.so across the whole desktop).  Writable
	 * (data) pages stay private.  Mirrors aarch64's mmap. */
	int cacheable = !writable && fd->ino != 0;
	void *mp = fd->mp;
	u_int32_t ino = fd->ino;

	for (i = 0; i < npages; i++)
	{
		u64 va_pg = va + i * PAGE_SIZE;
		off_t foff = (off_t)(file_off + i * PAGE_SIZE);
		uintptr_t frame;

		/* Cache hit: share the cached frame read-only; allocate nothing. */
		if (cacheable)
		{
			uintptr_t hit = (uintptr_t)vm_filecache_lookup_ref(mp, ino, foff);
			if (hit != 0)
			{
				x86_64_map_user_page_shared(_current->md.md_cr3, va_pg, (u64)hit);
				continue;
			}
		}

		frame = vmm_find_free_page(sysID);
		if (frame == 0)
			return -1;
		memset(P2V(frame), 0, PAGE_SIZE);
		fd->offset = foff;
		fread((void *)P2V(frame), 1, PAGE_SIZE, fd); /* short read => zero tail */

		/* Cacheable + a 32-bit frame (the cache key is 32-bit): publish it shared. */
		if (cacheable && (frame >> 32) == 0)
		{
			u_int32_t winner = 0;
			if (vm_filecache_insert(mp, ino, foff, (u_int32_t)frame, &winner) == 0)
			{
				x86_64_map_user_page_shared(_current->md.md_cr3, va_pg, (u64)frame);
				continue;
			}
			if (winner != 0)
			{
				/* Lost the insert race: share the winner (referenced for us by
				 * vm_filecache_insert), return our frame. */
				x86_64_map_user_page_shared(_current->md.md_cr3, va_pg, (u64)winner);
				free_page((uintptr_t)frame);
				continue;
			}
		}

		/* Private page (writable data, or an oversized frame). */
		x86_64_map_user_page_to(_current->md.md_cr3, va_pg, (u64)frame, writable);
	}
	return 0;
}

/**
 * mmap(addr, len, prot, flags, fd, off) — private mapping, anonymous or
 * file-backed.  Anonymous (MAP_ANON 0x20, or no valid fd) is zero-filled; a
 * file-backed mapping reads the file's bytes into each page (the loader's path
 * for shared-library segments — without it ld-musl maps zeroed segments and any
 * lib it loads has an empty dynamic section).  Honours MAP_FIXED (0x10).  Syscall
 * 477 passes @off in PAGE_SIZE units (matching aarch64's musl).  @return the
 * mapped base (0 on failure).
 *
 * Returns the address DIRECTLY (not via td_retval): the syscall return slot
 * td_retval[] is int (32-bit), which cannot hold a 64-bit user VA, so the syscall
 * entry intercepts mmap/brk and puts this result straight in rax.
 */
u64 x86_64_user_mmap(u64 addr, u64 len, u64 prot, u64 flags, u64 fd, u64 off)
{
	u64 base;
	u64 npages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
	int writable = (prot & 0x2) != 0; /* PROT_WRITE */

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

	/* File-backed (MAP_ANON 0x20 clear) with a valid fd: read the file in.  A
	 * library's text/data segments are mapped writable here (relocations patch
	 * them); RELRO/mprotect-RO is a later refinement. */
	if ((flags & 0x20) == 0 && (int)fd >= 0)
	{
		struct file *fp = 0;
		if (getfd(&_current->td, &fp, (int)fd) != 0 || fp == 0 || fp->fd == 0)
			return 0;
		if (map_file(base, npages, fp->fd, off * PAGE_SIZE, writable) != 0)
			return 0;
	}
	else if (map_anon(base, npages) != 0)
	{
		return 0;
	}

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
 * Native syscall 68 — fill the userland buffer with the disk inventory (whole
 * disks + MBR partitions) for the graphical Disk Utility.  The arch-neutral
 * disk_query() (sys/kern/disk_query.c) drives md_disk_list() + MBR parsing;
 * mirrors the aarch64 handler.  The entry count is returned in td_retval[0].
 */
int sys_disk_query(struct thread *td, struct sys_disk_query_args *uap)
{
	td->td_retval[0] = disk_query(uap->buf, (int)uap->max);
	return (0);
}

/**
 * Native syscall 62 — fill @uap->out (struct ubix_sysinfo: 4 × u_int32_t) with
 * uptime, total/free physical pages, and page size.  Feeds Settings' memory
 * readout + ubix_sysinfo().  Mirrors the MI implementation in sys/kern/fb.c
 * (not linked on x86_64, so this is the x86_64 provider).
 */
int sys_sysinfo(struct thread *td, struct sys_sysinfo_args *uap)
{
	u_int32_t *out = (u_int32_t *)uap->out;

	if (out == 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}
	out[0] = systemVitals ? systemVitals->sysUptime : 0; /* uptime, seconds */
	out[1] = numPages;                                   /* total physical pages */
	out[2] = systemVitals ? systemVitals->freePages : 0; /* free physical pages */
	out[3] = PAGE_SIZE;                                  /* bytes per page */
	td->td_retval[0] = 0;
	return (0);
}

/* nanosleep wait channel — any address works (no one wakes it; the timeout
 * callout fires the wakeup).  cond returns 0 so the sleep always blocks for the
 * full duration. */
static int g_nanosleep_chan;
static int nanosleep_never(void *arg)
{
	(void)arg;
	return 0;
}

/**
 * nanosleep(2) (FreeBSD 240).  Sleeps the caller descheduled for the requested
 * duration via the callout-driven timed wait (a per-task callout wakes it at the
 * deadline) instead of busy-yielding — so a pacing daemon (e.g. aural) naps
 * without spinning.  Mirrors the i386 sys_nanosleep (sys/kern/syscall.c) but with
 * 64-bit register args.
 *
 * @param args the dispatched register args: args[0] = rqtp (timespec*), args[1] =
 *             rmtp (remaining, may be NULL).  uBixOS musl is _REDIR_TIME64, so
 *             timespec is { int64_t tv_sec; long tv_nsec } with tv_nsec at byte 8.
 */
int sys_nanosleep(struct thread *td, void *args)
{
	register_t *params = (register_t *)args;
	const char *rqtp = (const char *)(uintptr_t)params[0];
	char *rmtp = (char *)(uintptr_t)params[1];
	long long tv_sec;
	long tv_nsec;
	u_int32_t ticks;

	if (rqtp == 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}
	tv_sec = *(const long long *)(rqtp + 0);
	tv_nsec = *(const long *)(rqtp + 8);
	if (tv_sec < 0 || tv_nsec < 0 || tv_nsec >= 1000000000L)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	/* 100 Hz tick = 10 ms; round the request up to whole ticks. */
	ticks = (u_int32_t)(tv_sec * 100) + (u_int32_t)((tv_nsec + 9999999L) / 10000000L);
	if (ticks > 0)
		sched_wait_event_timeout(&g_nanosleep_chan, nanosleep_never, 0, ticks);

	if (rmtp != 0)
		memset(rmtp, 0, sizeof(long long) + sizeof(long));
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
