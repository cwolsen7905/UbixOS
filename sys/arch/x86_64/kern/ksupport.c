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

/* System vitals (sysTicks + the VFS fileSystems/mountPoints list heads).  The MI
 * VFS keeps its lists inside this struct, so it must be real memory, not NULL.
 * A static zero-initialised instance suffices for bring-up (vitals.c not linked). */
static vitalsNode g_vitals;
vitalsNode *systemVitals = &g_vitals;

/* Reboot-countdown flag the scheduler tick checks (set by the kbd Ctrl-Alt-Del
 * affordance on i386; inactive here). */
volatile u_int32_t reboot_at_tick = 0;

/* VFS/FAT function-pointer hooks normally defined in descrip.c (too heavy to link
 * for bring-up — it pulls the isa/tty/ioctl registry).  virtio_blk installs
 * g_device_find; fat_init installs the rename/truncate shortcuts. */
void *(*g_device_find)(int major, int minor) = 0;
int (*g_fs_rename)(void *fs, const char *src, const char *dst) = 0;
int (*g_fs_truncate)(void *file, u_int32_t length) = 0;

/* TTY hooks file.c uses only when writing to a terminal device node; NULL here
 * (no tty on x86_64 yet) — the file/dir read path never dereferences them. */
void (*g_tty_print)(const char *buf, void *term) = 0;
int (*g_tty_getchar)(void) = 0;

/**
 * Resolve a file descriptor to its open-file pointer (minimal version of
 * descrip.c's getfd, which is too heavy to link for bring-up).
 * @return 0 on success with *fp set, -1 on a bad/closed fd.
 */
int getfd(struct thread *td, struct file **fp, int fd)
{
	if (fd < 0 || fd >= O_FILES)
	{
		*fp = 0;
		return -1;
	}
	*fp = (struct file *)td->o_files[fd];
	return (*fp == 0) ? -1 : 0;
}

/** Kernel log sink — not wired on x86_64 yet (the klog ring buffer is unlinked). */
void klog(u_int8_t level, const char *fmt, ...)
{
	(void)level;
	(void)fmt;
}

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
