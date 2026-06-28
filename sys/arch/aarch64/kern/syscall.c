/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 syscall dispatch (QEMU `virt` bring-up).
 *
 * Minimal SVC dispatcher: the EL1 sync handler (exceptions.c) decodes an EL0
 * SVC and calls aarch64_syscall(number, args) with the saved x0-x5.  Numbers
 * follow the FreeBSD ABI the i386 POSIX table already uses (write=4, exit=1,
 * getpid=20), so this dovetails with the generic syscall table once the rest of
 * the kernel is ported — for now it is a hand-rolled handful routed straight to
 * the console.  A real process exits via aarch64_el0_return().
 */

#include "bringup.h"
#include <sys/types.h>
#include <sys/errno.h>          /* EINVAL (prlimit64) */
#include <ubixos/sched.h>       /* _current, sched_yield */
#include <ubixos/endtask.h>     /* endTask */
#include <vmm/vmm.h>            /* address-space helpers */
#include <vmm/vm_filecache.h>   /* shared file-page cache (read-only mmap sharing) */
#include <vmm/uregion.h>        /* vmm_uregion_mmap_anon, vmm_uregion_brk */
#include <sys/klog.h>           /* klog_read_wait — native klog_read read directly */
#include <aarch64/vmm_layout.h> /* MMAP_BASE / BRK_BASE (user-VA layout) */
#include <sys/sysproto_posix.h> /* sys_open/read/close/lseek + uap structs */
#include <sys/descrip.h>        /* getfd, struct file */
#include <mpi/mpi.h>            /* MPI mailboxes (native syscalls 50-53) */
#include <string.h>             /* strncpy (getcwd) */
#include <fs/vfs/stat.h>        /* struct statx (sys_statx) */
#include <ubixos/syscalls.h>    /* systemCalls[] / systemCalls_posix[] tables */
#include <sys/elf_load.h>       /* md_sync_icache (file-backed/exec mmap) */
#include <fs/vfs/file.h>        /* fread (file-backed mmap) */
#include <ubixos/spinlock.h>    /* g_shmap_lock (MAP_SHARED write-back registry) */

/* Generic table-driven dispatch engine (sys/kern/syscall_dispatch.c). */
register_t ksyscall_dispatch(
    struct thread *td, struct syscall_entry *tbl, int count, u_int32_t number, register_t *args);

#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_FORK 2
#define SYS_RFORK 251             /* FreeBSD ABI — musl __clone (clone.s) routes here; see SYS_RFORK case */
#define CLONE_THREAD 0x00010000UL /* Linux clone flag in x0: set by pthread_create, clear by posix_spawn */
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_WAIT4 7           /* FreeBSD ABI (the number musl actually emits) */
#define SYS_EXECVE 59         /* FreeBSD ABI (the number musl actually emits) */
#define SYS_IOCTL 54          /* FreeBSD ABI; musl isatty() probes the tty with it */
#define SYS_WRITEV 121        /* FreeBSD ABI; the dynamic linker uses it for diagnostics */
#define SYS_SCHED_YIELD 331   /* UbixOS ABI (musl's sched_yield maps here) */
#define SYS_GETDENTS 272      /* FreeBSD getdirentries; musl getdents maps here */
#define SYS_OPENAT 499        /* FreeBSD ABI; musl openat (relative to a dir fd) */
#define SYS_CLOCK_GETTIME 232 /* FreeBSD ABI */
#define SYS_STATX 383         /* Linux slot; musl uses it for stat/fstat */
#define SYS_FCNTL 92          /* FreeBSD ABI */
#define SYS_MUNMAP 73         /* FreeBSD ABI */
#define SYS_NANOSLEEP 240     /* FreeBSD ABI */
#define SYS_UNAME 164         /* FreeBSD ABI */
#define SYS_GETUID 24         /* FreeBSD ABI */
#define SYS_GETEUID 25        /* FreeBSD ABI */
#define SYS_GETGID 47         /* FreeBSD ABI */
#define SYS_GETEGID 43        /* FreeBSD ABI */
#define SYS_SETUID 23         /* FreeBSD ABI */
#define SYS_SETGID 181        /* FreeBSD ABI */
#define SYS_BRK 17
#define SYS_GETPID 20
#define SYS_MPROTECT 74
#define SYS_FSTAT 189
#define SYS_MMAP 477
#define SYS_LSEEK 478
#define SYS_SET_TID_ADDRESS 258
#define SYS_RT_SIGPROCMASK 340 /* musl blocks/restores signals around fork() */
/* FreeBSD has no prlimit64; musl's aarch64 ABI hardcodes __NR_prlimit64=1024 and
 * routes get/setrlimit through it, so it lands out of the FreeBSD table's range.
 * Implement it natively against the per-thread rlim[] table. */
