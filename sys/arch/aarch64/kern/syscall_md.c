/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 machine-dependent + not-yet-ported syscall handler provisions.
 *
 * Both shared syscall tables (systemCalls[] native, systemCalls_posix[] POSIX)
 * reference handlers across the whole kernel.  This file defines the ones that
 * are not (yet) linkable on aarch64 so the tables link and the generic dispatch
 * engine (sys/kern/syscall_dispatch.c) can drive them:
 *
 *   - benign behaviours (sched_yield, nanosleep, munmap, getcwd, ...) get a real
 *     minimal implementation;
 *   - subsystems not ported to aarch64 yet (sockets, signals, the pty/display/
 *     input stack) return -ENOSYS so callers fail cleanly instead of the kernel
 *     failing to link.
 *
 * The arch-special calls that need the trapframe or the arch VA/loader (fork,
 * execve, mmap, brk, exit, write) are intercepted by the arch trampoline
 * (syscall.c) before table dispatch, so their entries here are unreached stubs.
 */

#include <sys/types.h>
#include <sys/sysproto.h>
#include <sys/sysproto_posix.h>
#include <fs/ubixfs/ubfs_vfs.h> /* ubfs_vfs_query — native pool-query syscall 67 */
#include <dev/disk.h>           /* disk_query — block-device enumeration syscall 68 */
#include <ubixos/sched.h>
#include <ubixos/errno.h>
#include <ubixos/time.h>   /* struct timeval/timezone (kernel gettimeofday) */
#include <ubixos/vitals.h> /* systemVitals->sysTicks (time source) */
#include <string.h>

#define ENOSYS_STUB(td)                                                                                                \
	do                                                                                                             \
	{                                                                                                              \
		(td)->td_retval[0] = -ENOSYS;                                                                          \
		return (ENOSYS);                                                                                       \
	} while (0)

#define OK_STUB(td)                                                                                                    \
	do                                                                                                             \
	{                                                                                                              \
		(td)->td_retval[0] = 0;                                                                                \
		return (0);                                                                                            \
	} while (0)

/* ---- benign / real minimal implementations ---------------------------- */

int sys_sched_yield(struct thread *td, void *uap)
{
	(void)uap;
	sched_yield();
	OK_STUB(td);
}

int sys_nanosleep(struct thread *td, void *uap)
{
	(void)uap; /* no timed sleep yet — yield once */
	sched_yield();
	OK_STUB(td);
}

int sys_munmap(struct thread *td, struct sys_munmap_args *uap)
{
	(void)uap; /* anon-mmap region is a bump allocator; reclaim is TODO */
	OK_STUB(td);
}

int sys_thread_exit_unmap(struct thread *td, struct sys_munmap_args *uap)
{
	(void)uap;
	OK_STUB(td);
}

int sys_madvise(struct thread *td, struct sys_madvise_args *uap)
{
	(void)uap;
	OK_STUB(td);
}

int sys_msync(struct thread *td, struct sys_msync_args *uap)
{
	(void)uap;
	OK_STUB(td);
}

static void copy_cwd(struct thread *td, char *buf, u_int32_t size)
{
	const char *cwd = (_current != 0 && _current->oInfo.cwd[0] != '\0') ? _current->oInfo.cwd : "/";
	if (buf == 0 || size == 0)
	{
		td->td_retval[0] = -EINVAL;
		return;
	}
	strncpy(buf, cwd, (size_t)size - 1);
	buf[size - 1] = '\0';
	td->td_retval[0] = 0;
}

int sys_getvfscwd(struct thread *td, struct sys_getvfscwd_args *uap)
{
	copy_cwd(td, uap->buf, uap->size);
	return (0);
}

/**
 * Native syscall 67 — fill the userland buffer with mounted UbixFS pool records
 * (the in-OS `ubpool`/`ubfs` commands).  Mirrors the i386 handler in
 * sys/kern/syscall.c; the driver enumerates the VFS mount list.
 *
 * @return 0; the pool count is returned in td_retval[0].
 */
int sys_ubfs_query(struct thread *td, struct sys_ubfs_query_args *uap)
{
	td->td_retval[0] = ubfs_vfs_query(uap->buf, (int)uap->max);
	return (0);
}

int sys_disk_query(struct thread *td, struct sys_disk_query_args *uap)
{
	td->td_retval[0] = disk_query(uap->buf, (int)uap->max);
	return (0);
}

