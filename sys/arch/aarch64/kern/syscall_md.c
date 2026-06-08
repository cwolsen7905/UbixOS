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
#include <ubixos/sched.h>
#include <ubixos/errno.h>
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

int sys_getcwd(struct thread *td, struct sys_getcwd_args *uap)
{
	copy_cwd(td, (char *)uap->buf, uap->size);
	return (0);
}

int gettimeofday(struct thread *td, struct gettimeofday_args *uap)
{
	u_int32_t *tv = (u_int32_t *)uap->tp; /* struct timeval {sec,usec} — no wall clock yet */
	if (tv != 0)
	{
		tv[0] = 0;
		tv[1] = 0;
	}
	OK_STUB(td);
}

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

void signal_post_kill(int sender_pid, int target_pid, int sig)
{
	(void)sender_pid;
	(void)target_pid;
	(void)sig;
}

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

int sys_pipe2(struct thread *td, struct sys_pipe2_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int pipe(struct thread *td, struct pipe_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
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

/* sockets */
int sys_socket(struct thread *td, struct sys_socket_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_bind(struct thread *td, struct sys_bind_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_connect(struct thread *td, struct sys_connect_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_listen(struct thread *td, struct sys_listen_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_accept(struct thread *td, struct sys_accept_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_recvfrom(struct thread *td, struct sys_recvfrom_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_recvmsg(struct thread *td, struct sys_recvmsg_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_sendmsg(struct thread *td, struct sys_sendmsg_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_sendto(struct thread *td, struct sys_sendto_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_setsockopt(struct thread *td, struct sys_setsockopt_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}

/* signals */
int sys_sigaction(struct thread *td, struct sys_sigaction_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_sigprocmask(struct thread *td, struct sys_sigprocmask_args *uap)
{
	(void)uap;
	OK_STUB(td); /* no signal delivery yet; pretend the mask op succeeded */
}
int sys_sigreturn(struct thread *td, struct sys_sigreturn_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_sigsuspend(struct thread *td, struct sys_sigsuspend_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}

/* pty / tty / display / input stack */
int sys_ptyalloc(struct thread *td, struct sys_ptyalloc_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_ptyfree(struct thread *td, struct sys_ptyfree_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_ptyinject(struct thread *td, struct sys_ptyinject_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_ptyresize(struct thread *td, struct sys_ptyresize_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_ptysnap(struct thread *td, struct sys_ptysnap_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_settty(struct thread *td, struct sys_settty_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_getkbd(struct thread *td, struct sys_getkbd_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_getmouse(struct thread *td, struct sys_getmouse_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_mapfb(struct thread *td, struct sys_mapfb_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
int sys_shareregion(struct thread *td, struct sys_shareregion_args *uap)
{
	(void)uap;
	ENOSYS_STUB(td);
}
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
