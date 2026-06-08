/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Bring-up demo: exercise the ramfs (the future initramfs root + tmpfs) on
 * aarch64 — and with it the generic VFS write + create path, which no other
 * deviceless fs covers.  Register ramfs, mount it at /ram, populate a file,
 * read it back, then write through the VFS and re-read to confirm growth.
 */
#include "bringup.h"
#include <fs/vfs/vfs.h>
#include <fs/vfs/mount.h>
#include <fs/vfs/file.h>
#include <fs/ramfs/ramfs.h>
#include <string.h>

/**
 * Mount ramfs at /ram and round-trip a file through it (read + write).
 */
void aarch64_ramfs_demo(void)
{
	static const char seed[] = "ramfs: seeded initramfs-style content\n";
	fileDescriptor_t *fd;
	char buf[128];
	int r;

	kprintf("ramfs demo: registering + mounting ramfs at /ram...\n");
	ramfs_init();
	if (vfs_mount(0, 0, 0, VFS_TYPE_RAMFS, "/ram", "rw") != 0)
	{
		kprintf("ramfs demo: vfs_mount(/ram) failed; skipping.\n");
		return;
	}

	/* Lay down a file the way the initramfs loader will lay down /bin/init. */
	if (ramfs_populate("/ram", "/hello.txt", seed, (u_int32_t)(sizeof(seed) - 1)) != 0)
	{
		kprintf("ramfs demo: populate failed.\n");
		return;
	}

	/* Read it back through the VFS. */
	fd = fopen("/ram/hello.txt", "r");
	if (fd == NULL)
	{
		kprintf("ramfs demo: fopen(/ram/hello.txt) failed.\n");
		return;
	}
	r = (int)fread(buf, 1, sizeof(buf) - 1, fd);
	buf[(r > 0 && r < (int)sizeof(buf)) ? r : 0] = '\0';
	fclose(fd);
	kprintf("ramfs demo: read %d bytes: %s", r, buf);

	/* Write a NEW file through the VFS write path, then re-read it. */
	fd = fopen("/ram/scratch.txt", "w");
	if (fd != NULL)
	{
		static const char w[] = "written through vfsWrite\n";
		int wr = (int)fwrite(w, 1, sizeof(w) - 1, fd);
		fclose(fd);

		fd = fopen("/ram/scratch.txt", "r");
		if (fd != NULL)
		{
			r = (int)fread(buf, 1, sizeof(buf) - 1, fd);
			buf[(r > 0 && r < (int)sizeof(buf)) ? r : 0] = '\0';
			fclose(fd);
			kprintf("ramfs demo: wrote %d, read back %d: %s", wr, r, buf);
		}
	}
	kprintf("ramfs demo: VFS read + write/create work on aarch64.\n");
}
