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
#include <machine/tls.h>
#include <i386/pcpu_asm.h>
#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>
#include <ubixos/endtask.h>
#include <ubixos/random.h>
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
#include <isa/pit.h>
#include <vmm/vmm.h>
#include <vmm/mmap.h>
#include <vmm/paging.h>

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

/*
 * sys_getrandom (FreeBSD syscall 563) — fill the caller's buffer with bytes
 * from the kernel ChaCha20 CSPRNG (the same source as /dev/urandom).  Backs
 * musl getrandom()/getentropy(); arc4random ultimately draws from here.  Runs
 * in the calling process's context, so the user pointer is written directly.
 * The flags argument (GRND_NONBLOCK/GRND_RANDOM) is accepted but ignored — the
 * generator never blocks.
 *
 * @return number of bytes written, or -1 on a NULL buffer.
 */
int sys_getrandom(struct thread *td, struct sys_getrandom_args *uap)
{
	if (uap->buf == NULL)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	krandom_bytes(uap->buf, uap->buflen);

	td->td_retval[0] = (int)uap->buflen;
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
	{
		return (error);
	}

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
	u_int32_t base = (u_int32_t)uap->addr & ~0xFFFU;
	u_int32_t end = base + round_page(uap->len);
	u_int16_t flags = PAGE_PRESENT | PAGE_USER;

	if (uap->prot & PROT_WRITE)
	{
		flags |= PAGE_WRITE;
	}

	for (u_int32_t va = base; va < end; va += PAGE_SIZE)
	{
		vmm_set_page_attributes(va, flags);
	}

	td->td_retval[0] = 0;
	return (0);
}

