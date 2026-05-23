/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/gen_calls.h>
#include <sys/resource.h>
#include <sys/thread.h>
#include <sys/gdt.h>
#include <ubixos/sched.h>
#include <ubixos/endtask.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <string.h>
#include <assert.h>
#include <sys/descrip.h>
#include <sys/video.h>
#include <sys/signal.h>
#include <ubixos/signal.h>
#include <ubixos/version.h>
#include <sys/sysproto_posix.h>
#include <sys/sysproto.h>
#include <ubixos/errno.h>
#include <ubixos/time.h>
#include <vmm/vmm.h>

/* Exit Syscall */
int sys_exit(struct thread *td, struct sys_exit_args *args)
{
	endTask(_current->id);
	return (0x0);
}

/* return the process id */
int getpid(struct thread *td, struct getpid_args *uap)
{
#ifdef DEBUG
	kprintf("[%s:%i]", __FILE__, __LINE__);
#endif
	td->td_retval[0] = _current->id;
	return (0);
}

/* return the process user id */
int getuid(struct thread *td, struct getuid_args *uap)
{
#ifdef DEBUG
	kprintf("[%s:%i]", __FILE__, __LINE__);
#endif
	td->td_retval[0] = _current->uid;
	return (0);
}

/* return the process group id */
int getgid(struct thread *td, struct getgid_args *uap)
{
#ifdef DEBUG
	kprintf("[%s:%i]", __FILE__, __LINE__);
#endif
	td->td_retval[0] = _current->gid;
	return (0);
}

int sys_issetugid(register struct thread *td, struct sys_issetugid_args *uap)
{
	td->td_retval[0] = 0;
	return (0);
}

int readlink(struct thread *td, struct readlink_args *uap)
{
#ifdef DEBUG
	kprintf("[%s:%i]", __FILE__, __LINE__);
#endif
	kprintf("readlink: [%s:%i]\n", uap->path, uap->count);
	td->td_retval[0] = -1;
	td->td_retval[1] = 0x0;
	return (0x0);
}

int gettimeofday_new(struct thread *td, struct gettimeofday_args *uap)
{
#ifdef DEBUG
	kprintf("[%s:%i]", __FILE__, __LINE__);
#endif
	return (0x0);
}

int read(struct thread *td, struct read_args *uap)
{
	int error = 0x0;
	size_t count = 0x0;
	struct file *fd = 0x0;

#ifdef DEBUG
	kprintf("[%s:%i]", __FILE__, __LINE__);
#endif

	error = getfd(td, &fd, uap->fd);

	if (error)
		return (error);

	count = fread(uap->buf, uap->nbyte, 0x1, fd->fd);
	kprintf("count: %i\n", count);
	td->td_retval[0] = count;

	return (error);
}

/*!
 * \brief place holder for now functionality to be added later
 */
int setitimer(struct thread *td, struct setitimer_args *uap)
{
	int error = 0x0;

	return (error);
}

int access(struct thread *td, struct access_args *uap)
{
	int error = 0x0;
	kprintf("name: [%s]\n", uap->path);
	return (error);
}

int mprotect(struct thread *td, struct mprotect_args *uap)
{
	int error = 0x0;
	return (error);
}

int sys_kill(struct thread *td, struct sys_kill_args *uap)
{
	if (uap->signum == 0) {
		/* Signal 0: existence check only. */
		kTask_t *target = schedFindTask(uap->pid);
		td->td_retval[0] = (target != NULL) ? 0 : -1;
		return (td->td_retval[0]);
	}

	signal_post_kill(_current->id, uap->pid, uap->signum);
	td->td_retval[0] = 0;
	return (0);
}

int sys_gettid(struct thread *td, void *uap)
{
	td->td_retval[0] = _current->id;
	return (0);
}

int sys_tkill(struct thread *td, struct sys_tkill_args *uap)
{
	struct sys_kill_args kargs;

	kargs.pid    = uap->tid;
	kargs.signum = uap->signum;
	return (sys_kill(td, &kargs));
}

