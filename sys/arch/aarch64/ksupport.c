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

/* ---- system vitals ------------------------------------------------------ */

static vitalsNode g_vitals;
vitalsNode *systemVitals = &g_vitals;

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
	va_list ap;
	uart_puts("\nKERNEL PANIC: ");
	va_start(ap, fmt);
	uart_vprintf(fmt, ap);
	va_end(ap);
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

/**
 * Acquire a spinlock.  Single-CPU bring-up: just record the state.
 */
void spinLock(spinLock_t lock)
{
	if (lock != 0)
		lock->locked = 1;
}

/**
 * Release a spinlock.
 */
void spinUnlock(spinLock_t lock)
{
	if (lock != 0)
		lock->locked = 0;
}

/**
 * Try to acquire a spinlock.  Single-CPU: always succeeds.
 *
 * @return 0 on success (lock acquired).
 */
int spinTryLock(spinLock_t lock)
{
	if (lock != 0)
		lock->locked = 1;
	return 0;
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
		_current->state = ZOMBIE;
}
