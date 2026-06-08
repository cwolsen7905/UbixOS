/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ramfs — in-memory filesystem (architecture-neutral).  Two roles:
 *   - the early-boot initramfs root: embed the boot binaries, mount ramfs as
 *     "/", populate it, then exec /bin/init (no disk driver needed);
 *   - the permanent tmpfs for /tmp, /run, /dev/shm.
 * It is also the kernel's only writable non-FAT filesystem, so it exercises the
 * generic VFS write/create/mkdir path independent of any block device.
 */
#ifndef _FS_RAMFS_H
#define _FS_RAMFS_H

#include <sys/types.h>

/**
 * Register the ramfs driver with the VFS (vfsRegisterFS).  Call once at init.
 *
 * @return 0 on success.
 */
int ramfs_init(void);

/**
 * Populate a file into the ramfs mounted at @mountPoint, creating parent
 * directories as needed and copying @len bytes of @data into it.  Used to lay
 * down the initramfs contents after mounting ramfs as root.
 *
 * @param path  absolute path within the mount (e.g. "/bin/init").
 * @return 0 on success, -1 on failure (no mount / OOM).
 */
int ramfs_populate(const char *mountPoint, const char *path, const void *data, u_int32_t len);

#endif /* _FS_RAMFS_H */
