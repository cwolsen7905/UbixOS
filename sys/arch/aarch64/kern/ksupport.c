/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 bring-up kernel-support stubs.
 *
 * Minimal stand-ins for the generic kernel facilities the scheduler core
 * depends on, so sched_core/sched_dispatch can LINK and run on aarch64 before
 * the real subsystems are ported.  Each is intentionally simple and clearly
 * temporary:
 *   - kmalloc/kfree   bump allocator over a static arena (no real free yet)
 *   - kpanic          print + halt
 *   - spinLock*       single-CPU no-ops (no other CPU to contend with)
 *   - systemVitals    a static vitals node (sysTicks driven by timer.c)
 *   - callout_*        no-ops (timed sleeps not wired yet)
 *   - krandom_stir     no-op (CSPRNG not ported)
 *   - sys_shutdown     halt
 *   - endTask          mark ZOMBIE (full reaping path not ported)
 *   - reboot_at_tick   inactive
 *
 * These are replaced one by one as kmalloc/vmm, the CSPRNG, callouts and
 * endtask are ported to aarch64.
 */

#include "bringup.h"
#include <sys/types.h>
#include <ubixos/sched.h>
#include <ubixos/spinlock.h>
#include <ubixos/vitals.h>
#include <ubixos/callout.h>
#include <sys/shutdown.h>
#include <mpi/mpi.h>               /* mpi_destroyProcessMboxes — free a task's mailboxes on exit */
#include <sys/descrip.h>           /* g_device_find — bus-device lookup hook (ubx_device_find shim) */
#include <sys/bus.h>               /* struct ubx_device */
#include <sys/thread.h>            /* O_FILES */
#include <sys/sysproto_posix.h>    /* sys_close — close a dying task's descriptors */
#include <ubixos/tty.h>            /* tty_hangup_by_owner */
#include <lib/kmalloc.h>           /* kfree — reap a dead task */
#include <ubixos/wait.h>           /* save_flags/cli/restore_flags */
#include <ubixos/sched_internal.h> /* taskList — reparent orphans */
#include <aarch64/pcpu.h>          /* g_pcpu — is a dead task still on some CPU? */

/* systemVitals is now the real generic vitals node (sys/kern/vitals.c),
 * allocated by vitals_init() during kmain bring-up. */

/* ---- deferred reboot (inactive on aarch64 bring-up) --------------------- */

volatile u_int32_t reboot_at_tick = 0;

/* kmalloc/kfree are now the real generic allocator (sys/lib/kmalloc.c), backed
 * by vmm_get_free_malloc_page (vmm_machdep.c) over identity-mapped RAM. */

/* ---- panic -------------------------------------------------------------- */

/**
 * Print a message and halt the CPU.  Never returns.
 */
void kpanic(const char *fmt, ...)
{
	char buf[512];
	va_list ap;

	uart_puts("\nKERNEL PANIC: ");
	va_start(ap, fmt);
	kvprintf(fmt, NULL, buf, 10, ap, sizeof(buf) - 1);
	va_end(ap);
	buf[sizeof(buf) - 1] = '\0';
	uart_puts(buf);
	for (;;)
		__asm__ volatile("wfi");
}

/* ---- spinlocks (single-CPU) -------------------------------------------- */

/**
 * Initialise a spinlock.
 */
void spinLockInit(spinLock_t lock)
{
	if (lock != 0)
		lock->locked = 0;
}

/*
 * Real atomic spinlock (smp-plan M3/M4).  Until APs were brought up these were
 * uniprocessor stubs (just set lock->locked); with a second core actually
 * running, the kernel heap (kmalloc) and every other spinLock-guarded structure
 * race and corrupt.  Mirror i386's yielding-mutex contract: atomic acquire via
 * LL/SC (ldaxr/stxr), yield the CPU while contended (so the holder runs), and a
 * store-release unlock.
 */

/**
 * Acquire a spinlock (atomic test-and-set; yields while contended).
 */
void spinLock(spinLock_t lock)
{
	if (lock == 0)
		return;
	for (;;)
	{
		u_int32_t cur, res;
		__asm__ __volatile__("1: ldaxr   %w0, [%2]      \n"
		                     "   cbnz    %w0, 2f        \n" /* held -> bail, don't store */
		                     "   stxr    %w1, %w3, [%2] \n" /* try claim (store 1) */
		                     "   cbnz    %w1, 1b        \n" /* lost exclusive -> retry */
		                     "2:                        \n"
		                     : "=&r"(cur), "=&r"(res)
		                     : "r"(&lock->locked), "r"(1u)
		                     : "memory");
		if (cur == 0)
			return; /* was free and we claimed it */
		while (lock->locked != 0)
			sched_yield(); /* mutex-style: let the holder run */
	}
}

/**
 * Release a spinlock (store-release, pairs with the ldaxr acquire).
 */
void spinUnlock(spinLock_t lock)
{
	if (lock != 0)
		__asm__ __volatile__("stlr wzr, [%0]" : : "r"(&lock->locked) : "memory");
}

/**
 * Try to acquire a spinlock without waiting.
 *
 * @return 0 if acquired, non-zero if already held.
 */
int spinTryLock(spinLock_t lock)
{
	u_int32_t cur, res = 1;

	if (lock == 0)
		return 0;
	__asm__ __volatile__("   ldaxr   %w0, [%2]      \n"
	                     "   cbnz    %w0, 1f        \n" /* held -> fail */
	                     "   stxr    %w1, %w3, [%2] \n" /* try claim */
	                     "1:                        \n"
	                     : "=&r"(cur), "+&r"(res)
	                     : "r"(&lock->locked), "r"(1u)
	                     : "memory");
	return (cur == 0 && res == 0) ? 0 : 1; /* acquired only if free AND store won */
}

