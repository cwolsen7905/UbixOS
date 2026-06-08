/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * ubfs_fs — UbixFS POSIX filesystem layer (the native files/dirs/permissions
 * layer over a filesystem dataset's object set; the rough equivalent of ZFS's
 * "ZPL", named neutrally since this is uBixOS's *native* fs model, not a compat
 * shim — uBixOS's `posix` namespace is the syscall-compat layer).  Maps POSIX
 * objects onto DMU objects: a file is an object whose data is its contents and
 * whose ubfs_inode bonus holds mode/uid/gid/times/size/nlink; a directory is an
 * object holding a name→object map (ubfs_dir).  See docs/design/ubixfs-pool-plan.md.
 *
 * v1 scope: root dir + files/dirs + symlinks; perms/owners/times; lookup,
 * read/write (size-tracked), readdir, unlink.  Hardlinks, rename, chmod/chown,
 * and object reclamation are the next slice.
 */
#ifndef _UBIXFS_FS_H
#define _UBIXFS_FS_H

#include "ubfs_dmu.h"
#include "ubfs_dir.h"

/* POSIX mode type bits (subset). */
#define UBFS_S_IFMT 0170000u
#define UBFS_S_IFREG 0100000u
#define UBFS_S_IFDIR 0040000u
#define UBFS_S_IFLNK 0120000u

/* dirent type tags (ubfs_dirent.type). */
#define UBFS_DT_REG 1
#define UBFS_DT_DIR 2
#define UBFS_DT_LNK 3

typedef struct ubfs_fs
{
	ubfs_dmu_os_t *os;  /* the filesystem dataset's object set */
	uint64_t       now; /* timestamp stamped into inode times (caller sets it) */
} ubfs_fs_t;

/** Wrap an (already opened) filesystem object set. */
void ubfs_fs_init(ubfs_fs_t *fs, ubfs_dmu_os_t *os, uint64_t now);

/** Create the root directory (object UBFS_OBJ_ROOT) of a fresh fs object set. */
int ubfs_fs_mkroot(ubfs_fs_t *fs, uint32_t uid, uint32_t gid);

/** Resolve an absolute path to its object number. */
int ubfs_fs_lookup(ubfs_fs_t *fs, const char *path, uint64_t *obj);

/** Create a directory / regular file / symlink; returns the new object in *obj. */
int ubfs_fs_mkdir(ubfs_fs_t *fs, const char *path, uint32_t mode, uint32_t uid, uint32_t gid, uint64_t *obj);
int ubfs_fs_create(ubfs_fs_t *fs, const char *path, uint32_t mode, uint32_t uid, uint32_t gid, uint64_t *obj);
int ubfs_fs_symlink(ubfs_fs_t *fs, const char *path, const char *target, uint32_t uid, uint32_t gid, uint64_t *obj);

/** File data I/O.  write extends size + stamps mtime; read clamps to size. */
int ubfs_fs_write(ubfs_fs_t *fs, uint64_t obj, uint64_t off, const void *buf, uint64_t len);
int ubfs_fs_read(ubfs_fs_t *fs, uint64_t obj, uint64_t off, void *buf, uint64_t len);

/** Read a symlink's target into buf (NUL-terminated, up to bufsz). */
int ubfs_fs_readlink(ubfs_fs_t *fs, uint64_t obj, char *buf, uint64_t bufsz);

/** Fetch an object's POSIX attributes (stat). */
int ubfs_fs_getattr(ubfs_fs_t *fs, uint64_t obj, ubfs_inode_t *out);

/** Iterate a directory's entries. */
int ubfs_fs_readdir(ubfs_fs_t *fs, uint64_t dirobj, ubfs_dir_cb cb, void *arg);

/** Remove a file/symlink/empty-dir entry from its parent. */
int ubfs_fs_unlink(ubfs_fs_t *fs, const char *path);

#endif /* _UBIXFS_FS_H */