#define SYS_PRLIMIT64 1024

/* MMAP_BASE / BRK_BASE (and the exec loader's DYN_ and USER_STACK_ regions) now
 * live in <aarch64/vmm_layout.h> — the single source of truth for the user-VA
 * layout. */

/* musl routes the Linux-compat primitives (exit_group, set_thread_area, futex)
 * through the UbixOS-native int $0x81 table by ORing in this flag (matches the
 * i386 port; see project_native_abi_threading). */
#define NATIVE_FLAG 0x8000
#define NATIVE_EXIT_GROUP 65

/* UbixOS-native (int $0x81-equivalent) syscall numbers used by lib/ubix_api. */
#define NATIVE_MPI_CREATE 50
#define NATIVE_MPI_DESTROY 51
#define NATIVE_MPI_POST 52
#define NATIVE_MPI_FETCH 53
#define NATIVE_GETCWD 41    /* ubix_getcwd(buf, size) */
#define NATIVE_KLOG_READ 47 /* klog_read(buf, max, start_seq) — read args directly (see below) */

/*
 * Shared writable file mappings pending write-back.  uBixOS mmap is
 * eager-private: a file's pages are read into PRIVATE frames and never flushed
 * to the backing file, so a MAP_SHARED writer — notably ld.lld's
 * FileOutputBuffer, which mmaps the output file, fills it, then munmaps to
 * commit — used to produce a 0-byte file.  Each MAP_SHARED + PROT_WRITE file
 * mapping is recorded here at mmap time so munmap can write its dirty pages back
 * through the VFS.  Bounded; on overflow the mapping is simply not tracked
 * (degrades to the old no-flush behaviour).
 */
struct aarch64_shmap
{
	pidType pid;
	uintptr_t va;
	size_t len;
	off_t foff;
	char path[512];
	int active;
};
static struct aarch64_shmap g_shmaps[32];
static struct spinLock g_shmap_lock = SPIN_LOCK_INITIALIZER;

/*
 * The mmap bump cursor + program break describe the ADDRESS SPACE, not the task.
 * rfork(RFMEM) threads share one TTBR0, so a PER-TASK cursor makes every thread
 * start allocating at MMAP_BASE again and map on top of the regions its siblings
 * are already using — silent heap/stack corruption (the on-device ld.lld worker
 * remapped the main thread's malloc arena, so mallocng's alloc_slot spun on a
 * trashed meta pointer).  The thread-group LEADER (id == tgid) holds the canonical
 * cursors; every thread allocates through the leader under this lock.  A normal
 * single-threaded process is its own leader, so this is a no-op there.
 */
static struct spinLock g_as_region_lock = SPIN_LOCK_INITIALIZER;

/**
 * Return the task that owns this address space's shared mmap/brk cursors — the
 * thread-group leader (id == tgid), or @_current itself when it is the leader (or
 * the leader has already exited).
 */
static kTask_t *aarch64_as_owner(void)
{
	if (_current->tgid != 0 && _current->tgid != (u_int32_t)_current->id)
	{
		kTask_t *leader = schedFindTask(_current->tgid);
		if (leader != 0)
			return (leader);
	}
	return (_current);
}

/**
 * Terminate the current task (shared by exit / exit_group): a scheduled user
 * task is reaped + rescheduled; a bring-up enter_el0 demo longjmps back.
 */
static void do_exit(u_int64_t code)
{
	kprintf("[kernel] EL0 process exit(%lu)\n", code);
	if (_current != 0 && _current->md.md_usp != 0)
	{
		_current->exit_code = (u_int32_t)(code & 0xff); /* saved for wait4 (W_EXITED) */
		endTask(_current->id);
		sched_yield();
	}
	else
	{
		aarch64_el0_return();
	}
}

/**
 * write(fd, buf, len): copy @len bytes from the (currently-mapped) user buffer
 * to the console.  Bring-up only — ignores @fd and does no buffer validation.
 *
 * @return number of bytes written.
 */
static u_int64_t sc_write(u_int64_t fd, u_int64_t buf, u_int64_t len)
{
	const char *p = (const char *)(uintptr_t)buf;

	(void)fd;
	for (u_int64_t i = 0; i < len; i++)
	{
		if (p[i] == '\n')
			uart_putc('\r'); /* serial terminals need CR+LF, not a bare LF */
		uart_putc(p[i]);
	}
	return len;
}

