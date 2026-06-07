/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS on-disk format — the single source of truth for the pooled
 * copy-on-write (lite-ZFS) filesystem.  Plain C, fixed-width, little-endian
 * (i386/aarch64).  Structures only — no behaviour.  Shared by the host tools,
 * the kernel driver, and (eventually) GRUB.  See docs/design/ubixfs-pool-plan.md.
 *
 * Heritage: a deliberately "lite" ZFS — pool + datasets, CoW, checksummed block
 * pointers, an uberblock/transaction-group commit.  Snapshots are DEFERRED but
 * the three hooks that keep them addable later are present from day one:
 *   (1) `birth_txg` in every block pointer,
 *   (2) uberblock -> MOS -> dataset -> objset indirection (never UB -> fs),
 *   (3) all block frees funnel through one path (a code rule, not a struct).
 */
#ifndef _FS_UBIXFS_ONDISK_H
#define _FS_UBIXFS_ONDISK_H

#include <stdint.h>

/* Compile-time assert that works in C99 (no C11 dependency). */
#define UFS_CTASSERT(name, cond) typedef char ufs_ctassert_##name[(cond) ? 1 : -1]

#define UFS_MAGIC 0x5346584942550a0dULL /* "\r\nUBIXFS" */
#define UFS_VERSION 1

#define UFS_BLOCK_SHIFT 12
#define UFS_BLOCK_SIZE (1u << UFS_BLOCK_SHIFT) /* 4096 — metadata block / min recordsize */
#define UFS_NAME_MAX 255

/* ── algorithm ids (recorded per block in the blkptr / dnode) ───────────────
 * Pluggable strategies (OCP): the reader dispatches on these ids, so new algos
 * are added without touching callers or breaking old blocks. */
enum ufs_checksum
{
	UFS_CK_OFF = 0,
	UFS_CK_FLETCHER4 = 1,
	UFS_CK_SHA256 = 2, /* reserved */
};
enum ufs_compress
{
	UFS_CO_OFF = 0,
	UFS_CO_LZ4 = 1,  /* reserved (added after the engine works) */
	UFS_CO_ZSTD = 2, /* reserved */
};

/* ── object / block content types ──────────────────────────────────────────*/
enum ufs_otype
{
	UFS_OT_NONE = 0,
	UFS_OT_OBJSET = 1,     /* an object-set header block */
	UFS_OT_DNODE = 2,      /* a metadnode's data: the dnode array */
	UFS_OT_INDIRECT = 3,   /* a block full of blkptrs (object data tree) */
	UFS_OT_PLAIN_FILE = 4, /* regular file data */
	UFS_OT_DIRECTORY = 5,  /* directory data (name -> object id) */
	UFS_OT_DATASET = 6,    /* a dataset object (bonus = ufs_dataset_phys_t) */
	UFS_OT_ZVOL = 7,       /* raw volume data */
};

/* Object-set types */
enum ufs_ostype
{
	UFS_OST_MOS = 0, /* the pool's meta-object-set (dataset directory) */
	UFS_OST_FS = 1,  /* a POSIX filesystem dataset */
	UFS_OST_ZVOL = 2 /* a raw block volume dataset */
};

/* Dataset types (ufs_dataset_phys_t.type) */
enum ufs_dstype
{
	UFS_DS_FS = 1,
	UFS_DS_ZVOL = 2
};

/* dnode bonus-buffer types */
enum ufs_btype
{
	UFS_BT_NONE = 0,
	UFS_BT_ZNODE = 1,   /* POSIX attributes (ufs_znode_t) */
	UFS_BT_DATASET = 2, /* ufs_dataset_phys_t */
};

/* ── DVA: where a block lives (vdev + block offset) ────────────────────────
 * offset is a 64-bit block number → no practical volume-size cap. */
typedef struct ufs_dva
{
	uint32_t vdev;
	uint32_t pad;
	uint64_t offset; /* in UFS_BLOCK_SIZE units */
} ufs_dva_t;

/* ── block pointer: the universal "pointer to a block" ─────────────────────
 * Carries integrity (checksum), compression, size, and — the snapshot hook —
 * the txg the block was born in.  Fixed 128 bytes. */
