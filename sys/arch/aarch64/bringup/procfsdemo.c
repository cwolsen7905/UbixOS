/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Bring-up demo: prove the generic VFS file layer works on aarch64.
 *
 * After path B decoupled the syscall core + file.c layer from the tty/socket/
 * device subsystems, the whole VFS read path (descrip + mount + file +
 * vfs_calls + procfs, over the shared frame allocator + kmalloc) links into the
 * aarch64 kernel.  This exercises it end-to-end with no user process and no
 * disk: register procfs, mount it at /proc, then fopen()/fread() /proc/meminfo
 * and print it.  Output on the UART is the verification.
 */
#include "bringup.h"
#include <fs/vfs/vfs.h>
#include <fs/vfs/mount.h>
#include <fs/vfs/file.h>
#include <fs/procfs/procfs.h>

/**
 * Mount procfs and read /proc/meminfo through the generic VFS.
 */
void aarch64_procfs_demo(void)
{
	fileDescriptor_t *fd;
	char buf[512];
	size_t n;

	kprintf("procfs demo: registering + mounting procfs at /proc...\n");
	procfs_init();
	if (vfs_mount(0, 0, 0, VFS_TYPE_PROCFS, "/proc", "r") != 0)
	{
		kprintf("procfs demo: vfs_mount(/proc) failed; skipping.\n");
		return;
	}

	fd = fopen("/proc/meminfo", "r");
	if (fd == NULL)
	{
		kprintf("procfs demo: fopen(/proc/meminfo) failed.\n");
		return;
	}

	n = fread(buf, 1, sizeof(buf) - 1, fd);
	buf[(n < sizeof(buf)) ? n : (sizeof(buf) - 1)] = '\0';
	fclose(fd);

	kprintf("procfs demo: read %u bytes from /proc/meminfo via the VFS:\n", (unsigned)n);
	kprintf("%s\n", buf);
	kprintf("procfs demo: VFS file I/O works on aarch64.\n");
}