/**
 * Return the current task's open-file entry for @fd, or NULL if out of range /
 * unset.  Used by the write path to decide whether an fd needs real fileops
 * dispatch (pty / socket) versus the bring-up UART shortcut.
 */
static struct file *aarch64_lookup_fd(int fd)
{
	if (_current == 0 || fd < 0 || fd >= O_FILES)
		return (0);
	return (struct file *)_current->td.o_files[fd];
}

/**
 * Record a MAP_SHARED writable file mapping so munmap can flush it back to @path.
 * Silently drops the mapping (no write-back) if the table is full.
 */
static void shmap_register(pidType pid, uintptr_t va, size_t len, off_t foff, const char *path)
{
	int i;

	spinLock(&g_shmap_lock);
	for (i = 0; i < (int)(sizeof(g_shmaps) / sizeof(g_shmaps[0])); i++)
	{
		if (!g_shmaps[i].active)
		{
			g_shmaps[i].pid = pid;
			g_shmaps[i].va = va;
			g_shmaps[i].len = len;
			g_shmaps[i].foff = foff;
			strncpy(g_shmaps[i].path, path, sizeof(g_shmaps[i].path) - 1);
			g_shmaps[i].path[sizeof(g_shmaps[i].path) - 1] = '\0';
			g_shmaps[i].active = 1;
			break;
		}
	}
	spinUnlock(&g_shmap_lock);
}

/**
 * Write a shared mapping's pages back to its backing file: reopen the file r+w
 * and copy each mapped page (reached through the TTBR1 physmap) out via the VFS.
 * Runs without g_shmap_lock held — fopen/fwrite take vfs_io_lock.
 */
static void shmap_writeback(const struct aarch64_shmap *m)
{
	fileDescriptor_t *fd;
	u_int64_t *l1;
	size_t done;

	if (_current == 0 || _current->md.md_ttbr0 == 0)
		return;
	fd = fopen(m->path, "r+");
	if (fd == 0)
		return;
	l1 = (u_int64_t *)(uintptr_t)_current->md.md_ttbr0;
	fd->offset = m->foff;
	for (done = 0; done < m->len; done += PAGE_SIZE)
	{
		uintptr_t phys = (uintptr_t)pmap_extract(l1, m->va + done) & ~0xFFFUL;
		size_t chunk = (m->len - done < PAGE_SIZE) ? (m->len - done) : PAGE_SIZE;

		if (phys == 0)
		{
			fd->offset += chunk; /* unmapped hole — leave the file's bytes as-is */
			continue;
		}
		fwrite((void *)(uintptr_t)AARCH64_VIRT_OF(phys), (int)chunk, 1, fd);
	}
	fclose(fd);
}

/**
 * munmap hook: flush + release every shared mapping for @pid that starts at @va.
 * Copies the entry out under the lock, then writes it back lock-free.
 */
static void shmap_unmap(pidType pid, uintptr_t va)
{
	for (;;)
	{
		struct aarch64_shmap m;
		int i, found = 0;

		spinLock(&g_shmap_lock);
		for (i = 0; i < (int)(sizeof(g_shmaps) / sizeof(g_shmaps[0])); i++)
		{
			if (g_shmaps[i].active && g_shmaps[i].pid == pid && g_shmaps[i].va == va)
			{
				m = g_shmaps[i];
				g_shmaps[i].active = 0;
				found = 1;
				break;
			}
		}
		spinUnlock(&g_shmap_lock);
		if (!found)
			break;
		shmap_writeback(&m);
	}
}

/**
 * mmap(addr, len, prot, flags, fd, off): private mapping, anonymous or
 * file-backed.  Arch glue around the generic policy (vmm_uregion_mmap_anon):
 * owns the VA-layout choice (where this address space's mmap region starts) and
 * the TTBR0 root; the page-counting / allocate-zero-map work is neutral.
 *
 * For a file-backed mapping (no MAP_ANON and a valid fd) the region is reserved
 * + zero-mapped as usual, then the file's [off, off+len) bytes are read into each
 * page via its physical (identity) alias — the path musl's dynamic linker uses to
 * map shared libraries (map_library).  prot is advisory here: pages are mapped
 * EL0-RW (and the loader mprotects code segments via a separate call); the only
 * prot bit honoured is PROT_EXEC, which marks the page EL0-executable.
 *
 * @return the mapped base VA, or (void*)-1 on failure.
 */