/* callouts are now the real generic subsystem (sys/kern/callout.c). */

/* ---- misc stubs --------------------------------------------------------- */

/* krandom_stir is now the real CSPRNG (sys/kern/random.c). */

/**
 * Shut down / reboot the machine (PSCI, via machine_power in sys/kern/reboot.c).
 */
int sys_shutdown(shutdownCMD_t cmd)
{
	kprintf("sys_shutdown: %s.\n", cmd == HALT ? "powering off" : "rebooting");
	machine_power(cmd == HALT);
	return 0;
}

/**
 * Bus-device lookup shim: the generic newbus registry (sys/sys/bus.c) is not
 * linked on aarch64 — block devices register through the g_device_find hook
 * instead (virtio-blk installs it).  devfs and other generic code call
 * ubx_device_find() directly, so provide it here by delegating to that hook.
 *
 * @return the matching device, or NULL (the hook is unset, or no match).
 */
struct ubx_device *ubx_device_find(int major, int minor)
{
	if (g_device_find == 0)
		return (0);
	return (struct ubx_device *)g_device_find(major, minor);
}

/**
 * assert() failure handler — print the location and panic.
 */
void __assert(const char *func, const char *file, int line, const char *expr)
{
	kpanic("assert failed: %s (%s:%d in %s)\n", expr, file, line, func != 0 ? func : "?");
}

/**
 * Terminate the current task: the aarch64 port of sys/kern/endtask.c.
 *
 * Runs on the dying task's own kernel stack while its address space is still
 * active.  Wakes a pthread_join()er (CLONE_CHILD_CLEARTID), and — when this is
 * the last task of its thread group — closes every open descriptor (dropping
 * pipe writer counts so readers see EOF) and flags the address space for the
 * reaper (aarch64_reap_dead_tasks) to free once the task is off-CPU.  Then
 * frees its MPI mailboxes, hands the terminal back and becomes a ZOMBIE for the
 * parent's wait().  @pid is ignored: every caller ends _current.
 */
void endTask(pidType pid)
{
	(void)pid;
	if (_current == 0)
		return;

	/* CLONE_CHILD_CLEARTID: zero the user word and wake a pthread_join()er
	 * while this thread's address space is still mapped. */
	if (_current->clear_tid != NULL)
	{
		*_current->clear_tid = 0;
		sched_wakeup_chan(_current->clear_tid);
		_current->clear_tid = NULL;
	}

	{
		int last = (sched_tgid_others_alive(_current->tgid, _current->id) == 0);

		_current->reap_free_as = last ? 1 : 0; /* siblings still use a shared space */
		/* Flush MAP_SHARED file mappings while the address space is still live. */
		if (last)
			aarch64_shmap_release_all(_current, (u_int64_t *)(uintptr_t)_current->md.md_ttbr0);
		/* Close descriptors, stop the sleep callout, hand children to init —
		 * shared with i386 (kern_fork.c). */
		proc_exit_cleanup(_current, last);
	}

	/* Free the task's MPI mailboxes so their names/pids do not leak — a leaked
	 * mailbox blocks a relaunched owner (e.g. a respawned authd/login) from
	 * recreating it. */
	mpi_destroyProcessMboxes(_current->id);

	if (_current->term != NULL && _current->term->owner == _current->id && _current->parent != NULL)
		_current->term->owner = _current->parent->id;
	tty_hangup_by_owner(_current->id);

	_current->state = ZOMBIE;
}

/**
 * Free the tasks wait() (or the scheduler, for orphans) has queued for deletion:
 * their user address space (unless a thread sibling still shares it), kernel
 * stack and task struct.
 *
 * Called from wait4 — process context, where the yielding VMM/heap locks the
 * frees take are safe (not from an idle task or the timer IRQ).  A task is only
 * freed once it is surely off every CPU (see the test below), since wait() can
 * collect a still-running ZOMBIE.
 */
void aarch64_reap_dead_tasks(void)
{
	static kTask_t *g_reap_pending; /* dequeued, waiting until surely off-CPU */
	static struct spinLock g_reap_lock = SPIN_LOCK_INITIALIZER;
	kTask_t *t, **pp;
	u_int32_t now = systemVitals->sysTicks;

	/* Several CPUs' idle loops (and wait4) call this; one reaper at a time, and
	 * a busy reaper is simply skipped (the next caller picks the work up). */
	if (spinTryLock(&g_reap_lock) != 0)
		return;

	while ((t = sched_getDelTask()) != NULL)
	{
		t->reap_tick = now;
		t->next = g_reap_pending;
		g_reap_pending = t;
	}

	/* wait() may collect a child that is still a ZOMBIE — possibly still
	 * finishing endTask on another CPU.  Free it only once no CPU has it as
	 * current and two ticks have passed (every CPU has rescheduled since). */
	for (pp = &g_reap_pending; (t = *pp) != NULL;)
	{
		int i, busy = (now - t->reap_tick) < 2;

		for (i = 0; i < MAXCPU && !busy; i++)
			if (g_pcpu[i].current == t)
				busy = 1;
		if (busy)
		{
			pp = &t->next;
			continue;
		}
		*pp = t->next;
		if (t->reap_free_as && t->md.md_ttbr0 != 0 &&
		    t->md.md_ttbr0 != (u_int64_t)(uintptr_t)aarch64_kernel_l1())
			pmap_free_user_space((u_int64_t *)(uintptr_t)t->md.md_ttbr0);
		if (t->kernelStack != NULL)
			kfree(t->kernelStack);
		kfree(t);
	}
	spinUnlock(&g_reap_lock);
}
