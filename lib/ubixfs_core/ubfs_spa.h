/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS SPA (Storage Pool Allocator) — the pool layer: vdev labels, the
 * uberblock ring, transaction-group commit, and block allocation.  Block I/O is
 * injected (dependency inversion) so the same code runs under the host tools and
 * the kernel.  See docs/design/ubixfs-pool-plan.md.
 *
 * v1 simplifications (flagged for later phases):
 *   - single vdev; one (front) label — back-label redundancy is a hardening pass.
 *   - allocation is a fixed on-disk bitmap (space-maps-in-the-CoW-tree later),
 *     flushed in-place on commit.  Frees are DEFERRED one txg (released only
 *     after the uberblock flip lands), which makes the in-place flush crash-safe:
 *     a torn commit can at worst leak blocks, never hand out a block a still-
 *     adoptable root references.
 */
#ifndef _UBIXFS_SPA_H
#define _UBIXFS_SPA_H

#include <fs/ubixfs/ondisk.h>

/* Injected block I/O over one vdev.  read/write move exactly one
 * UBFS_BLOCK_SIZE block; return 0 on success, negative on error. */
typedef struct ubfs_vdev_io
{
	void *ctx;
	int (*read)(void *ctx, uint64_t blk, void *buf);
	int (*write)(void *ctx, uint64_t blk, const void *buf);
	uint64_t size_blocks; /* total vdev size in UBFS_BLOCK_SIZE blocks */
} ubfs_vdev_io_t;

typedef struct ubfs_pool ubfs_pool_t; /* opaque handle */

/* ── lifecycle ──────────────────────────────────────────────────────────────*/

/**
 * Format a fresh pool on the vdev (writes label, zeroes the uberblock ring +
 * bitmap, commits txg 1 with an empty root).  `guid_seed` seeds the pool/vdev
 * GUIDs (pass a clock value); v1 GUIDs are placeholders.
 * @return 0 on success, negative on error.
 */
int ubfs_pool_format(const ubfs_vdev_io_t *io, const char *name, uint64_t guid_seed, ubfs_pool_t **out);

/**
 * Open an existing pool: read the label, scan the uberblock ring, adopt the
 * valid uberblock with the highest txg (crash recovery).  @return 0 or negative.
 */
int ubfs_pool_open(const ubfs_vdev_io_t *io, ubfs_pool_t **out);

void ubfs_pool_close(ubfs_pool_t *p);

/**
 * OR `bits` into the pool's feature_incompat mask and persist the vdev config.
 * Idempotent.  Used to mark a pool as using an incompatible feature (e.g. large
 * blocks) so older readers refuse it.  @return 0 on success, negative on error.
 */
int ubfs_pool_set_incompat(ubfs_pool_t *p, uint64_t bits);

/* ── transaction commit ─────────────────────────────────────────────────────*/

/**
 * Commit the next transaction group: flush allocation state, then write a fresh
 * uberblock (txg+1) carrying `rootbp` (the MOS root; pass NULL/zero while no
 * objects exist) into the ring slot for the new txg.  The uberblock flip is the
 * atomic commit point.  @return 0 or negative.
 */
int ubfs_pool_commit(ubfs_pool_t *p, const ubfs_blkptr_t *rootbp);

/* ── accessors ──────────────────────────────────────────────────────────────*/
uint64_t ubfs_pool_txg(const ubfs_pool_t *p);
const ubfs_uberblock_t *ubfs_pool_uberblock(const ubfs_pool_t *p);
const ubfs_vdev_config_t *ubfs_pool_config(const ubfs_pool_t *p);
uint64_t ubfs_pool_free_blocks(const ubfs_pool_t *p);

/* ── block allocation ───────────────────────────────────────────────────────*/

/** Allocate one block; returns its block number, or 0 if the pool is full
 *  (block 0 is the label, so 0 is a safe "none" sentinel). */
uint64_t ubfs_alloc_block(ubfs_pool_t *p);

/** Allocate `n` *contiguous* blocks (a run); returns the first block number, or
 *  0 if no contiguous free run of that length exists.  Backs variable-size
 *  records (a >4 KiB leaf occupies one run).  `n == 1` is the plain block path. */
uint64_t ubfs_alloc_run(ubfs_pool_t *p, uint64_t n);

/** Free one block.  THE single free chokepoint (snapshot hook #3): a future
 *  snapshot-aware version changes only this function.  The free is DEFERRED —
 *  the block becomes reallocatable only after the next ubfs_pool_commit()
 *  lands its uberblock, so an in-flight txg can never overwrite blocks the
 *  previous committed root still references. */
void ubfs_free_block(ubfs_pool_t *p, uint64_t blk);

/** Free a run of `n` contiguous blocks starting at `blk` (via the chokepoint). */
void ubfs_free_run(ubfs_pool_t *p, uint64_t blk, uint64_t n);

/* ── low-level block I/O (used by upper layers: DMU etc.) ───────────────────*/
int ubfs_read_block(ubfs_pool_t *p, uint64_t blk, void *buf);
int ubfs_write_block(ubfs_pool_t *p, uint64_t blk, const void *buf);

#endif /* _UBIXFS_SPA_H */