int sys_kill(struct thread *td, struct sys_kill_args *uap)
{
	/*
	 * Process-group signalling (POSIX):
	 *   pid == 0  -> every process in the caller's process group.
	 *   pid <  0  -> every process in the group |pid|.
	 * Used for session teardown: vlogin kills the session's group on logout.
	 * The kernel is cooperative (no preemption without an explicit yield), so
	 * walking taskList here without yielding is atomic and safe.
	 */
	if (uap->pid <= 0)
	{
		u_int32_t pgrp = (uap->pid == 0) ? _current->pgrp : (u_int32_t)(-uap->pid);
		kTask_t *t;
		int found = 0;

		for (t = taskList; t != NULL; t = t->next)
		{
			if (t->pgrp != pgrp || t->id <= 1)
				continue; /* never signal init (pid 1) */
			found = 1;
			if (uap->signum != 0)
				signal_post_kill(_current->id, (int)t->id, uap->signum);
		}
		td->td_retval[0] = found ? 0 : -1;
		return (td->td_retval[0]);
	}

	if (uap->signum == 0)
	{
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

	kargs.pid = uap->tid;
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
	p[0] = (int32_t)tv.tv_sec;           /* tv_sec low  32 bits */
	p[1] = 0;                            /* tv_sec high 32 bits */
	p[2] = (int32_t)(tv.tv_usec * 1000); /* tv_nsec              */
	p[3] = 0;                            /* padding              */

	td->td_retval[0] = 0;
	return (0);
}

/* FUTEX command mask — strips FUTEX_PRIVATE (0x80) / FUTEX_CLOCK_REALTIME (0x100). */
#define FUTEX_CMD_MASK 0x7F
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAIT_BITSET 9

/* Context for the FUTEX_WAIT sleep condition (see futex_wait_cond). */
struct futex_wait_ctx
{
	volatile int *uaddr;
	int           val;
};

/**
 * futex_wait_cond - stop-waiting predicate for a FUTEX_WAIT sleep.
 *
 * Returns true once the futex word no longer holds the value the waiter slept
 * on — i.e. a waker changed it.  sched_wait_event_timeout() re-checks this
 * under interrupts-off, which on a single CPU makes the compare-and-sleep
 * atomic with respect to FUTEX_WAKE (no other thread can run between).
 */
static int futex_wait_cond(void *arg)
{
	struct futex_wait_ctx *c = (struct futex_wait_ctx *)arg;

	return (*c->uaddr != c->val);
}

/**
 * sys_membarrier - no-op on a uniprocessor.
 *
 * membarrier(2) forces memory barriers across all CPUs running threads of the
 * process; on a single CPU that is trivially already satisfied, so every command
 * is a successful no-op.  musl calls MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED
 * once from pthread_create and ignores the result.  FreeBSD has membarrier
 * natively (POSIX syscall 584), so it lives on the FreeBSD-numbered table.
 */
int sys_membarrier(struct thread *td, void *uap)
{
	(void)uap;
	td->td_retval[0] = 0;
	return (0);
}

/**
 * sys_futex - minimal futex on the kernel's wait_chan sleep/wake primitive.
 *
 * UbixOS-native ABI (int $0x81) — futex is a Linux primitive with no FreeBSD
 * syscall number, so it lives in the native table, not the FreeBSD-numbered
 * POSIX one.  musl's pthread layer (mutex/cond/sem/barrier/once) funnels
 * through FUTEX_WAIT/FUTEX_WAKE.  Threads share one address space, so a user
 * virtual address is a stable wait token across the whole group — we sleep and
 * wake directly on `uaddr`.
 *
 * REQUEUE/CMP_REQUEUE are treated as WAKE (waking instead of requeueing is
 * correct — woken waiters simply re-contend; the requeue target is ignored, a
 * v1 simplification that is safe for musl's condition variables).  Priority-
 * inheritance and other ops return -ENOSYS so musl falls back.
 *
 * @return 0 on a normal wake / successful wake; -errno (EAGAIN if the value
 *         already changed, ETIMEDOUT on timeout, ENOSYS for unsupported ops).
 */
int sys_futex(struct thread *td, struct sys_futex_args *uap)
{
	int  cmd = uap->op & FUTEX_CMD_MASK;
	int *uaddr = uap->uaddr;

	if (uaddr == NULL)
	{
		td->td_retval[0] = EINVAL;
		return (-1);
	}

	switch (cmd)
	{
	case FUTEX_WAIT:
	case FUTEX_WAIT_BITSET: /* bitset/abs-time nuance ignored — treat as WAIT */
	{
		struct futex_wait_ctx ctx;
		u_int32_t             ticks = 0;
		int                   timed_out;

		/* Value already changed → don't sleep (classic futex race guard). */
		if (*uaddr != uap->val)
		{
			td->td_retval[0] = EAGAIN;
			return (-1);
		}

		/* Optional RELATIVE timeout.  musl i386 timespec is { int64 tv_sec;
		 * long tv_nsec }; read it with a matching local layout so we do not
		 * depend on the kernel's struct timespec.  PIT_TIMER ticks/second. */
		if (uap->timeout != NULL)
		{
			struct
			{
				long long tv_sec;
				long      tv_nsec;
			} ts;

			memcpy(&ts, uap->timeout, sizeof(ts));
			ticks = (u_int32_t)((long long)ts.tv_sec * PIT_TIMER +
			                    ts.tv_nsec / (1000000000L / PIT_TIMER));
			if (ticks == 0)
				ticks = 1; /* non-NULL but sub-tick → wait at least one tick */
		}

		ctx.uaddr = (volatile int *)uaddr;
		ctx.val   = uap->val;

		timed_out = sched_wait_event_timeout(uaddr, futex_wait_cond, &ctx, ticks);
		if (timed_out)
		{
			td->td_retval[0] = ETIMEDOUT;
			return (-1);
		}
		td->td_retval[0] = 0;
		return (0);
	}

	case FUTEX_WAKE:
	case FUTEX_REQUEUE:     /* wake instead of requeue (v1) */
	case FUTEX_CMP_REQUEUE: /* wake instead of requeue (v1) */
		sched_wakeup_chan(uaddr);
		td->td_retval[0] = (uap->val > 0) ? uap->val : 0; /* >=0: not -ENOSYS */
		return (0);

	default:
		td->td_retval[0] = ENOSYS; /* PI mutexes, WAKE_OP, … → musl fallback */
		return (-1);
	}
}

/*
 * set_thread_area — TLS setup syscall (slot 351).
 *
 * UbixOS musl calls __syscall(SYS_set_thread_area, pthread_ptr) where
 * pthread_ptr IS the TLS base (the pthread struct address).  This is NOT the
 * Linux struct user_desc interface — musl passes the raw TLS pointer as a
 * single argument.  We record the base and hand the actual install to the
 * machine layer (i386 → LDT[1] + %gs; aarch64 → TPIDR_EL0), so this syscall is
 * arch-neutral.
 */
int sys_set_thread_area(struct thread *td, struct sys_set_thread_area_args *uap)
{
	/* uap->u_info IS the TLS base pointer (the pthread struct address). */
	uintptr_t base = (uintptr_t)uap->u_info;

	if (base == 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	/*
	 * Remember this thread's TLS base; cpu_switch() re-installs the resuming
	 * thread's own base on each context switch (see context_switch.c).  The
	 * install itself is machine dependent — behind <machine/tls.h>.
	 */
	_current->tls_base = (u_int32_t)base;
	machine_set_tls(td, base);

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
		{
			continue;
		}
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
		{
			continue;
		}
		sys_read(td, &ra);
		if (td->td_retval[0] < 0)
		{
			td->td_retval[0] = total > 0 ? total : td->td_retval[0];
			return (-1);
		}
		total += td->td_retval[0];
		if ((size_t)td->td_retval[0] < ra.nbyte)
		{
			break;
		}
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
	{
		td->td_retval[0] = 1;
	}
	else
	{
		td->td_retval[0] = 0;
	}
	return (0);
}

#define WNOHANG 0x0001   /* don't block if no child ready */
#define WUNTRACED 0x0002 /* report stopped children */

#define W_STOPPED(sig) (((sig) << 8) | 0x7f)
#define W_EXITED(code) ((code) << 8)
#define W_SIGNALED(sig) ((sig) & 0x7f)

/*
 * Find a direct child of _current that has exited or (if WUNTRACED) stopped.
 * When a DEAD child is found it is spliced from taskList and queued for
 * freeing — the caller reads child->id before the system task reclaims it.
 */
static kTask_t *wait_find_child(int want_pid, int options, int *wstatus)
{
	kTask_t *t;
	int untraced = options & WUNTRACED;

	for (t = taskList; t != NULL; t = t->next)
	{
		if (t->parent != _current)
			continue;
		if (want_pid != -1 && (int)t->id != want_pid)
			continue;
		/*
		 * Accept both DEAD and ZOMBIE: with two-phase exit, sched()
		 * transitions ZOMBIE→DEAD asynchronously.  If the parent calls
		 * wait_find_child before that tick fires, the child is still
		 * ZOMBIE.  Collecting it here is correct — the parent is the one
		 * doing the reaping, not sched().
		 */
		if (t->state == DEAD || t->state == ZOMBIE)
		{
			if (wstatus)
				*wstatus = W_EXITED(0);
			if (t->prev != NULL)
				t->prev->next = t->next;
			else
				taskList = t->next;
			if (t->next != NULL)
				t->next->prev = t->prev;
			pid_hash_remove(t);
			sched_addDelTask(t);
			if (_current->children > 0)
				_current->children--;
			return (t);
		}
		if (untraced && t->state == STOPPED && t->t_stopped_sig != 0)
		{
			if (wstatus)
				*wstatus = W_STOPPED(t->t_stopped_sig);
			t->t_stopped_sig = 0;
			return (t);
		}
	}
	return (NULL);
}

int sys_wait4(struct thread *td, struct sys_wait4_args *args)
{
#define SIG_PENDING_UNBLOCKED_W(td) ((td)->sig_pending & ~(td)->sigmask.__bits[0])
	kTask_t *child;
	int wstatus = 0;

	/*
	 * ECHILD check.  For wait-any (pid == -1) we trust children counter
	 * but also scan taskList for uncollected dead/zombie children whose
	 * counter decrement may have raced.  For a specific pid we verify the
	 * target exists and is our direct child.
	 */
	if (args->pid == -1)
	{
		if (_current->children <= 0)
		{
			kTask_t *t;
			int found = 0;
			for (t = taskList; t != NULL; t = t->next)
			{
				if (t->parent == _current)
				{
					found = 1;
					break;
				}
			}
			if (!found)
			{
				td->td_retval[0] = -ECHILD;
				return (ECHILD);
			}
		}
	}
	else
	{
		kTask_t *t = schedFindTask((u_int32_t)args->pid);
		if (t == NULL || t->parent != _current)
		{
			td->td_retval[0] = -ECHILD;
			return (ECHILD);
		}
	}

	if (args->options & WNOHANG)
	{
		child = wait_find_child(args->pid, args->options, &wstatus);
		if (child == NULL)
		{
			td->td_retval[0] = 0;
			if (args->status)
				*args->status = 0;
			return (0);
		}
		if (args->status)
			*args->status = wstatus;
		td->td_retval[0] = (int)child->id;
		return (0);
	}

	/*
	 * Blocking wait: check → sleep → re-check → yield.
	 *
	 * The re-check immediately after sched_sleep handles a lost-wakeup
	 * race: if the child went ZOMBIE between our "not ready" check and
	 * sched_sleep, the ZOMBIE handler in sched() tried to wake us while
	 * we were still RUNNING (no-op), then sched_sleep dequeued us.  The
	 * re-check sees the now-DEAD/ZOMBIE child and avoids sleeping forever.
	 */
	for (;;)
	{
		child = wait_find_child(args->pid, args->options, &wstatus);
		if (child != NULL)
			break;

		if (SIG_PENDING_UNBLOCKED_W(td))
		{
			td->td_retval[0] = -EINTR;
			return (EINTR);
		}

		sched_sleep(_current, WAIT);

		/* Re-check to catch lost-wakeup. */
		child = wait_find_child(args->pid, args->options, &wstatus);
		if (child != NULL)
		{
			sched_wakeup(_current);
			break;
		}

		sched_yield();
		sched_wakeup(_current);
	}

	if (args->status)
		*args->status = wstatus;
	td->td_retval[0] = (int)child->id;
	return (0);
#undef SIG_PENDING_UNBLOCKED_W
}

int sys_sysarch(struct thread *td, struct sys_sysarch_args *args)
{
#if defined(__i386__)

	void **segbase = 0x0;
	u_int32_t base_addr = 0x0;

	if (args->op == 10)
	{
		kprintf("SETGSBASE: 0x%X:0x%X", args->parms, args->parms[0]);

		segbase = (void **)args->parms;

		kprintf("SGS: [0x%X:0x%X]", segbase[0], segbase[1]);
		base_addr = (u_int32_t)segbase[0];

		struct gdtDescriptor *tmp_desc = 0x0;

		tmp_desc = (struct gdtDescriptor *)(VMM_USER_LDT + sizeof(struct gdtDescriptor)); // taskLDT[1];

		tmp_desc->limitLow = 0xFFFF; //(0xFFFFF & 0xFFFF);
		tmp_desc->limitHigh = 0xF;   //(0xFFFFF >> 16);
		tmp_desc->baseLow = (base_addr & 0xFFFF);
		tmp_desc->baseMed = ((base_addr >> 16) & 0xFF);
		tmp_desc->access = ((dData + dWrite + dBig + dBiglim + dDpl3) + dPresent) >> 8;
		tmp_desc->granularity = ((dData + dWrite + dBig + dBiglim + dDpl3) & 0xFF) >> 4;
		tmp_desc->baseHigh = base_addr >> 24;

		/*
		 * Reload LDTR and refresh the %gs hidden descriptor cache from the
		 * just-updated LDT[1] (TLS) entry, then restore %gs = SEL_PCPU.  The
		 * user's %gs = 0xF is re-installed by the syscall exit's `pop %gs`
		 * (from the saved trapframe), so the live kernel %gs must be left as
		 * the per-CPU selector or every subsequent _current (%gs:8) access in
		 * kernel context would read the TLS base instead of g_pcpu.
		 */
		asm("push %eax\n"
		    "mov $0x18,%ax\n"
		    "lldt %ax\n"
		    "mov $0xF,%eax\n"
		    "mov %eax,%gs\n" /* refresh %gs cache from updated LDT[1] */
		    ASM_PCPU_LOAD_GS /* then restore %gs = SEL_PCPU so _current (%gs:8) stays valid */
		    "pop %eax\n");

		td->td_retval[0] = 0;
	}
	else
	{
		kprintf("sysarch(%i,NULL)", args->op);
		td->td_retval[0] = -1;
	}
#else
	/* aarch64 sets the EL0 TLS base via TPIDR_EL0 at user level (msr), not a
	 * sysarch/GDT call — nothing to do here. */
	(void)args;
	td->td_retval[0] = -1;
#endif
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
	if (args->pid == 0 || args->pid == _current->id)
	{
		td->td_retval[0] = _current->pgrp;
		return (0);
	}
	kTask_t *t = schedFindTask(args->pid);
	if (t == NULL)
	{
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
	if (args->pid == 0 || args->pid == _current->id)
	{
		_current->pgrp = (args->pgid == 0) ? (u_int32_t)_current->id : (u_int32_t)args->pgid;
		td->td_retval[0] = 0;
		return (0);
	}

	/* Target is another process — must be a direct child of the caller. */
	t = schedFindTask((u_int32_t)args->pid);
	if (t == NULL || t->parent != _current)
	{
		td->td_retval[0] = -1;
		return (0);
	}
	t->pgrp = (args->pgid == 0) ? (u_int32_t)t->id : (u_int32_t)args->pgid;
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
	{
		len = sizeof(_current->username);
	}

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
struct kern_utsname
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
	struct kern_utsname *uts = (struct kern_utsname *)args->buf;

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
	_current->pgrp = (u_int32_t)_current->id;
	_current->sid = (u_int32_t)_current->id;
	_current->ct_tty = NULL;
	td->td_retval[0] = (int)_current->id;
	return (0);
}

/* getrusage(2) — FreeBSD 117. Return zeroed rusage; timing not tracked. */
int sys_getrusage(struct thread *td, struct sys_getrusage_args *args)
{
	if (args->rusage == NULL)
	{
		td->td_retval[0] = -1;
		return (EFAULT);
	}
	memset(args->rusage, 0, sizeof(struct rusage));
	td->td_retval[0] = 0;
	return (0);
}

/*
 * setitimer(2) / getitimer(2) — FreeBSD slots 83 / 86.
 * Stub: report no previous timer; ignore the new value.  tcsh calls these to
 * set up SIGALRM-based prompt timeouts; returning success with a zeroed oitv
 * is enough for tcsh to proceed without spinning on ENOSYS.
 */
int sys_setitimer(struct thread *td, struct setitimer_args *uap)
{
	if (uap->oitv != NULL)
		memset(uap->oitv, 0, sizeof(struct itimerval));
	td->td_retval[0] = 0;
	return (0);
}

int sys_getitimer(struct thread *td, struct setitimer_args *uap)
{
	if (uap->itv != NULL)
		memset(uap->itv, 0, sizeof(struct itimerval));
	td->td_retval[0] = 0;
	return (0);
}
