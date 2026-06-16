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
#include <sys/thread.h>  /* struct thread, O_FILES (getfd) */
#include <sys/descrip.h> /* getfd decl + g_* hook decls */
#include <lib/kconsole.h>

/* Reboot-countdown flag the scheduler tick checks (set by the kbd Ctrl-Alt-Del
 * affordance on i386; inactive here — no MI provider linked for x86_64). */
volatile u_int32_t reboot_at_tick = 0;

/* NOTE: systemVitals, the g_device_find + g_fs + g_tty hooks, getfd, klog,
 * the callout helpers, krandom_stir, endTask and smp_cpu_count are now provided
 * by the real MI files (vitals.c, descrip.c, klog.c, callout.c, random.c,
 * endtask.c, cpu_enum.c) linked for the full POSIX syscall table; the bring-up
 * stubs that used to live here were removed. */

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

/* --- strcpy (used by the UbixFS pool driver's directory code) ----------------- */
char *strcpy(char *dst, const char *src)
{
	char *d = dst;
	while ((*d++ = *src++) != '\0')
		;
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

/* --- console: register COM1 as a kconsole sink so the real kprintf.c routes
 * here (graduated from the bring-up stub kprintf).  x86_64_console_init() must
 * run before the first kprintf. --------------------------------------------- */

static void kc_serial_putc(int c)
{
	if (c == '\n')
		serial_putc('\r'); /* serial sink owns CR/LF expansion */
	serial_putc((char)c);
}

static struct kconsole g_kc_serial = {kc_serial_putc, "com1", KC_SERIAL, 0};

void x86_64_console_init(void)
{
	kconsole_register(&g_kc_serial);
}

/* --- string ops the freestanding build needs (no arch lib/string.c yet) ------- */

int strcmp(const char *a, const char *b)
{
	while (*a != '\0' && *a == *b)
	{
		a++;
		b++;
	}
	return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, unsigned long n)
{
	for (; n != 0; n--, a++, b++)
	{
		if (*a != *b)
			return (int)(unsigned char)*a - (int)(unsigned char)*b;
		if (*a == '\0')
			break;
	}
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