static u_int64_t sc_mmap(u_int64_t addr, u_int64_t len, u_int64_t prot, u_int64_t flags, u_int64_t fd, u_int64_t off)
{
	u_int64_t *l1;
	uintptr_t next, va;

	kTask_t *as_owner;

	if (_current == 0 || _current->md.md_ttbr0 == 0)
		return (u_int64_t)-1;

	l1 = (u_int64_t *)(uintptr_t)_current->md.md_ttbr0;

	/* Allocate from the address space's SHARED cursor (the thread-group leader's),
	 * not this task's — otherwise rfork siblings overlap each other's mappings.
	 * MAP_FIXED is FreeBSD flag 0x10: the caller (mallocng guard page, loader
	 * segment) requires the mapping to land at addr. */
	as_owner = aarch64_as_owner();
	spinLock(&g_as_region_lock);
	if (as_owner->md.md_mmap_next == 0)
		as_owner->md.md_mmap_next = MMAP_BASE;
	next = (uintptr_t)as_owner->md.md_mmap_next;
	va = vmm_uregion_mmap_anon(l1, &next, (size_t)len, (flags & 0x10) && addr != 0, (uintptr_t)addr);
	as_owner->md.md_mmap_next = (u_int64_t)next;
	spinUnlock(&g_as_region_lock);
	if (va == 0)
		return (u_int64_t)-1;

	/* File-backed (MAP_ANON is 0x20): populate each page from the file.  A
	 * read-only, file-identified page is shared through the file-page cache so a
	 * library's text/rodata is one physical copy across every process (the dynamic
	 * linker's hot path); writable/anonymous pages stay private.  Syscall 477 is
	 * mmap2, so @off arrives in PAGE_SIZE units. */
	if ((flags & 0x20) == 0 && (int)fd >= 0)
	{
		struct file *fp = 0;
		if (getfd(&_current->td, &fp, (int)fd) == 0 && fp != 0 && fp->fd != 0)
		{
			u_int64_t done;
			int ro = ((prot & 0x2) == 0); /* PROT_WRITE == 0x2 */
			int ex = ((prot & 0x4) != 0); /* PROT_EXEC  == 0x4 */
			int cacheable = ro && fp->fd->ino != 0;
			void *mp = fp->fd->mp;
			u_int32_t ino = fp->fd->ino;

			for (done = 0; done < len; done += PAGE_SIZE)
			{
				u_int64_t va_pg = va + done;
				off_t foff = (off_t)(off * PAGE_SIZE) + (off_t)done;
				uintptr_t priv = (uintptr_t)pmap_extract(l1, va_pg) & ~0xFFFUL;
				size_t chunk = (len - done < PAGE_SIZE) ? (size_t)(len - done) : PAGE_SIZE;

				if (priv == 0)
					break; /* region not fully mapped (shouldn't happen) */

				/* Cache hit: share the cached frame, return the reserved one. */
				if (cacheable)
				{
					uintptr_t hit = (uintptr_t)vm_filecache_lookup_ref(mp, ino, foff);
					if (hit != 0)
					{
						pmap_map_user_page_shared(l1, va_pg, (u_int64_t)hit, ex);
						if (ex)
							md_sync_icache(AARCH64_VIRT_OF(hit), PAGE_SIZE);
						free_page(priv);
						continue;
					}
				}

				/* Miss: read the file into the reserved private frame.  The kernel
				 * reaches the frame through the TTBR1 physmap (AARCH64_VIRT_OF) — the
				 * raw physical `priv` is only valid for pmap/cache bookkeeping, not as a
				 * kernel pointer now that TTBR0 carries user mappings only.  Positional
				 * (vfs_pread_locked) — NOT "fd->offset = foff; fread()" — so two CPUs
				 * faulting pages of the same mmap'd file (e.g. a shared library) can't
				 * clobber the shared fd->offset around the read (SMP "not loadable"). */
				if (vfs_pread_locked(fp->fd, (void *)(uintptr_t)AARCH64_VIRT_OF(priv), foff, chunk) ==
				        0 &&
				    !cacheable)
					break; /* EOF on a private mapping: leave the rest zero (bss tail) */

				/* Cacheable and the frame fits a 32-bit cache key: publish it shared. */
				if (cacheable && (priv >> 32) == 0)
				{
					u_int32_t winner = 0;
					if (vm_filecache_insert(mp, ino, foff, (u_int32_t)priv, &winner) == 0)
					{
						pmap_map_user_page_shared(l1, va_pg, (u_int64_t)priv, ex);
						if (ex)
							md_sync_icache(AARCH64_VIRT_OF(priv), PAGE_SIZE);
						continue;
					}
					if (winner != 0)
					{
						/* Lost the insert race: share the winner (already referenced
						 * for us by vm_filecache_insert), return our frame. */
						pmap_map_user_page_shared(l1, va_pg, (u_int64_t)winner, ex);
						if (ex)
							md_sync_icache(AARCH64_VIRT_OF(winner), PAGE_SIZE);
						free_page(priv);
						continue;
					}
				}

				/* Private page (writable data, or an oversized frame): keep the RW
				 * mapping uregion installed; mark it executable if requested. */
				if (ex)
				{
					pmap_map_user_page(l1, va_pg, (u_int64_t)priv, 1);
					md_sync_icache(AARCH64_VIRT_OF(priv), PAGE_SIZE);
				}
			}

			/* MAP_SHARED (0x01) + PROT_WRITE: the writer expects its stores to
			 * reach the file (ld.lld's mmap'd output).  Pages are private RW frames
			 * above; record the mapping so munmap flushes them back.  MAP_PRIVATE,
			 * read-only, and anonymous mappings are deliberately NOT tracked. */
			if ((flags & 0x01) && (prot & 0x2) && fp->fd->fileName[0] != '\0')
				shmap_register(_current->id,
				               (uintptr_t)va,
				               (size_t)len,
				               (off_t)(off * PAGE_SIZE),
				               fp->fd->fileName);

			return (u_int64_t)va;
		}
	}

	/* Anonymous (or file-backed with no usable fd): honour PROT_EXEC. */
	if ((prot & 0x4) != 0)
	{
		u_int64_t done;
		for (done = 0; done < len; done += PAGE_SIZE)
		{
			u_int64_t phys = pmap_extract(l1, va + done);
			if (phys == 0)
				continue;
			pmap_map_user_page(l1, va + done, phys, 1 /* executable */);
			md_sync_icache((uintptr_t)AARCH64_VIRT_OF(phys), PAGE_SIZE);
		}
	}

	return (u_int64_t)va;
}