typedef struct ufs_blkptr
{
	ufs_dva_t dva;          /* 16 — where */
	uint64_t  birth_txg;    /* 8  — SNAPSHOT HOOK: txg this block was written */
	uint64_t  fill;         /* 8  — count of allocated blocks beneath this ptr */
	uint32_t  lsize;        /* 4  — logical (uncompressed) size in bytes */
	uint32_t  psize;        /* 4  — physical (on-disk, post-compression) size */
	uint8_t   type;         /* enum ufs_otype */
	uint8_t   level;        /* 0 = data block; >0 = indirect (blkptr) block */
	uint8_t   comp;         /* enum ufs_compress */
	uint8_t   cksum;        /* enum ufs_checksum */
	uint8_t   flags;        /* reserved */
	uint8_t   rsvd[3];      /* align */
	uint64_t  checksum[4];  /* 32 — 256-bit; algo per `cksum` */
	uint8_t   pad[48];      /* to 128 */
} ufs_blkptr_t;
UFS_CTASSERT(blkptr_128, sizeof(ufs_blkptr_t) == 128);

/* ── uberblock: the atomic commit point (one valid UB with the highest txg
 * wins on mount).  Written round-robin into the label's uberblock ring. ──── */
#define UFS_UBERBLOCK_SIZE 1024
typedef struct ufs_uberblock
{
	uint64_t     magic;        /* UFS_MAGIC */
	uint64_t     version;      /* UFS_VERSION */
	uint64_t     txg;          /* transaction group this UB commits */
	uint64_t     timestamp;    /* wall-clock seconds at commit */
	ufs_blkptr_t rootbp;       /* 128 — points at the MOS objset (NOT the fs) */
	uint64_t     checksum[4];  /* self-validating checksum over the UB */
	uint8_t      pad[UFS_UBERBLOCK_SIZE - 32 - 128 - 32];
} ufs_uberblock_t;
UFS_CTASSERT(uberblock_1024, sizeof(ufs_uberblock_t) == UFS_UBERBLOCK_SIZE);

/* ── vdev label: lets us find the pool; redundant copies at fixed offsets ───
 * Layout per label: [config block][uberblock ring].  v1 writes 2 labels (front
 * + back of the vdev); room reserved to grow to 4 like ZFS. */
#define UFS_UBERBLOCK_RING (128 * 1024)
#define UFS_NUBERBLOCKS (UFS_UBERBLOCK_RING / UFS_UBERBLOCK_SIZE) /* 128 */
#define UFS_LABEL_SIZE (UFS_BLOCK_SIZE + UFS_UBERBLOCK_RING)      /* config + ring */

typedef struct ufs_vdev_config
{
	uint64_t magic;           /* UFS_MAGIC */
	uint64_t version;         /* UFS_VERSION */
	uint64_t pool_guid;       /* identifies the pool (all vdevs share it) */
	uint64_t vdev_guid;       /* identifies this vdev */
	uint64_t txg;             /* txg at last label sync */
	uint64_t asize;           /* usable size of this vdev, in blocks */
	uint64_t feature_incompat; /* incompat feature bitmask (0 in v1) */
	uint64_t feature_ro_compat;
	uint32_t ashift;          /* log2(block size) = UFS_BLOCK_SHIFT */
	uint32_t nlabels;         /* how many label copies exist */
	char     name[64];        /* pool name */
	uint8_t  pad[UFS_BLOCK_SIZE - 8 * 8 - 4 * 2 - 64];
} ufs_vdev_config_t;
UFS_CTASSERT(vdev_config_blk, sizeof(ufs_vdev_config_t) == UFS_BLOCK_SIZE);

/* ── dnode: an on-disk object ──────────────────────────────────────────────
 * Object data is addressed by a CoW blkptr tree rooted at blkptr[0] (nlevels
 * deep).  The bonus buffer holds inline metadata: a znode for FS objects, or a
 * ufs_dataset_phys_t for dataset objects.  Fixed 512 bytes. */
#define UFS_DNODE_SIZE 512
#define UFS_DN_NBLKPTR 1 /* v1: one inline blkptr; bigger objects use indirection */
typedef struct ufs_dnode
{
	uint8_t      type;       /* enum ufs_otype */
	uint8_t      nlevels;    /* blkptr-tree depth (1 = blkptr[0] points at data) */
	uint8_t      nblkptr;    /* blkptrs present (UFS_DN_NBLKPTR) */
	uint8_t      bonustype;  /* enum ufs_btype */
	uint8_t      checksum;   /* data checksum algo */
	uint8_t      compress;   /* data compression algo */
	uint8_t      flags;
	uint8_t      pad8;
	uint32_t     datablksz;  /* data block size in bytes (variable recordsize) */
	uint16_t     bonuslen;   /* bytes of bonus in use */
	uint16_t     pad16;
	uint64_t     maxblkid;   /* highest present data block id (logical size cue) */
	uint64_t     used;       /* bytes allocated to this object */
	ufs_blkptr_t blkptr[UFS_DN_NBLKPTR]; /* 128 */
	uint8_t      bonus[UFS_DNODE_SIZE - 32 - 128 * UFS_DN_NBLKPTR]; /* 352 */
} ufs_dnode_t;
UFS_CTASSERT(dnode_512, sizeof(ufs_dnode_t) == UFS_DNODE_SIZE);