int sys_clock_gettime(struct thread *td, struct sys_clock_gettime_args *uap)
{
	struct timeval tv;
	struct timezone tz;

	if (uap->tp == 0x0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	gettimeofday(&tv, &tz);

	/*
	 * musl libc on i386 uses 64-bit time_t, so its struct timespec is:
	 *   { int64_t tv_sec; int32_t tv_nsec; int32_t _pad; }  (16 bytes, LE)
	 * Write the four 32-bit words in order: sec_lo, sec_hi, nsec, pad.
	 */
	int32_t *p = (int32_t *)uap->tp;
	p[0] = (int32_t)tv.tv_sec;          /* tv_sec low  32 bits */
	p[1] = 0;                            /* tv_sec high 32 bits */
	p[2] = (int32_t)(tv.tv_usec * 1000);/* tv_nsec              */
	p[3] = 0;                            /* padding              */

	td->td_retval[0] = 0;
	return (0);
}

/* futex stub — always succeeds for single-threaded musl */
int sys_futex(struct thread *td, struct sys_futex_args *uap)
{
	td->td_retval[0] = 0;
	return (0);
}

/*
 * set_thread_area — TLS setup syscall (slot 351).
 *
 * UbixOS musl calls __syscall(SYS_set_thread_area, pthread_ptr) where
 * pthread_ptr IS the TLS base (the pthread struct address).  This is NOT
 * the Linux struct user_desc interface — musl passes the raw TLS pointer
 * as a single argument.
 *
 * We write LDT[1] with base = pthread_ptr, then set tf_gs = 0xF so the
 * iret back to user mode installs %gs = LDT[1] selector.  After this,
 * %gs:0 == pthread->self, matching what musl's __get_tp() expects.
 */
int sys_set_thread_area(struct thread *td, struct sys_set_thread_area_args *uap)
{
	struct gdtDescriptor *tlsDesc = 0x0;

	/* uap->u_info IS the TLS base pointer (pthread struct address). */
	uint32_t base_addr = (uint32_t)uap->u_info;

	if (base_addr == 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	/* Write LDT[1] — second entry in the per-process LDT page */
	tlsDesc = (struct gdtDescriptor *)(VMM_USER_LDT + sizeof(struct gdtDescriptor));

	tlsDesc->limitLow = 0xFFFF;
	tlsDesc->limitHigh = 0xF;
	tlsDesc->baseLow = (base_addr & 0xFFFF);
	tlsDesc->baseMed = ((base_addr >> 16) & 0xFF);
	tlsDesc->access = ((dData + dWrite + dBig + dBiglim + dDpl3) + dPresent) >> 8;
	tlsDesc->granularity = ((dData + dWrite + dBig + dBiglim + dDpl3) & 0xFF) >> 4;
	tlsDesc->baseHigh = (base_addr >> 24);

	/* Reload the LDT register so the updated LDT[1] descriptor is live. */
	asm volatile("pushl %eax\n\t"
	             "movw  $0x18,%ax\n\t"
	             "lldt  %ax\n\t"
	             "popl  %eax\n\t");

	/*
	 * Propagate the new %gs selector (0xF = LDT[1], TI=1, RPL=3) back to
	 * user space via the saved trapframe register.  sys_call_posix.S pushes
	 * and pops all segment registers around the syscall, so writing
	 * tf_gs here is the only way to make %gs=0xF survive the iret.
	 * Setting %gs directly in kernel asm would be discarded by the pop.
	 */
	td->frame->tf_gs = 0xF;

	td->td_retval[0] = 0;
	return (0);
}

/* writev — scatter-gather write: loop over iov[] calling sys_write for each */
int sys_writev(struct thread *td, struct sys_writev_args *uap)
{
	struct sys_write_args wa;
	int i;
	ssize_t total = 0;

	wa.fd = uap->fd;
	for (i = 0; i < uap->iovcnt; i++)
	{
		wa.buf = uap->iov[i].iov_base;
		wa.nbyte = uap->iov[i].iov_len;
		if (wa.nbyte == 0)
			continue;
		sys_write(td, &wa);
		if (td->td_retval[0] < 0)
		{
			td->td_retval[0] = total > 0 ? total : td->td_retval[0];
			return (-1);
		}
		total += td->td_retval[0];
	}
	td->td_retval[0] = total;
	return (0);
}

int sys_readv(struct thread *td, struct sys_readv_args *uap)
{
	struct sys_read_args ra;
	int i;
	ssize_t total = 0;

	ra.fd = uap->fd;
	for (i = 0; i < uap->iovcnt; i++)
	{
		ra.buf = uap->iov[i].iov_base;
		ra.nbyte = uap->iov[i].iov_len;
		if (ra.nbyte == 0)
			continue;
		sys_read(td, &ra);
		if (td->td_retval[0] < 0)
		{
			td->td_retval[0] = total > 0 ? total : td->td_retval[0];
			return (-1);
		}
		total += td->td_retval[0];
		if ((size_t)td->td_retval[0] < ra.nbyte)
			break;
	}
	td->td_retval[0] = total;
	return (0);
}

/* exit_group — exit all threads in the process (alias to sys_exit) */
int sys_exit_group(struct thread *td, struct sys_exit_group_args *uap)
{
	endTask(_current->id);
	return (0);
}

int sys_invalid(struct thread *td, void *args)
{
	kprintf("ISC[%i:%i]", td->frame->tf_eax, _current->id);
	td->td_retval[0] = -1;
	return (0);
}

int sys_pidStatus(struct thread *td, struct sys_pidStatus_args *args)
{
	kTask_t *task = schedFindTask(args->pid);

	if (task != NULL && task->state != DEAD)
		td->td_retval[0] = 1;
	else
		td->td_retval[0] = 0;
	return (0);
}

int sys_wait4(struct thread *td, struct sys_wait4_args *args)
{
	int error = 0;

	if (args->pid == -1)
	{
		if (_current->children <= 0)
		{
			td->td_retval[0] = ECHILD;
			return (-1);
		}

		int children = _current->children;

		sched_setStatus(_current->id, WAIT);
		while (_current->children == children)
		{
			sched_yield();
		}

		td->td_retval[0] = _current->last_exit;
		td->td_retval[1] = 0x8;
	}
	else
	{
		kTask_t *tmpTask = NULL;
		int retries;

		/*
		 * Retry briefly: a just-forked child may not appear in the scheduler
		 * until after the parent's next yield (fork-to-schedule race).
		 */
		for (retries = 0; retries < 100; retries++)
		{
			tmpTask = schedFindTask(args->pid);
			if (tmpTask != NULL)
				break;
			sched_yield();
		}

		if (tmpTask != NULL)
		{
			sched_setStatus(_current->id, WAIT);
			while (tmpTask != NULL)
			{
				sched_yield();
				tmpTask = schedFindTask(args->pid);
			}
			td->td_retval[0] = args->pid;
		}
		else
		{
			td->td_retval[0] = -1;
			error = -1;
		}
	}
	return (error);
}

int sys_sysarch(struct thread *td, struct sys_sysarch_args *args)
{

	void **segbase = 0x0;
	uint32_t base_addr = 0x0;

	if (args->op == 10)
	{
		kprintf("SETGSBASE: 0x%X:0x%X", args->parms, args->parms[0]);

		segbase = (void **)args->parms;

		kprintf("SGS: [0x%X:0x%X]", segbase[0], segbase[1]);
		base_addr = (uint32_t)segbase[0];

		struct gdtDescriptor *tmpDesc = 0x0;

		tmpDesc = (struct gdtDescriptor *)(VMM_USER_LDT + sizeof(struct gdtDescriptor)); // taskLDT[1];

		tmpDesc->limitLow = 0xFFFF; //(0xFFFFF & 0xFFFF);
		tmpDesc->limitHigh = 0xF;   //(0xFFFFF >> 16);
		tmpDesc->baseLow = (base_addr & 0xFFFF);
		tmpDesc->baseMed = ((base_addr >> 16) & 0xFF);
		tmpDesc->access = ((dData + dWrite + dBig + dBiglim + dDpl3) + dPresent) >> 8;
		tmpDesc->granularity = ((dData + dWrite + dBig + dBiglim + dDpl3) & 0xFF) >> 4;
		tmpDesc->baseHigh = base_addr >> 24;

		asm("push %eax\n"
		    "mov $0x18,%ax\n"
		    "lldt %ax\n" /* "lgdtl (loadGDT)\n" */
		    "mov $0xF,%eax\n"
		    "mov %eax,%gs\n"
		    "pop %eax\n");

		td->td_retval[0] = 0;
	}
	else
	{
		kprintf("sysarch(%i,NULL)", args->op);
		td->td_retval[0] = -1;
	}
	return (0);
}

int sys_getpid(struct thread *td, struct sys_getpid_args *args)
{
	td->td_retval[0] = _current->id;
	return (0);
}
int sys_geteuid(struct thread *td, struct sys_geteuid_args *args)
{
	td->td_retval[0] = _current->uid;
	return (0);
}

int sys_getegid(struct thread *td, struct sys_getegid_args *args)
{
	td->td_retval[0] = _current->gid;
	return (0);
}

int sys_getppid(struct thread *td, struct sys_getppid_args *args)
{
	td->td_retval[0] = _current->ppid;
	return (0);
}

int sys_getpgrp(struct thread *td, struct sys_getpgrp_args *args)
{
	td->td_retval[0] = _current->pgrp;
	return (0);
}

int sys_getpgid(struct thread *td, struct sys_getpgid_args *args)
{
	if (args->pid == 0 || args->pid == _current->id) {
		td->td_retval[0] = _current->pgrp;
		return (0);
	}
	kTask_t *t = schedFindTask(args->pid);
	if (t == NULL) {
		td->td_retval[0] = ESRCH;
		return (ESRCH);
	}
	td->td_retval[0] = t->pgrp;
	return (0);
}

int sys_setpgid(struct thread *td, struct sys_setpgid_args *args)
{
	kTask_t *t;

	/* Target is the calling process itself. */
	if (args->pid == 0 || args->pid == _current->id) {
		_current->pgrp = (args->pgid == 0)
		    ? (uint32_t)_current->id
		    : (uint32_t)args->pgid;
		td->td_retval[0] = 0;
		return (0);
	}

	/* Target is another process — must be a direct child of the caller. */
	t = schedFindTask((uint32_t)args->pid);
	if (t == NULL || t->parent != _current) {
		td->td_retval[0] = -1;
		return (0);
	}
	t->pgrp = (args->pgid == 0) ? (uint32_t)t->id : (uint32_t)args->pgid;
	td->td_retval[0] = 0;
	return (0);
}

int sys_gettimeofday(struct thread *td, struct sys_gettimeofday_args *args)
{
	gettimeofday(args->tp, args->tzp);
	td->td_retval[0] = 0;
	return (0);
}

int sys_getlogin(struct thread *thr, struct sys_getlogin_args *args)
{
	int error = 0;
	size_t len = args->namelen;

	if (len > sizeof(_current->username))
		len = sizeof(_current->username);

	memcpy(args->namebuf, _current->username, len);

	return (error);
}

int sys_setlogin(struct thread *thr, struct sys_setlogin_args *args)
{
	int error = 0;

	memcpy(_current->username, args->namebuf, 256);

	return (error);
}

int sys_getrlimit(struct thread *thr, struct sys_getrlimit_args *args)
{
	int error = 0;

	struct rlimit *rlim = 0x0;

	switch (args->which)
	{
	case 0:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 1:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 2:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 3:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 4:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 5:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 6:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 7:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 8:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 9:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 10:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 11:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 12:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 13:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	case 14:
		args->rlp->rlim_cur = thr->rlim[args->which].rlim_cur;
		args->rlp->rlim_max = thr->rlim[args->which].rlim_max;
		break;
	default:
		error = -1;
		kprintf("[getrlimit: %i]", args->which);
	}

	return (error);
}

int sys_setrlimit(struct thread *thr, struct sys_setrlimit_args *args)
{
	int error = 0;

	switch (args->which)
	{
	case 0:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 1:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 2:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 3:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 4:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 5:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 6:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 7:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 8:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 9:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 10:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 11:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 12:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 13:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	case 14:
		thr->rlim[args->which].rlim_cur = args->rlp->rlim_cur;
		thr->rlim[args->which].rlim_max = args->rlp->rlim_max;
		break;
	default:
		error = -1;
		kprintf("[setrlimit: %i]", args->which);
	}

	return (error);
}

/*
 * uname(2) — syscall 164.
 * Fills a userland struct utsname from the version macros in version.h.
 * The layout must match include/sys/utsname.h (_SYS_NAMELEN = 256).
 */
struct _kern_utsname
{
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

int sys_uname(struct thread *td, struct sys_uname_args *args)
{
	struct _kern_utsname *uts = (struct _kern_utsname *)args->buf;

	if (uts == 0x0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	memset(uts, 0, sizeof(*uts));
	strncpy(uts->sysname, "UBIX", sizeof(uts->sysname) - 1);
	strncpy(uts->nodename, "ubixos", sizeof(uts->nodename) - 1);
	strncpy(uts->release, UBIXOS_VERSION_RELEASE, sizeof(uts->release) - 1);
	strncpy(uts->version, UBIXOS_VERSION_STRING, sizeof(uts->version) - 1);
	strncpy(uts->machine, "i386", sizeof(uts->machine) - 1);

	td->td_retval[0] = 0;
	return (0);
}

/* set_tid_address(2) — Linux 258. Store tidptr; return current TID (PID). */
int sys_set_tid_address(struct thread *td, struct sys_set_tid_address_args *uap)
{
	td->td_retval[0] = _current->id;
	return (0);
}

/* setsid(2) — FreeBSD 147. Become session leader; new session has no ctty. */
int sys_setsid(struct thread *td, struct sys_setsid_args *args)
{
	_current->pgrp   = (uint32_t)_current->id;
	_current->sid    = (uint32_t)_current->id;
	_current->ct_tty = NULL;
	td->td_retval[0] = (int)_current->id;
	return (0);
}

/* getrusage(2) — FreeBSD 117. Return zeroed rusage; timing not tracked. */
int sys_getrusage(struct thread *td, struct sys_getrusage_args *args)
{
	if (args->rusage == NULL) {
		td->td_retval[0] = -1;
		return (EFAULT);
	}
	memset(args->rusage, 0, sizeof(struct rusage));
	td->td_retval[0] = 0;
	return (0);
}
