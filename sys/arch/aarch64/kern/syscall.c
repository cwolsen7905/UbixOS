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
#include <ubixos/sched.h>       /* _current, sched_yield */
#include <ubixos/endtask.h>     /* endTask */
#include <vmm/vmm.h>            /* address-space helpers */
#include <vmm/uregion.h>        /* vmm_uregion_mmap_anon, vmm_uregion_brk */
#include <aarch64/vmm_layout.h> /* MMAP_BASE / BRK_BASE (user-VA layout) */
#include <sys/sysproto_posix.h> /* sys_open/read/close/lseek + uap structs */
#include <sys/descrip.h>        /* getfd, struct file */
#include <mpi/mpi.h>            /* MPI mailboxes (native syscalls 50-53) */
#include <string.h>             /* strncpy (getcwd) */
#include <fs/vfs/stat.h>        /* struct statx (sys_statx) */
#include <ubixos/syscalls.h>    /* systemCalls[] / systemCalls_posix[] tables */
#include <sys/elf_load.h>       /* md_sync_icache (file-backed/exec mmap) */
#include <fs/vfs/file.h>        /* fread (file-backed mmap) */

/* Generic table-driven dispatch engine (sys/kern/syscall_dispatch.c). */
register_t ksyscall_dispatch(
    struct thread *td, struct syscall_entry *tbl, int count, u_int32_t number, register_t *args);

#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_FORK 2
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
#define NATIVE_GETCWD 41 /* ubix_getcwd(buf, size) */

/**
 * Terminate the current task (shared by exit / exit_group): a scheduled user
 * task is reaped + rescheduled; a bring-up enter_el0 demo longjmps back.
 */
static void do_exit(u_int64_t code)
{
	kprintf("[kernel] EL0 process exit(%lu)\n", code);
	if (_current != 0 && _current->md.md_usp != 0)
	{
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
		uart_putc(p[i]);
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

	if (_current == 0 || _current->md.md_ttbr0 == 0)
		return (u_int64_t)-1;
	if (_current->md.md_mmap_next == 0)
		_current->md.md_mmap_next = MMAP_BASE;

	l1 = (u_int64_t *)(uintptr_t)_current->md.md_ttbr0;
	next = (uintptr_t)_current->md.md_mmap_next;

	/* MAP_FIXED is FreeBSD flag 0x10: the caller (e.g. mallocng's guard page,
	 * or the loader placing a segment) requires the mapping to land at addr. */
	va = vmm_uregion_mmap_anon(l1, &next, (size_t)len, (flags & 0x10) && addr != 0, (uintptr_t)addr);
	_current->md.md_mmap_next = (u_int64_t)next;
	if (va == 0)
		return (u_int64_t)-1;

	/* File-backed (MAP_ANON is 0x20): read file content into the reserved pages.
	 * Write through each page's physical alias (PAN-safe), page by page since the
	 * frames are not physically contiguous. */
	if ((flags & 0x20) == 0 && (int)fd >= 0)
	{
		struct file *fp = 0;
		if (getfd(&_current->td, &fp, (int)fd) == 0 && fp != 0 && fp->fd != 0)
		{
			u_int64_t done;

			/* Syscall 477 is mmap2: the offset arrives in PAGE_SIZE units
			 * (musl passes off/UNIT), so scale back to a byte offset. */
			fp->fd->offset = (off_t)(off * PAGE_SIZE);
			for (done = 0; done < len; done += PAGE_SIZE)
			{
				u_int64_t phys = pmap_extract(l1, va + done);
				size_t chunk = (len - done < PAGE_SIZE) ? (size_t)(len - done) : PAGE_SIZE;
				size_t got;
				if (phys == 0)
					break; /* region not fully mapped (shouldn't happen) */
				got = fread((void *)(uintptr_t)phys, 1, chunk, fp->fd);
				if (got == 0)
					break; /* EOF: remaining pages stay zero (bss tail) */
			}
		}
	}

	/* PROT_EXEC (0x4): the dynamic linker maps code segments executable. */
	if ((prot & 0x4) != 0)
	{
		u_int64_t done;
		for (done = 0; done < len; done += PAGE_SIZE)
		{
			u_int64_t phys = pmap_extract(l1, va + done);
			if (phys == 0)
				continue;
			pmap_map_user_page(l1, va + done, phys, 1 /* executable */);
			md_sync_icache((uintptr_t)phys, PAGE_SIZE);
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

	if (_current == 0 || _current->md.md_ttbr0 == 0)
		return BRK_BASE;
	if (_current->md.md_brk == 0)
		_current->md.md_brk = BRK_BASE;

	l1 = (u_int64_t *)(uintptr_t)_current->md.md_ttbr0;
	return (u_int64_t)vmm_uregion_brk(l1, (uintptr_t *)&_current->md.md_brk, (uintptr_t)newbrk);
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
			               f->fd_type == FD_TYPE_PIPE || f->fd != 0))
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
			int is_fileop = (f != 0 && (f->fd_type == FD_TYPE_TTYV || f->fd_type == FD_TYPE_SOCKET ||
			                            f->fd_type == FD_TYPE_PIPE));
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

		case SYS_FORK:
			/* args is the trapframe; the child resumes here returning 0. */
			return (u_int64_t)aarch64_fork(args);

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
			/* No-op: the anonymous-mmap region is a bump allocator with no reclaim
			 * yet, so unmapping just leaks (harmless for short-lived programs). */
			return 0;

		case SYS_NANOSLEEP:
			/* No timed sleep yet — yield once and report completion. */
			sched_yield();
			return 0;

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