/**
 * POSIX __getcwd(2) (FreeBSD slot 326).  Mirrors the shared sys_getcwd in
 * sys/kern/syscall.c: on success returns the byte length of the path including
 * its NUL terminator, not 0.  musl's getcwd() treats a 0 return as ENOENT, so
 * copy_cwd()'s 0-on-success convention (fine for the native getvfscwd) would
 * make every getcwd() fail — which is why tcsh's dinit() printed "No such file
 * or directory" before falling back to "/".
 */
int sys_getcwd(struct thread *td, struct sys_getcwd_args *uap)
{
	const char *cwd = (_current != 0 && _current->oInfo.cwd[0] != '\0') ? _current->oInfo.cwd : "/";

	copy_cwd(td, (char *)uap->buf, uap->size);
	if (td->td_retval[0] == 0)
		td->td_retval[0] = (int)(strlen(cwd) + 1);
	return (0);
}

/*
 * gettimeofday is now the shared sys/posix/time.c implementation (built on the
 * single md_uptime time source — aarch64's is the CNTVCT counter in timer.c).
 * The old aarch64-local 100 Hz gettimeofday lived here only because time.c's
 * x86 CMOS time_init was unbuildable on aarch64; that code is now #if-guarded,
 * so time.c links on both arches and there is one gettimeofday.
 */

void machine_set_tls(struct thread *td, uintptr_t base)
{
	(void)td;
	__asm__ volatile("msr tpidr_el0, %0" : : "r"(base));
}

int vmm_set_page_attributes(uintptr_t va, u_int32_t attr)
{
	(void)va;
	(void)attr;
	return (0);
}

int sysGetFreePage(struct thread *td, u_int32_t *count)
{
	(void)count;
	OK_STUB(td);
}

/* signal_post_kill + the sys_sig* calls below are now the real, arch-neutral
 * implementations in sys/posix/signal.c (compiled for aarch64); the aarch64 EL0
 * signal-frame delivery + sigreturn live there too, behind __aarch64__. */

/* ---- arch-special entries (intercepted by syscall.c; unreached) -------- */

int sys_mmap(struct thread *td, struct sys_mmap_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_mmap2(struct thread *td, struct sys_mmap_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int obreak(struct thread *td, struct obreak_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_execve(struct thread *td, struct sys_execve_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_fork(struct thread *td, struct sys_fork_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_rfork(struct thread *td, struct sys_rfork_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}

/* ---- not-yet-ported subsystems: fail cleanly with -ENOSYS ------------- */

/* pipe (42) + pipe2 (542) are real now: sys/posix/pipe.c + kern_pipe.c are
 * linked into AARCH64_GENERIC_SRCS, and the pipe read/write/close data path in
 * vfs_calls.c was already linked.  Their table entries call the real handlers. */
int sys_sysctl(struct thread *td, struct sys_sysctl_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_sysinfo(struct thread *td, struct sys_sysinfo_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_klog_read(struct thread *td, struct sys_klog_read_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}

/* sockets: the real implementations now link from sys/net/net/sys_arch.c (lwIP),
 * so the stubs that used to live here are gone. */

/* signals: sys_sigaction / sys_sigprocmask / sys_sigreturn / sys_sigsuspend are
 * the real arch-neutral + aarch64 implementations in sys/posix/signal.c. */

/* pty syscalls (ptyalloc/free/inject/snap/resize): real in dev/display.c, backed
 * by the pty pool in sys/posix/tty.c (now compiled for aarch64). */
/**
 * settty (slot 48) — claim the system console as the controlling terminal.
 *
 * On i386 this claims the serial TTY (the slot-based VGA/serial console model).
 * aarch64 has no slot-based serial claim: the kernel already wires fd 0/1/2 to
 * the PL011 + fbcon console fileops for every process (aarch64_console_init), and
 * fork inherits them — so PID 1's start_console() just needs this to succeed so
 * it can exec the console primary (views/login).  No-op success.
 */
int sys_settty(struct thread *td, struct sys_settty_args *uap)
{
	(void)uap;
	td->td_retval[0] = 0;
	return (0);
}
/* sys_getkbd + sys_getmouse: real implementations in sys/arch/aarch64/dev/input.c. */
/* sys_mapfb + sys_fbpresent + sys_shareregion: real in sys/arch/aarch64/dev/display.c. */
int sys_vesa_modes(struct thread *td, struct sys_vesa_modes_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_net_configure(struct thread *td, struct sys_net_configure_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
