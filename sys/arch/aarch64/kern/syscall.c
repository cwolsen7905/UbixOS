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
#include <sys/sysproto_posix.h> /* sys_open/read/close/lseek + uap structs */
#include <sys/descrip.h>        /* getfd, struct file */
#include <mpi/mpi.h>            /* MPI mailboxes (native syscalls 50-53) */
#include <string.h>             /* strncpy (getcwd) */

#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_FORK 2
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_WAIT4 7         /* FreeBSD ABI (the number musl actually emits) */
#define SYS_EXECVE 59       /* FreeBSD ABI (the number musl actually emits) */
#define SYS_IOCTL 54        /* FreeBSD ABI; musl isatty() probes the tty with it */
#define SYS_WRITEV 121      /* FreeBSD ABI; the dynamic linker uses it for diagnostics */
#define SYS_SCHED_YIELD 331 /* UbixOS ABI (musl's sched_yield maps here) */
#define SYS_BRK 17
#define SYS_GETPID 20
#define SYS_MPROTECT 74
#define SYS_FSTAT 189
#define SYS_MMAP 477
#define SYS_LSEEK 478
#define SYS_SET_TID_ADDRESS 258
#define SYS_RT_SIGPROCMASK 340 /* musl blocks/restores signals around fork() */

/* Per-process anonymous-mmap region base (block 8 — clear of code/stack). */
#define MMAP_BASE 0x200000000UL
/* Per-process brk heap base (block 7 — clear of code/stack/mmap). */
#define BRK_BASE 0x1C0000000UL

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
 * mmap(addr, len, prot, flags, fd, off): anonymous private mapping.  Arch glue
 * around the generic policy (vmm_uregion_mmap_anon): owns only the VA-layout
 * choice (where this address space's mmap region starts) and the TTBR0 root;
 * the page-counting / allocate-zero-map work is architecture-neutral.  prot/fd/
 * off are ignored — pages are backed RW, which covers musl's malloc.
 *
 * @return the mapped base VA, or (void*)-1 on failure.
 */
static u_int64_t sc_mmap(u_int64_t addr, u_int64_t len, u_int64_t flags)
{
	u_int64_t *l1;
	uintptr_t next, va;

	if (_current == 0 || _current->md.md_ttbr0 == 0)
		return (u_int64_t)-1;
	if (_current->md.md_mmap_next == 0)
		_current->md.md_mmap_next = MMAP_BASE;

	l1 = (u_int64_t *)(uintptr_t)_current->md.md_ttbr0;
	next = (uintptr_t)_current->md.md_mmap_next;

	/* MAP_FIXED is FreeBSD flag 0x10: the caller (e.g. mallocng's guard page)
	 * requires the mapping to land exactly at addr. */
	va = vmm_uregion_mmap_anon(l1, &next, (size_t)len, (flags & 0x10) && addr != 0, (uintptr_t)addr);
	_current->md.md_mmap_next = (u_int64_t)next;
	return (va != 0) ? (u_int64_t)va : (u_int64_t)-1;
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
				kprintf("[kernel] unimplemented native EL0 syscall #%lu\n", n);
				return (u_int64_t)-1;
		}
	}

	switch (number)
	{
		case SYS_WRITE:
			/* The hand-rolled UART write already emits to the console for any fd;
			 * no fd-table lookup needed (and the early EL0 demos have no fd table
			 * yet).  The console fileop write path is reached via the file layer
			 * for real file/pipe fds. */
			return sc_write(args[0], args[1], args[2]);

		case SYS_WRITEV:
		{
			/* writev(fd, iov, iovcnt): emit each iovec to the console.  The musl
			 * dynamic linker uses it for its diagnostics (and stdio later). */
			struct iovec_lp64
			{
				u_int64_t iov_base;
				u_int64_t iov_len;
			} *iov = (struct iovec_lp64 *)(uintptr_t)args[1];
			u_int64_t total = 0;
			for (int i = 0; i < (int)args[2]; i++)
				total += sc_write(args[0], iov[i].iov_base, iov[i].iov_len);
			return total;
		}

		case SYS_MMAP:
			return sc_mmap(args[0], args[1], args[3]); /* addr, len, flags (prot/fd/off ignored) */

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
			return (u_int64_t)aarch64_exec_replace((const char *)(uintptr_t)args[0]);

		case SYS_WAIT4:
			/* wait4(pid, status, options, rusage): cooperative reap.  options/
			 * rusage are ignored (no WNOHANG/WUNTRACED yet). */
			return (u_int64_t)aarch64_wait4((int)args[0], (int *)(uintptr_t)args[1]);

		case SYS_IOCTL:
			/* Pretend success so musl isatty()/tcgetattr() treat the console as a
			 * tty.  No real termios yet. */
			return 0;

		case SYS_SCHED_YIELD:
			sched_yield();
			return 0;

		case SYS_GETPID:
			return (_current != 0) ? (u_int64_t)_current->id : 0;

		case SYS_SET_TID_ADDRESS:
			/* musl registers a clear-on-exit TID address at startup; the return
			 * value is the caller's thread id. */
			return (_current != 0) ? (u_int64_t)_current->id : 1;

		case SYS_RT_SIGPROCMASK:
			/* No-op: EL0 signal delivery is not wired yet, so blocking/restoring
			 * the signal mask around fork() has no effect.  Returning 0 keeps
			 * musl's fork() happy (and quiet). */
			return 0;

		case SYS_OPEN:
		{
			/* FreeBSD sysent ABI: build the uap from x0..x2, run on the current
			 * thread, hand x0 the td_retval (bytes / fd, or -errno on error). */
			struct sys_open_args ua;
			ua.path = (char *)(uintptr_t)args[0];
			ua.flags = (int)args[1];
			ua.mode = (int)args[2];
			sys_open(&_current->td, &ua);
			return (u_int64_t)_current->td.td_retval[0];
		}

		case SYS_READ:
		{
			struct sys_read_args ua;
			ua.fd = (int)args[0];
			ua.buf = (void *)(uintptr_t)args[1];
			ua.nbyte = (size_t)args[2];
			sys_read(&_current->td, &ua);
			return (u_int64_t)_current->td.td_retval[0];
		}

		case SYS_CLOSE:
		{
			struct sys_close_args ua;
			ua.fd = (int)args[0];
			sys_close(&_current->td, &ua);
			return (u_int64_t)_current->td.td_retval[0];
		}

		case SYS_EXIT:
			do_exit(args[0]);
			return 0; /* unreachable */

		default:
			kprintf("[kernel] unknown EL0 syscall #%lu\n", number);
			return (u_int64_t)-1;
	}
}
