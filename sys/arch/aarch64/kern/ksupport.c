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
#include <mpi/mpi.h>            /* mpi_destroyProcessMboxes — free a task's mailboxes on exit */
#include <sys/descrip.h>        /* g_device_find — bus-device lookup hook (ubx_device_find shim) */
#include <sys/bus.h>            /* struct ubx_device */
#include <sys/sysproto_posix.h> /* sys_close — close the task's fds on exit (pipe-EOF) */

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
 * Shut down / reboot the machine — bring-up: just halt.
 */
int sys_shutdown(shutdownCMD_t cmd)
{
	(void)cmd;
	kprintf("sys_shutdown: halting.\n");
	for (;;)
		__asm__ volatile("wfi");
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
 * Terminate a task — bring-up: mark it ZOMBIE (the generic reap path then
 * transitions it to DEAD).  The full address-space teardown is not ported.
 */
void endTask(pidType pid)
{
	(void)pid;
	if (_current != 0)
	{
		/* Close the task's open PIPE fds so a parent blocked reading this task's pipe
		 * write-end sees EOF when the writer exits (the bmake Cmd_Exec case: fork +
		 * read child output — without this it hangs forever).  ONLY pipes: the full
		 * reaper is unported on aarch64 and never closed fds, so driving the tty/pty/
		 * socket fileops close() from here on every exit is both unnecessary and unsafe
		 * — closing a job-control child's inherited pty fd corrupted the still-shared
		 * device and panicked the kernel (tcsh `ls` repeated).  Leave non-pipe fds to
		 * the (pre-existing) no-op teardown.
		 *
		 * Threads (rfork(RFMEM)) SHALLOW-share one fd table across a tgid, so only the
		 * LAST thread of the group may close them — an earlier thread exiting must
		 * leave the shared fd objects intact for its still-running siblings. */
		if (sched_tgid_others_alive(_current->tgid, _current->id) == 0)
		{
			for (int fd = 0; fd < O_FILES; fd++)
			{
				struct file *f = (struct file *)_current->td.o_files[fd];

				if (f != 0 && f->fd_type == FD_TYPE_PIPE)
				{
					struct sys_close_args ca;
					ca.fd = fd;
					sys_close(&_current->td, &ca);
				}
			}
		}

		/* Free the task's MPI mailboxes so their names/pids do not leak — a leaked
		 * mailbox blocks a relaunched owner (e.g. a respawned authd/login) from
		 * recreating it. */
		mpi_destroyProcessMboxes(_current->id);
		_current->state = ZOMBIE;
	}
}
