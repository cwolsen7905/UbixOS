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

/* Pointer-returning: callers dereference the result, so return NULL. */
void *ubx_device_find()
{
	return 0;
}
long execThread()
{
	return -1;
}
long inportByte()
{
	return -1;
}
long md_disk_list()
{
	return -1;
}
long md_resident_pages()
{
	return -1;
}
long outportByteP()
{
	return -1;
}
long sys_accept()
{
	return -1;
}
long sys_bind()
{
	return -1;
}
long sys_connect()
{
	return -1;
}
long sys_disk_query()
{
	return -1;
}
long sys_fbpresent()
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
long sys_getkbd()
{
	return -1;
}
long sys_getmouse()
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
long sys_listen()
{
	return -1;
}
long sys_mapfb()
{
	return -1;
}
long sys_nanosleep()
{
	return -1;
}
long sys_net_configure()
{
	return -1;
}
long sys_ptyalloc()
{
	return -1;
}
long sys_ptyfree()
{
	return -1;
}
long sys_ptyinject()
{
	return -1;
}
long sys_ptyresize()
{
	return -1;
}
long sys_ptysnap()
{
	return -1;
}
long sys_recvfrom()
{
	return -1;
}
long sys_recvmsg()
{
	return -1;
}
long sys_rfork()
{
	return -1;
}
long sys_sched_yield()
{
	return -1;
}
long sys_sendmsg()
{
	return -1;
}
long sys_sendto()
{
	return -1;
}
long sys_setsockopt()
{
	return -1;
}
long sys_settty()
{
	return -1;
}
long sys_shareregion()
{
	return -1;
}
long sys_socket()
{
	return -1;
}
long sys_sysctl()
{
	return -1;
}
long sys_sysinfo()
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
long tty_hangup_by_owner()
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