/* ── znode: POSIX attributes, stored in a file/dir dnode's bonus ───────────*/
typedef struct ufs_znode
{
	uint64_t mode;   /* S_IF* type | rwxrwxrwx */
	uint64_t size;   /* logical size in bytes */
	uint64_t uid;
	uint64_t gid;
	uint64_t nlink;  /* hardlink count */
	uint64_t atime;  /* seconds (nsec split is a later, flagged extension) */
	uint64_t mtime;
	uint64_t ctime;
	uint64_t crtime; /* birth time */
	uint64_t parent; /* parent directory object id */
	uint64_t xattr;  /* xattr-dir object id, 0 = none (ACLs ride here later) */
	uint64_t rdev;   /* device id for special files (0 otherwise) */
	uint64_t flags;
	uint64_t gen;    /* generation (object-reuse guard) */
} ufs_znode_t;
UFS_CTASSERT(znode_fits_bonus, sizeof(ufs_znode_t) <= UFS_DNODE_SIZE - 32 - 128 * UFS_DN_NBLKPTR);

/* ── dataset_phys: in a dataset object's bonus (in the MOS) ─────────────────
 * The uberblock -> MOS -> dataset -> objset indirection (snapshot hook #2)
 * lives here: `bp` points at this dataset's object-set root. */
typedef struct ufs_dataset_phys
{
	ufs_blkptr_t bp;          /* 128 — this dataset's objset root */
	uint64_t     type;        /* enum ufs_dstype */
	uint64_t     used;        /* bytes used by the dataset */
	uint64_t     recordsize;  /* property: data block size in bytes */
	uint64_t     volsize;     /* zvol: exported device size in bytes */
	uint32_t     compression; /* property: enum ufs_compress */
	uint32_t     atime;       /* property: update atimes? (bool) */
	char         name[64];
} ufs_dataset_phys_t;
UFS_CTASSERT(dataset_fits_bonus, sizeof(ufs_dataset_phys_t) <= UFS_DNODE_SIZE - 32 - 128 * UFS_DN_NBLKPTR);

/* ── objset: header for a collection of objects ────────────────────────────
 * `metadnode`'s data IS the dnode array — object N's dnode is at byte
 * N*sizeof(ufs_dnode_t) within it.  The MOS and every dataset is an objset. */
#define UFS_OBJSET_SIZE 1024
typedef struct ufs_objset
{
	ufs_dnode_t metadnode; /* 512 — its data = the dnode array */
	uint64_t    type;      /* enum ufs_ostype */
	uint64_t    flags;
	uint8_t     pad[UFS_OBJSET_SIZE - 512 - 16];
} ufs_objset_t;
UFS_CTASSERT(objset_1024, sizeof(ufs_objset_t) == UFS_OBJSET_SIZE);

/* ── directory entry (v1: fixed-size, linear) ──────────────────────────────
 * Directory object data is an array of these.  obj==0 marks a free slot.  A
 * scalable hash-directory is a later phase; the fixed form keeps v1 simple. */
#define UFS_DIRENT_SIZE 256
typedef struct ufs_dirent
{
	uint64_t obj;     /* target object id (0 = free slot) */
	uint8_t  type;    /* DT_* (file/dir/symlink) */
	uint8_t  namelen; /* bytes in name (excl. NUL) */
	uint8_t  pad[6];
	char     name[UFS_DIRENT_SIZE - 16]; /* 240; v1 max name 239 */
} ufs_dirent_t;
UFS_CTASSERT(dirent_256, sizeof(ufs_dirent_t) == UFS_DIRENT_SIZE);

/* Well-known object ids within an objset */
#define UFS_OBJ_ROOT 1 /* FS objset: the root directory object */

#endif /* _FS_UBIXFS_ONDISK_H */
