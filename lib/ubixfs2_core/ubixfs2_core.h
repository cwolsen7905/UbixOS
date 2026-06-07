/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * ubixfs2_core — block-I/O-agnostic UbixFS v2 logic, shared by the host tools,
 * the kernel driver, and (read layer) GRUB.  The caller injects block I/O via
 * callbacks and owns all storage; this library performs no I/O or allocation of
 * its own.  See docs/design/ubixfs2-plan.md.
 */
#ifndef _UBIXFS2_CORE_H
#define _UBIXFS2_CORE_H

#include <fs/ubixfs2/format.h>
#include <stddef.h>

/* Block I/O callbacks: return 0 on success, negative on error. */
typedef struct ubixfs2
{
	void *io_ctx;
	int (*read_block)(void *ctx, uint32_t blk, void *buf);
	int (*write_block)(void *ctx, uint32_t blk, const void *buf);
	ubixfs2_super_t super; /* cached on mount/format */
} ubixfs2_t;

/* ---- lifecycle ---- */
int ubixfs2_format(ubixfs2_t *fs, int64_t num_blocks, const char *label);
int ubixfs2_mount(ubixfs2_t *fs);
int ubixfs2_sync_super(ubixfs2_t *fs);

/* ---- inode I/O (an inode == one block; addr == block number) ---- */
int ubixfs2_read_inode(ubixfs2_t *fs, uint32_t blk, ubixfs2_inode_t *out);
int ubixfs2_write_inode(ubixfs2_t *fs, const ubixfs2_inode_t *in);

/* ---- path lookup ---- */
/* Resolve an absolute path to its inode block; returns 0 and sets *blk, or <0. */
int ubixfs2_lookup(ubixfs2_t *fs, const char *path, uint32_t *blk);

/* ---- directory iteration (read layer) ---- */
typedef int (*ubixfs2_dir_cb)(void *arg, const char *name, uint32_t inode_block);
int ubixfs2_readdir(ubixfs2_t *fs, const ubixfs2_inode_t *dir, ubixfs2_dir_cb cb, void *arg);

/* ---- file read (read layer) ---- */
/* Read up to len bytes at offset; returns bytes read or <0. */
int ubixfs2_read_file(ubixfs2_t *fs, const ubixfs2_inode_t *ino, void *buf, int64_t off, int64_t len);

/* ---- write layer (kernel + host tool; not GRUB) ---- */
int ubixfs2_mkdir(ubixfs2_t *fs, const char *path, uint32_t mode, uint32_t uid, uint32_t gid);
/* Create a regular file and write `len` bytes of `data` into it (v0: whole-file). */
int ubixfs2_create_file(ubixfs2_t *fs, const char *path, const void *data, int64_t len, uint32_t mode,
                        uint32_t uid, uint32_t gid);
int ubixfs2_unlink(ubixfs2_t *fs, const char *path);

#endif /* _UBIXFS2_CORE_H */