/**
 * brk(newbrk): set the program break, Linux-style — returns the resulting break
 * (musl's mallocng glue.h treats the return as the new break; brk(0) queries).
 * Arch glue around the generic vmm_uregion_brk policy; owns only the heap-base
 * VA-layout choice and the TTBR0 root.
 *
 * @return the (new) program break.
 */
static u_int64_t sc_brk(u_int64_t newbrk)
{
	u_int64_t *l1;
	kTask_t *as_owner;
	u_int64_t r;

	if (_current == 0 || _current->md.md_ttbr0 == 0)
		return BRK_BASE;

	l1 = (u_int64_t *)(uintptr_t)_current->md.md_ttbr0;

	/* The program break is per-ADDRESS-SPACE: share the thread-group leader's so
	 * rfork siblings don't each grow a private brk over the others' heap. */
	as_owner = aarch64_as_owner();
	spinLock(&g_as_region_lock);
	if (as_owner->md.md_brk == 0)
		as_owner->md.md_brk = BRK_BASE;
	r = (u_int64_t)vmm_uregion_brk(l1, (uintptr_t *)&as_owner->md.md_brk, (uintptr_t)newbrk);
	spinUnlock(&g_as_region_lock);
	return (r);
}

/*
 * nanosleep helper: a never-satisfied condition makes sched_wait_event_timeout()
 * sleep the full requested duration (the task is dequeued — zero CPU — and woken
 * only by its one-shot timeout callout).  The channel is a private address no
 * waker posts to, so the timeout is the sole wakeup (a signal also wakes it,
 * which is the correct early-return behaviour).
 */
static int g_nanosleep_chan;
static int nanosleep_never(void *arg)
{
	(void)arg;
	return 0;
}

/**
 * Dispatch an EL0 syscall.  @args points at the saved x0..x5 (x0 = arg0).
 *
 * @return the value to place in the caller's x0 (exit does not return).
 */
