/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 syscall-surface stubs (Phase 5e).  The MI POSIX table (syscalls_posix.c)
 * references the entire syscall surface; the implementations that need x86_64
 * machine-dependent glue not yet ported -- exec/fork, mmap/VM, signals, sockets
 * (lwIP), pty/tty, the display/input devices -- are stubbed here so the table
 * links and the *implemented* calls (file I/O, getpid, time, pipe, sem, MPI)
 * dispatch through their real MI code.  Each stub is replaced as its subsystem is
 * brought up on x86_64.  K&R empty parens => the linker resolves by name; the
 * (unexercised) callers' prototypes drive the ABI, the stubs ignore args.
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
long machine_set_tls()
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
long obreak()
{
	return -1;
}
long outportByteP()
{
	return -1;
}
long signal_deliver_frame()
{
	return -1;
}
long sysGetFreePage()
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
long sys_execve()
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
long sys_madvise()
{
	return -1;
}
long sys_mapfb()
{
	return -1;
}
long sys_mmap()
{
	return -1;
}
long sys_mmap2()
{
	return -1;
}
long sys_msync()
{
	return -1;
}
long sys_munmap()
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
long sys_sigreturn()
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
long vmm_set_page_attributes()
{
	return -1;
}
