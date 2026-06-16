/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 syscall-surface stubs (Phase 5e).  The MI POSIX table references the
 * entire syscall surface; the implementations that still need x86_64 machine-
 * dependent glue -- exec/fork, signals, sockets (lwIP), pty/tty, the display/
 * input devices -- are stubbed here so the table links and the implemented calls
 * dispatch through their real code.  The mmap/VM + TLS calls graduated to
 * syscall_md.c.  Each stub is replaced as its subsystem is brought up; K&R empty
 * parens => the linker resolves by name, the (unexercised) callers' prototypes
 * drive the ABI.
 */

/* ubx_device_find (the devfs char/block lookup that delegates to g_device_find)
 * is the real implementation in dev/ac97.c — it needs <sys/bus.h>/<sys/descrip.h>
 * whose prototypes conflict with this file's deliberately-loose K&R stubs. */
long inportByte()
{
	return -1;
}
long outportByteP()
{
	return -1;
}
long sys_fork()
{
	return -1;
}
long sys_getcwd()
{
	return -1;
}
long sys_getvfscwd()
{
	return -1;
}
long sys_klog_read()
{
	return -1;
}
long sys_net_configure()
{
	return -1;
}
long sys_rfork()
{
	return -1;
}
/* sched_yield(2): a real implementation, NOT a stub — the cooperative scheduler
 * depends on it (e.g. login polling for authd's MPI reply must hand the CPU to
 * authd, or it spins and times out).  The MI sched_yield() re-dispatches. */
long sys_sched_yield()
{
	extern void sched_yield(void);
	sched_yield();
	return 0;
}
long sys_sysctl()
{
	return -1;
}
long sys_thread_exit_unmap()
{
	return -1;
}
long sys_ubfs_query()
{
	return -1;
}
long sys_vesa_modes()
{
	return -1;
}
long vmm_clean_virtual_space()
{
	return -1;
}

/* mmap/mmap2/brk are intercepted by x86_64_syscall (which returns the 64-bit VA
 * directly — td_retval is 32-bit), so these table entries are unreached; present
 * only so the POSIX table links. */
long sys_mmap()
{
	return -1;
}
long sys_mmap2()
{
	return -1;
}
long obreak()
{
	return -1;
}