u_int64_t aarch64_syscall(u_int64_t number, u_int64_t *args)
{
	/* Native (int $0x81-equivalent) syscalls musl ORs with NATIVE_FLAG. */
	if (number & NATIVE_FLAG)
	{
		u_int64_t n = number & ~(u_int64_t)NATIVE_FLAG;
		switch (n)
		{
			case NATIVE_EXIT_GROUP:
				do_exit(args[0]);
				return 0; /* unreachable */

			/* UbixOS MPI: the syscall runs in the caller's address space (IRQ-
			 * masked), so the user name/message pointers are directly valid. */
			case NATIVE_MPI_CREATE:
				return (u_int64_t)mpi_createMbox((char *)(uintptr_t)args[0]);
			case NATIVE_MPI_DESTROY:
				return (u_int64_t)mpi_destroyMbox((char *)(uintptr_t)args[0]);
			case NATIVE_MPI_POST:
				return (u_int64_t)mpi_postMessage((char *)(uintptr_t)args[0],
				                                  (u_int32_t)args[1],
				                                  (mpi_message_t *)(uintptr_t)args[2]);
			case NATIVE_MPI_FETCH:
				return (u_int64_t)mpi_fetchMessage((char *)(uintptr_t)args[0],
				                                   (mpi_message_t *)(uintptr_t)args[1]);

			case NATIVE_GETCWD:
			{
				/* ubix_getcwd(buf, size): copy the task's cwd (default "/"). */
				char *ubuf = (char *)(uintptr_t)args[0];
				u_int64_t size = args[1];
				const char *cwd =
				    (_current != 0 && _current->oInfo.cwd[0] != '\0') ? _current->oInfo.cwd : "/";
				if (ubuf == 0 || size == 0)
					return (u_int64_t)-1;
				strncpy(ubuf, cwd, (size_t)size - 1);
				ubuf[size - 1] = '\0';
				return 0;
			}

			case NATIVE_KLOG_READ:
				/* klog_read(buf, max_entries, start_seq): read the 3 args straight
				 * from x0..x2.  The generic table dispatch sizes args by the uap
				 * struct and packs its two trailing 32-bit fields (max_entries,
				 * start_seq) into one 64-bit register slot on aarch64 — so start_seq
				 * (x2) is dropped and read as 0, and logd re-reads the same entries
				 * forever (100% CPU).  Reading the registers directly sidesteps it. */
				_current->td.td_retval[0] = klog_read_wait(
				    (struct klog_entry *)(uintptr_t)args[0], (int)args[1], (u_int32_t)args[2]);
				return (u_int64_t)_current->td.td_retval[0];

			default:
				/* Fall through to the shared native table (systemCalls[]) — new
				 * native syscalls are added there, not hand-mapped here. */
				return (u_int64_t)ksyscall_dispatch(
				    &_current->td, systemCalls, totalCalls, (u_int32_t)n, (register_t *)args);
		}
	}

	switch (number)
	{
		case SYS_WRITE:
		{
			/* A pty slave (FD_TYPE_TTYV), socket, or pipe fd must dispatch through
			 * the real sys_write so the fileops decide the destination — the pty
			 * feeds the VT100 cell grid (the GUI terminal), a socket goes to lwIP,
			 * a pipe goes into its pipeInfo buffer (the taskbar app launcher relies
			 * on this).  Plain console/placeholder/no-table fds keep the bring-up
			 * UART path (sc_write), which is also what the early EL0 demos rely on. */
			struct file *f = aarch64_lookup_fd((int)args[0]);
			if (f != 0 && (f->fd_type == FD_TYPE_TTYV || f->fd_type == FD_TYPE_SOCKET ||
			               f->fd_type == FD_TYPE_PIPE || f->fd_type == FD_TYPE_PTMASTER || f->fd != 0))
			{
				/* Anything with a real underlying descriptor (a VFS/devfs file
				 * such as /dev/audio, a regular file) or a pty/socket/pipe goes
				 * through sys_write so the fileops/VFS pick the destination.  Only
				 * the bare console placeholder fds (fd==NULL, FD_TYPE_TTY) keep the
				 * bring-up UART path (sc_write) the early EL0 demos rely on. */
				struct sys_write_args ua;
				ua.fd = (int)args[0];
				ua.buf = (void *)(uintptr_t)args[1];
				ua.nbyte = (size_t)args[2];
				sys_write(&_current->td, &ua);
				return (u_int64_t)_current->td.td_retval[0];
			}
			return sc_write(args[0], args[1], args[2]);
		}

		case SYS_WRITEV:
		{
			struct iovec_lp64
			{
				u_int64_t iov_base;
				u_int64_t iov_len;
			} *iov = (struct iovec_lp64 *)(uintptr_t)args[1];
			struct file *f = aarch64_lookup_fd((int)args[0]);
			/* Match SYS_WRITE's routing: a pty/socket/pipe OR any real underlying
			 * descriptor (a VFS/devfs regular file, f->fd != 0) dispatches through
			 * sys_write so the fileops/VFS pick the destination.  Without the
			 * f->fd != 0 case, buffered stdio (musl flushes via writev) writing a
			 * regular file fell through to the bring-up UART path — so file writes
			 * vanished to the serial console (e.g. logd's /var/log/messages). */
			int is_fileop =
			    (f != 0 && (f->fd_type == FD_TYPE_TTYV || f->fd_type == FD_TYPE_SOCKET ||
			                f->fd_type == FD_TYPE_PIPE || f->fd_type == FD_TYPE_PTMASTER || f->fd != 0));
			u_int64_t total = 0;
			for (int i = 0; i < (int)args[2]; i++)
			{
				if (is_fileop)
				{
					struct sys_write_args ua;
					ua.fd = (int)args[0];
					ua.buf = (void *)(uintptr_t)iov[i].iov_base;
					ua.nbyte = (size_t)iov[i].iov_len;
					sys_write(&_current->td, &ua);
					if (_current->td.td_retval[0] > 0)
						total += (u_int64_t)_current->td.td_retval[0];
				}
				else
				{
					total += sc_write(args[0], iov[i].iov_base, iov[i].iov_len);
				}
			}
			return total;
		}

		case SYS_MMAP:
			/* addr, len, prot, flags, fd, off — file-backed when no MAP_ANON. */
			return sc_mmap(args[0], args[1], args[2], args[3], args[4], args[5]);

		case SYS_MPROTECT:
			return 0; /* mmap pages are already RW; no fine-grained enforcement yet */

		case SYS_BRK:
			return sc_brk(args[0]);

		case SYS_PRLIMIT64:
		{
			/* prlimit64(pid, resource, const rlimit64 *new, rlimit64 *old):
			 * pid is ignored (per-thread limits); read/write the 64-bit limit
			 * pair against the current thread's rlim[] (rlim_t is int64_t, so
			 * the layout matches musl's struct rlimit64). */
			int resource = (int)args[1];
			u_int64_t *newl = (u_int64_t *)(uintptr_t)args[2];
			u_int64_t *oldl = (u_int64_t *)(uintptr_t)args[3];

			if (resource < 0 || resource >= RLIM_NLIMITS)
				return (u_int64_t)-EINVAL;
			if (oldl != 0)
			{
				oldl[0] = (u_int64_t)_current->td.rlim[resource].rlim_cur;
				oldl[1] = (u_int64_t)_current->td.rlim[resource].rlim_max;
			}
			if (newl != 0)
			{
				_current->td.rlim[resource].rlim_cur = (rlim_t)newl[0];
				_current->td.rlim[resource].rlim_max = (rlim_t)newl[1];
			}
			return 0;
		}

		case SYS_FORK:
			/* args is the trapframe; the child resumes here returning 0.
			 * vfork is aliased to fork in musl (it tail-calls fork()), so it
			 * arrives here too — no separate vfork syscall path is needed. */
			return (u_int64_t)aarch64_fork(args);

		case SYS_RFORK:
			/* musl routes BOTH pthread_create and posix_spawn through __clone
			 * (clone.s, patched to emit this FreeBSD rfork slot).  args is the
			 * trapframe; x0=flags, x1=child stack, x2=TLS.  A real thread
			 * (CLONE_THREAD) SHARES the AS + fd table (rfork/RFMEM); posix_spawn's
			 * CLONE_VM|CLONE_VFORK child must instead get a private COW copy + a
			 * copied fd table on the supplied stack, so its file-actions + execve
			 * never disturb the parent — that is fork() with the child SP overridden. */
			if (args[0] & CLONE_THREAD)
				return (u_int64_t)aarch64_rfork(args);
			return (u_int64_t)aarch64_clone(args);

		case SYS_EXECVE:
			/* execve(path, argv, envp): replace the current image + restart EL0.
			 * Does not return on success; -1 (to x0) on a load failure. */
			return (u_int64_t)aarch64_exec_replace((const char *)(uintptr_t)args[0],
			                                       (char *const *)(uintptr_t)args[1],
			                                       (char *const *)(uintptr_t)args[2]);

		case SYS_WAIT4:
			/* wait4(pid, status, options, rusage): cooperative reap.  options is
			 * honoured for WNOHANG (non-blocking poll); rusage is ignored.  Note
			 * pidStatus (ubix_api) shares this SVC but passes only x0, so x2 may be
			 * garbage — aarch64_wait4 blocks by default, and a stray WNOHANG bit
			 * just makes a poll non-blocking, which pidStatus callers retry. */
			return (u_int64_t)aarch64_wait4((int)args[0], (int *)(uintptr_t)args[1], (int)args[2]);

		case SYS_OPENAT:
		{
			/* openat(dirfd, path, flags, mode): absolute paths (and AT_FDCWD with a
			 * '/' cwd) resolve the same as open — dirfd-relative paths are TODO. */
			struct sys_open_args ua;
			ua.path = (char *)(uintptr_t)args[1];
			ua.flags = (int)args[2];
			ua.mode = (int)args[3];
			sys_open(&_current->td, &ua);
			return (u_int64_t)_current->td.td_retval[0];
		}

			/* clock_gettime(232) pre-case pruned (Phase 3): the table's
			 * sys_clock_gettime now builds on the unified md_uptime time source
			 * (aarch64 = the CNTVCT counter), so it keeps the nanosecond-resolution
			 * advancing clock DOOM needs — without a per-arch pre-case. */

			/* SYS_STATX pre-case pruned (Phase 3 bisect) — falls through to the
			 * shared table (statx at 383, ARG_COUNT(sys_statx_args) = 5 words). */

			/* getdents(272) pre-case pruned (Phase 3): the table aliases 272 to
			 * sys_getdirentries (musl patches __NR_getdents to 272), so it falls
			 * through.  ARG_COUNT copies a 4th word (basep) from x3, but
			 * sys_getdirentries never dereferences basep, so the garbage is inert. */

			/* fcntl(92) pre-case pruned (Phase 3) — falls through to the table. */

		case SYS_MUNMAP:
			/* Flush any MAP_SHARED writable file mapping that starts here back to
			 * its file (ld.lld's mmap'd-output commit path).  The anonymous-mmap
			 * region is still a bump allocator with no reclaim, so frames are not
			 * freed — unmapping an anon range remains a harmless leak. */
			shmap_unmap(_current->id, (uintptr_t)args[0]);
			return 0;

		case SYS_NANOSLEEP:
		{
			/* Real timed sleep: descheduled for the requested duration via the
			 * callout-driven wait, so a polling daemon (e.g. aural) paces itself
			 * without busy-spinning.  Args are 64-bit; timespec is two longs. */
			const long *rqtp = (const long *)(uintptr_t)args[0];
			long *rmtp = (long *)(uintptr_t)args[1];

			if (rqtp != 0)
			{
				long tv_sec = rqtp[0];
				long tv_nsec = rqtp[1];
				if (tv_sec >= 0 && tv_nsec >= 0 && tv_nsec < 1000000000L)
				{
					/* 100 Hz tick = 10 ms; round the request up to whole ticks. */
					u_int32_t ticks =
					    (u_int32_t)(tv_sec * 100) + (u_int32_t)((tv_nsec + 9999999L) / 10000000L);
					if (ticks == 0)
						ticks = 1;
					sched_wait_event_timeout(&g_nanosleep_chan, nanosleep_never, 0, ticks);
				}
			}
			if (rmtp != 0)
			{
				rmtp[0] = 0;
				rmtp[1] = 0;
			}
			return 0;
		}

			/* uname(164)/sched_yield(331) pre-cases pruned (Phase 3): the table's
			 * sys_uname now reports the arch machine name (gen_calls.c #if), and
			 * sys_sched_yield just yields + sets td_retval[0]=0 — both identical to
			 * the table.  ioctl(54) likewise falls through to the real sys_ioctl. */

			/* getuid/geteuid/getgid/getegid/getpid are no longer intercepted: the
			 * shared table handlers (sys_getUID/getGID in kern/access.c,
			 * sys_geteuid/getegid/getpid in posix/gen_calls.c) now all set
			 * td_retval[0], so the generic dispatch returns the right value on
			 * both arches.  See Phase 3 in docs/design/console-and-arch-convergence-plan.md. */
		case SYS_SETUID:
			if (_current != 0)
			{
				_current->uid = (u_int32_t)args[0];
				_current->euid = (u_int16_t)args[0];
			}
			return 0;
		case SYS_SETGID:
			if (_current != 0)
			{
				_current->gid = (u_int32_t)args[0];
				_current->egid = (u_int16_t)args[0];
			}
			return 0;

			/* set_tid_address(258) pre-case pruned (Phase 3): the table's
			 * sys_set_tid_address returns _current->id, identical. */

			/* SYS_RT_SIGPROCMASK (340) is no longer intercepted — it falls through to
			 * the real sys_sigprocmask in the POSIX table now that EL0 signal
			 * delivery is wired, so blocked masks are actually honoured. */

			/* open(5)/read(3)/close(6) pre-cases pruned (Phase 3): plain
			 * build-uap + call-sysent-handler + return td_retval[0] — exactly
			 * what the shared table does.  They fall through to it. */

		case SYS_EXIT:
			do_exit(args[0]);
			return 0; /* unreachable */

		default:
			/* Fall through to the shared POSIX table (systemCalls_posix[]) — the
			 * arch-special calls above are intercepted; everything else dispatches
			 * through the generic table, so new syscalls are added once (in the
			 * table) rather than hand-mapped per architecture. */
			return (u_int64_t)ksyscall_dispatch(
			    &_current->td, systemCalls_posix, totalCalls_posix, (u_int32_t)number, (register_t *)args);
	}
}
