/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 bring-up support shims (smp-plan / Phase 3).  Minimal implementations of
 * the few machine-dependent / global symbols the first machine-independent kernel
 * objects (vmm_memory.c, kmalloc.c) reference, so they can link before the full
 * generic kernel is ported.  The real kprintf/vitals/etc. replace these shims as
 * later phases link them.  Sibling of aarch64's ksupport.c.
 */

#include "x86_64.h"
#include <ubixos/spinlock.h>
#include <ubixos/vitals.h>
#include <ubixos/callout.h>
#include <ubixos/endtask.h>
#include <ubixos/random.h>
#include <ubixos/sched.h>
#include <sys/shutdown.h>
#include <sys/types.h>

/* Set by vitals_init() once linked; NULL is safe — vmm_memory.c guards on it. */
vitalsNode *systemVitals = 0;

/* Reboot-countdown flag the scheduler tick checks (set by the kbd Ctrl-Alt-Del
 * affordance on i386; inactive here). */
volatile u_int32_t reboot_at_tick = 0;

/* --- bring-up stubs for the scheduler's optional subsystems ------------------ *
 * sched_core/sched_dispatch reference these; the timed-sleep (callout), CSPRNG
 * stir, task-reaper (endTask), CPU enumeration and shutdown paths are not wired
 * on x86_64 yet.  Stubbed so the scheduler links + basic thread switching runs;
 * replaced as each real subsystem is linked.  Uniprocessor: one CPU. */
void callout_reset(struct callout *c, u_int32_t ticks, void (*fn)(void *), void *arg)
{
	(void)c;
	(void)ticks;
	(void)fn;
	(void)arg;
}
void callout_stop(struct callout *c)
{
	(void)c;
}
void callout_run_expired(u_int32_t now)
{
	(void)now;
}
void krandom_stir(u_int64_t sample)
{
	(void)sample;
}
void endTask(pidType id)
{
	(void)id;
	/* Mark the calling task ZOMBIE so the scheduler stops dispatching it; the
	 * caller then sched()s away.  Full reaping (free its address space + kernel
	 * stack) is not ported on x86_64 yet — a terminated bring-up task leaks until
	 * the real exit/wait path lands (5d/5e). */
	if (_current != 0)
		_current->state = ZOMBIE;
}
unsigned smp_cpu_count(void)
{
	return 1;
}
int sys_shutdown(shutdownCMD_t cmd)
{
	(void)cmd;
	serial_puts("sys_shutdown: halting.\n");
	for (;;)
		__asm__ __volatile__("cli; hlt");
}

/* --- strncpy (string.h; the freestanding build provides no libc) ------------- */
char *strncpy(char *dst, const char *src, unsigned long n)
{
	unsigned long i = 0;
	for (; i < n && src[i] != '\0'; i++)
		dst[i] = src[i];
	for (; i < n; i++)
		dst[i] = '\0';
	return dst;
}

/* --- atomic spinlock (x86 xchg via the gcc builtin) --------------------------- */

void spinLockInit(spinLock_t lock)
{
	if (lock != 0)
		lock->locked = 0;
}

void spinLock(spinLock_t lock)
{
	if (lock == 0)
		return;
	while (__sync_lock_test_and_set(&lock->locked, 1) != 0)
		__asm__ __volatile__("pause");
}

void spinUnlock(spinLock_t lock)
{
	if (lock != 0)
		__sync_lock_release(&lock->locked);
}

int spinTryLock(spinLock_t lock)
{
	if (lock == 0)
		return 0;
	return __sync_lock_test_and_set(&lock->locked, 1) != 0 ? 1 : 0;
}

/* --- panic / assert ----------------------------------------------------------- */

void kpanic(const char *fmt, ...)
{
	serial_puts("\nKERNEL PANIC: ");
	serial_puts(fmt);
	serial_puts("\n");
	for (;;)
		__asm__ __volatile__("cli; hlt");
}

void __assert(const char *func, const char *file, int line, const char *e)
{
	(void)line;
	serial_puts("\nassert failed: ");
	serial_puts(e);
	serial_puts(" in ");
	serial_puts(func);
	serial_puts(" (");
	serial_puts(file);
	serial_puts(")\n");
	for (;;)
		__asm__ __volatile__("cli; hlt");
}

/* --- minimal kprintf (bring-up; %s %c %d %u %x %p %l*) ------------------------- */

int kprintf(const char *fmt, ...)
{
	__builtin_va_list ap;
	__builtin_va_start(ap, fmt);
	for (const char *p = fmt; *p != '\0'; p++)
	{
		if (*p != '%')
		{
			if (*p == '\n')
				serial_putc('\r');
			serial_putc(*p);
			continue;
		}
		p++;
		int lng = 0;
		while (*p == 'l')
		{
			lng++;
			p++;
		}
		switch (*p)
		{
			case 's':
				serial_puts(__builtin_va_arg(ap, const char *));
				break;
			case 'c':
				serial_putc((char)__builtin_va_arg(ap, int));
				break;
			case 'u':
				serial_putdec(lng ? __builtin_va_arg(ap, unsigned long)
				                  : __builtin_va_arg(ap, unsigned));
				break;
			case 'd':
			case 'i':
				serial_putdec(lng ? (u64) __builtin_va_arg(ap, long) : (u64) __builtin_va_arg(ap, int));
				break;
			case 'x':
			case 'X':
				serial_puthex(lng ? __builtin_va_arg(ap, unsigned long)
				                  : __builtin_va_arg(ap, unsigned));
				break;
			case 'p':
				serial_puthex((u64) __builtin_va_arg(ap, void *));
				break;
			case '%':
				serial_putc('%');
				break;
			default:
				serial_putc('%');
				serial_putc(*p);
				break;
		}
	}
	__builtin_va_end(ap);
	return 0;
}

/* --- freestanding mem ops (the compiler emits calls to these) ----------------- */

void *memset(void *dst, int c, unsigned long n)
{
	unsigned char *p = (unsigned char *)dst;
	while (n-- != 0)
		*p++ = (unsigned char)c;
	return dst;
}

void *memcpy(void *dst, const void *src, unsigned long n)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	while (n-- != 0)
		*d++ = *s++;
	return dst;
}

int memcmp(const void *a, const void *b, unsigned long n)
{
	const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
	for (; n-- != 0; x++, y++)
		if (*x != *y)
			return (int)*x - (int)*y;
	return 0;
}
