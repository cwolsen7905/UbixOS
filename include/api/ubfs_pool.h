/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS pool/dataset introspection — the uBixOS-native API behind the in-OS
 * `ubpool` and `ubfs` commands (the zpool/zfs analog).  Pool management is
 * OS-specific, so this uses the uBixOS native ABI (int $0x81), not the FreeBSD
 * POSIX table.  The kernel fills an array of these records from its mounted
 * UbixFS pools (see sys/fs/ubixfs/ubfs_vfs.c).
 */
#ifndef _API_UBFS_POOL_H
#define _API_UBFS_POOL_H

#include <stdint.h>

/* Native syscall slot (int $0x81) for the pool/dataset query. */
#define UBIX_SYS_POOL_QUERY 67

/* flags */
#define UBIX_POOL_RAW 0x1 /* backed by a raw block-device partition (else loopback file) */
#define UBIX_POOL_RW 0x2  /* mounted read-write */

/*
 * One mounted UbixFS pool + its root dataset.  Fixed-width fields only, no
 * pointers, so the layout is identical for the i386 (32-bit) and aarch64
 * (64-bit) ABIs — the kernel and userland share this struct verbatim.
 */
struct ubix_pool_info
{
	char pool[64];         /* pool name (from the vdev label) */
	char mountpoint[64];   /* where the kernel mounted it */
	char dataset[64];      /* root dataset name */
	uint64_t total_blocks; /* pool capacity in 4 KB blocks */
	uint64_t free_blocks;  /* free 4 KB blocks */
	uint64_t dataset_used; /* bytes used by the root dataset */
	uint32_t block_size;   /* bytes per pool block (4096) */
	uint32_t flags;        /* UBIX_POOL_RAW | UBIX_POOL_RW */
};

/**
 * Fill @buf with up to @max mounted-pool records.
 *
 * @return the number of pools written (>= 0), or -1 on error.
 */
int ubix_pool_query(struct ubix_pool_info *buf, uint32_t max);

#endif /* _API_UBFS_POOL_H */
