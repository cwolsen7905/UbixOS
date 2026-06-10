/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS kernel VFS driver (the pooled copy-on-write / lite-ZFS filesystem).
 * The on-disk format + all pool/dataset/file logic live in the portable core
 * (lib/ubixfs_core); this driver is the thin VFS glue that registers the core
 * with sys/fs/vfs and drives it over a backing store.  Distinct from the legacy
 * v1 BeFS-style driver in the same directory (fs/ubixfs/ubixfs.h).  See
 * docs/design/ubixfs-pool-plan.md (steps K2+).
 */
#ifndef _FS_UBIXFS_UBFS_VFS_H
#define _FS_UBIXFS_UBFS_VFS_H

/**
 * Register the UbixFS driver (VFS_TYPE_UBIXFS) with the VFS.
 *
 * Call once at boot before vfs_mount().  K2 mounts a file-backed (loopback)
 * pool: the backing image is an ordinary file on an already-mounted filesystem
 * (default "/pool.img"), read 4 KB at a time through the kernel file API.
 *
 * @return 0 on success, non-zero if registration failed.
 */
int ubfs_vfs_init(void);

/**
 * Set the backing-image path used by the next UbixFS mount (default "/pool.img").
 */
void ubfs_vfs_set_backing(const char *path);

#endif /* _FS_UBIXFS_UBFS_VFS_H */
