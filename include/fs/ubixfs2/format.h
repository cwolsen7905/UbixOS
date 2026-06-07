/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS v2 on-disk format — THE single source of truth.
 *
 * Shared verbatim by all three readers of the format: the host tool suite
 * (tools/ubixfs2), the kernel driver (sys/fs/ubixfs2), and the GRUB module.
 * Plain C, fixed-width types, explicit little-endian (i386).  No behaviour here —
 * structures only.  See docs/design/ubixfs2-plan.md.
 *
 * BeFS-derived: allocation groups, blockRun extents, a dataStream of direct /
 * indirect / double-indirect extents, inline small_data, and POSIX ownership
 * (uid/gid/mode) built into the inode.
 */
#ifndef _FS_UBIXFS2_FORMAT_H
#define _FS_UBIXFS2_FORMAT_H

#include <stdint.h>

#define UBIXFS2_MAGIC1 0x32424655 /* "UFB2" */
#define UBIXFS2_MAGIC2 0x0B0B0B0B
#define UBIXFS2_BLOCK_SIZE 4096
#define UBIXFS2_NDIRECT 64
#define UBIXFS2_NAMELEN 256
#define UBIXFS2_INODE_MAGIC 0x3BBE0AD9

#define UBIXFS2_CLEAN 0x4E454C43 /* "CLEN" */
#define UBIXFS2_DIRTY 0x54524944 /* "DIRT" */

#define UBIXFS2_BYTE_ORDER_LE 0x1 /* little-endian (i386) — the only supported order */

/*
 * An extent: a run of `len` contiguous blocks starting at block `start` within
 * allocation group `ag`.  v0 uses a single AG, so ag is always 0 and the
 * absolute block number is `start`.
 */
typedef struct ubixfs2_blockrun
{
	int32_t  ag;
	uint16_t start;
	uint16_t len;
} ubixfs2_blockrun_t;

/* Superblock — block 0 of the volume. */
typedef struct ubixfs2_super
{
	char     name[32];      /* volume label */
	uint32_t magic1;        /* UBIXFS2_MAGIC1 */
	uint32_t byte_order;    /* UBIXFS2_BYTE_ORDER_LE */
	uint32_t block_size;    /* UBIXFS2_BLOCK_SIZE */
	uint32_t block_shift;   /* log2(block_size) */
	int64_t  num_blocks;    /* total blocks in the volume */
	int64_t  used_blocks;   /* allocated blocks */
	uint32_t blocks_per_ag; /* blocks per allocation group */
	uint32_t ag_shift;      /* log2(blocks_per_ag) */
	uint32_t num_ags;       /* number of allocation groups (v0: 1) */
	uint32_t last_used_ag;  /* allocation hint */
	int32_t  flags;         /* UBIXFS2_CLEAN / UBIXFS2_DIRTY */

	uint32_t bitmap_start; /* first block of the block bitmap */
	uint32_t bitmap_blocks; /* bitmap length in blocks */

	ubixfs2_blockrun_t log; /* journal area (phase 6; unused in v0) */
	int64_t            log_start;
	int64_t            log_end;

	ubixfs2_blockrun_t root_dir; /* inode address of "/" */
	uint32_t           magic2;   /* UBIXFS2_MAGIC2 */

	char pad[4096 - 32 - 4 * 8 - 8 * 2 - 4 * 5 - 4 - 4 * 2 - 12 - 8 * 2 - 12 - 4];
} ubixfs2_super_t;

/* A file's data map: direct extents, then indirect / double-indirect (v0: direct only). */
typedef struct ubixfs2_stream
{
	ubixfs2_blockrun_t direct[UBIXFS2_NDIRECT];
	ubixfs2_blockrun_t indirect;        /* unused in v0 */
	ubixfs2_blockrun_t double_indirect; /* unused in v0 */
	int64_t            size;            /* logical size in bytes */
} ubixfs2_stream_t;

/*
 * Inode — occupies exactly one block in v0, so an inode's address is a single
 * block (a blockrun with len==1; absolute block == inode_num.start).
 */
typedef struct ubixfs2_inode
{
	uint32_t           magic;                 /* UBIXFS2_INODE_MAGIC */
	ubixfs2_blockrun_t inode_num;             /* this inode's own block */
	ubixfs2_blockrun_t parent;                /* parent directory inode */
	char               name[UBIXFS2_NAMELEN]; /* entry name */
	uint32_t           uid;                   /* POSIX owner */
	uint32_t           gid;                   /* POSIX group */
	uint32_t           mode;                  /* S_IF* type | rwxrwxrwx perms */
	uint32_t           flags;
	uint32_t           type;
	int64_t            atime;
	int64_t            mtime;
	int64_t            ctime;
	ubixfs2_blockrun_t attributes; /* extended attrs / ACLs (phase 7; 0 in v0) */
	ubixfs2_stream_t   blocks;     /* file data extents */
	uint32_t           ref_count;
	/* Inline data: tiny files, or a directory's entry array (v0 dirs live here). */
	uint8_t small_data[3200];
} ubixfs2_inode_t;

/*
 * Directory layout inside small_data (v0): a small header followed by fixed-size
 * entries.  Overflowing this triggers the B+tree path (a later phase).
 */
#define UBIXFS2_DIRENT_NAMELEN 56
typedef struct ubixfs2_dirent
{
	uint32_t inode_block;                 /* 0 == free slot */
	char     name[UBIXFS2_DIRENT_NAMELEN]; /* NUL-terminated */
} ubixfs2_dirent_t; /* 60 bytes */

typedef struct ubixfs2_dirhdr
{
	uint32_t count; /* number of live entries */
} ubixfs2_dirhdr_t;

/* Max inline directory entries: (3200 - 4) / 60. */
#define UBIXFS2_INLINE_DIRENTS ((3200 - (int)sizeof(ubixfs2_dirhdr_t)) / (int)sizeof(ubixfs2_dirent_t))

#endif /* _FS_UBIXFS2_FORMAT_H */
