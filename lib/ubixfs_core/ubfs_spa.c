/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS SPA implementation — pool format/open, the uberblock ring + txg commit
 * (the atomic commit point and crash-recovery mechanism), and the block
 * allocator.  See ubfs_spa.h and docs/design/ubixfs-pool-plan.md.
 *
 * Portability note: this uses malloc/free for the pool handle + the in-memory
 * allocation bitmap.  The kernel port swaps these for kmalloc/kfree (or a tiny
 * ubfs_mem shim); everything else is freestanding (memcpy/memset only).
 */
#include "ubfs_spa.h"
#include "fletcher.h"
#include <stdlib.h>
#include <string.h>

#define LABEL_BLOCKS (UBFS_LABEL_SIZE / UBFS_BLOCK_SIZE) /* 33: config(1) + ring(32) */
#define BITS_PER_BLOCK ((uint64_t)UBFS_BLOCK_SIZE * 8)

struct ubfs_pool
{
	ubfs_vdev_io_t     io;
	ubfs_vdev_config_t config;
	ubfs_uberblock_t   ub;           /* current (highest-txg, valid) uberblock */
	uint64_t          txg;          /* == ub.txg */
	uint8_t          *bitmap;       /* in-memory allocation bitmap */
	uint64_t          bitmap_start; /* first bitmap block */
	uint64_t          bitmap_blocks;
	uint64_t          first_data;   /* first allocatable block */
	uint64_t          alloc_hint;
	uint64_t          used_blocks;
};

/* ── uberblock ring geometry ────────────────────────────────────────────────*/

static void ub_slot_loc(uint32_t slot, uint64_t *blk, uint32_t *off)
{
	uint64_t byte = (uint64_t)UBFS_BLOCK_SIZE + (uint64_t)slot * UBFS_UBERBLOCK_SIZE;
	*blk = byte / UBFS_BLOCK_SIZE;
	*off = (uint32_t)(byte % UBFS_BLOCK_SIZE);
}

static int ub_read(ubfs_pool_t *p, uint32_t slot, ubfs_uberblock_t *out)
{
	uint8_t  block[UBFS_BLOCK_SIZE];
	uint64_t blk;
	uint32_t off;

	ub_slot_loc(slot, &blk, &off);
	if (p->io.read(p->io.ctx, blk, block) < 0)
		return -1;
	memcpy(out, block + off, sizeof(*out));
	return 0;
}

static int ub_write(ubfs_pool_t *p, uint32_t slot, const ubfs_uberblock_t *ub)
{
	uint8_t  block[UBFS_BLOCK_SIZE];
	uint64_t blk;
	uint32_t off;

	ub_slot_loc(slot, &blk, &off);
	if (p->io.read(p->io.ctx, blk, block) < 0) /* read-modify-write (4 UBs/block) */
		return -1;
	memcpy(block + off, ub, sizeof(*ub));
	return p->io.write(p->io.ctx, blk, block);
}

/* Self-checksum: Fletcher-4 over the uberblock with its checksum field zeroed. */
static void ub_checksum(const ubfs_uberblock_t *ub, uint64_t out[4])
{
	ubfs_uberblock_t tmp = *ub;

	memset(tmp.checksum, 0, sizeof(tmp.checksum));
	ubfs_fletcher4(&tmp, sizeof(tmp), out);
}

static int ub_valid(const ubfs_uberblock_t *ub)
{
	uint64_t ck[4];

	if (ub->magic != UBFS_MAGIC || ub->version != UBFS_VERSION)
		return 0;
	ub_checksum(ub, ck);
	return memcmp(ck, ub->checksum, sizeof(ck)) == 0;
}

/* ── allocation bitmap ──────────────────────────────────────────────────────*/

static int bitmap_get(ubfs_pool_t *p, uint64_t blk)
{
	return (p->bitmap[blk / 8] >> (blk % 8)) & 1;
}

static void bitmap_put(ubfs_pool_t *p, uint64_t blk, int val)
{
	if (val)
		p->bitmap[blk / 8] |= (uint8_t)(1u << (blk % 8));
	else
		p->bitmap[blk / 8] &= (uint8_t)~(1u << (blk % 8));
}

static int bitmap_flush(ubfs_pool_t *p)
{
	uint64_t i;

	for (i = 0; i < p->bitmap_blocks; i++)
		if (p->io.write(p->io.ctx, p->bitmap_start + i, p->bitmap + i * UBFS_BLOCK_SIZE) < 0)
			return -1;
	return 0;
}

/* ── layout (derived deterministically from the vdev size) ──────────────────*/

static void compute_layout(ubfs_pool_t *p, uint64_t asize)
{
	p->bitmap_start = LABEL_BLOCKS;
	p->bitmap_blocks = (asize + BITS_PER_BLOCK - 1) / BITS_PER_BLOCK;
	p->first_data = p->bitmap_start + p->bitmap_blocks;
	p->alloc_hint = p->first_data;
}

/* ── uberblock write (the commit primitive) ─────────────────────────────────*/

static int write_uberblock(ubfs_pool_t *p, uint64_t txg, const ubfs_blkptr_t *rootbp)
{
	ubfs_uberblock_t ub;

	memset(&ub, 0, sizeof(ub));
	ub.magic = UBFS_MAGIC;
	ub.version = UBFS_VERSION;
	ub.txg = txg;
	ub.timestamp = 0; /* core has no clock; caller can stamp later */
	if (rootbp)
		ub.rootbp = *rootbp;
	ub_checksum(&ub, ub.checksum);

	if (ub_write(p, (uint32_t)(txg % UBFS_NUBERBLOCKS), &ub) < 0)
		return -1;
	p->ub = ub;
	p->txg = txg;
	return 0;
}

/* ── public API ─────────────────────────────────────────────────────────────*/

int ubfs_pool_format(const ubfs_vdev_io_t *io, const char *name, uint64_t guid_seed, ubfs_pool_t **out)
{
	ubfs_pool_t *p;
	uint8_t     zero[UBFS_BLOCK_SIZE];
	uint8_t     cfgblk[UBFS_BLOCK_SIZE];
	uint64_t    i;

	if (io->size_blocks < LABEL_BLOCKS + 8)
		return -1; /* too small */

	p = (ubfs_pool_t *)malloc(sizeof(*p));
	if (!p)
		return -1;
	memset(p, 0, sizeof(*p));
	p->io = *io;

	compute_layout(p, io->size_blocks);

	p->bitmap = (uint8_t *)malloc(p->bitmap_blocks * UBFS_BLOCK_SIZE);
	if (!p->bitmap)
	{
		free(p);
		return -1;
	}
	memset(p->bitmap, 0, p->bitmap_blocks * UBFS_BLOCK_SIZE);

	/* Reserve label + bitmap region. */
	for (i = 0; i < p->first_data; i++)
		bitmap_put(p, i, 1);
	p->used_blocks = p->first_data;

	/* Build + write the vdev config (block 0). */
	memset(&p->config, 0, sizeof(p->config));
	p->config.magic = UBFS_MAGIC;
	p->config.version = UBFS_VERSION;
	p->config.pool_guid = guid_seed ? guid_seed : 1;
	p->config.vdev_guid = (guid_seed ? guid_seed : 1) ^ 0x5642ULL;
	p->config.txg = 1;
	p->config.asize = io->size_blocks;
	p->config.ashift = UBFS_BLOCK_SHIFT;
	p->config.nlabels = 1; /* v1: front label only */
	if (name)
		strncpy(p->config.name, name, sizeof(p->config.name) - 1);
	memset(cfgblk, 0, sizeof(cfgblk));
	memcpy(cfgblk, &p->config, sizeof(p->config));
	if (io->write(io->ctx, 0, cfgblk) < 0)
		goto fail;

	/* Zero the uberblock ring. */
	memset(zero, 0, sizeof(zero));
	for (i = 1; i < LABEL_BLOCKS; i++)
		if (io->write(io->ctx, i, zero) < 0)
			goto fail;

	if (bitmap_flush(p) < 0)
		goto fail;

	/* Commit txg 1 with an empty root (no MOS objects yet). */
	if (write_uberblock(p, 1, NULL) < 0)
		goto fail;

	*out = p;
	return 0;

fail:
	free(p->bitmap);
	free(p);
	return -1;
}

int ubfs_pool_open(const ubfs_vdev_io_t *io, ubfs_pool_t **out)
{
	ubfs_pool_t *p;
	uint8_t     cfgblk[UBFS_BLOCK_SIZE];
	uint64_t    i;
	int         have = 0;
	uint32_t    slot;

	p = (ubfs_pool_t *)malloc(sizeof(*p));
	if (!p)
		return -1;
	memset(p, 0, sizeof(*p));
	p->io = *io;

	if (io->read(io->ctx, 0, cfgblk) < 0)
		goto fail;
	memcpy(&p->config, cfgblk, sizeof(p->config));
	if (p->config.magic != UBFS_MAGIC || p->config.version != UBFS_VERSION)
		goto fail;

	compute_layout(p, p->config.asize);

	p->bitmap = (uint8_t *)malloc(p->bitmap_blocks * UBFS_BLOCK_SIZE);
	if (!p->bitmap)
		goto fail;
	for (i = 0; i < p->bitmap_blocks; i++)
		if (io->read(io->ctx, p->bitmap_start + i, p->bitmap + i * UBFS_BLOCK_SIZE) < 0)
			goto fail2;

	/* Count used blocks for free-space reporting. */
	p->used_blocks = 0;
	for (i = 0; i < p->config.asize; i++)
		if (bitmap_get(p, i))
			p->used_blocks++;

	/* Scan the uberblock ring; adopt the valid one with the highest txg. */
	for (slot = 0; slot < UBFS_NUBERBLOCKS; slot++)
	{
		ubfs_uberblock_t ub;
		if (ub_read(p, slot, &ub) < 0)
			continue;
		if (ub_valid(&ub) && (!have || ub.txg > p->ub.txg))
		{
			p->ub = ub;
			p->txg = ub.txg;
			have = 1;
		}
	}
	if (!have)
		goto fail2; /* no valid uberblock — pool unreadable */

	p->alloc_hint = p->first_data;
	*out = p;
	return 0;

fail2:
	free(p->bitmap);
fail:
	free(p);
	return -1;
}

void ubfs_pool_close(ubfs_pool_t *p)
{
	if (!p)
		return;
	free(p->bitmap);
	free(p);
}

int ubfs_pool_commit(ubfs_pool_t *p, const ubfs_blkptr_t *rootbp)
{
	if (bitmap_flush(p) < 0) /* v1: in-place (atomicity vs UB flip is a Phase-3 item) */
		return -1;
	return write_uberblock(p, p->txg + 1, rootbp);
}

uint64_t ubfs_pool_txg(const ubfs_pool_t *p)
{
	return p->txg;
}

const ubfs_uberblock_t *ubfs_pool_uberblock(const ubfs_pool_t *p)
{
	return &p->ub;
}

const ubfs_vdev_config_t *ubfs_pool_config(const ubfs_pool_t *p)
{
	return &p->config;
}

uint64_t ubfs_pool_free_blocks(const ubfs_pool_t *p)
{
	return p->config.asize - p->used_blocks;
}

uint64_t ubfs_alloc_block(ubfs_pool_t *p)
{
	uint64_t scanned = 0;
	uint64_t total = p->config.asize - p->first_data;
	uint64_t blk = p->alloc_hint;

	for (; scanned < total; scanned++)
	{
		if (blk >= p->config.asize)
			blk = p->first_data;
		if (!bitmap_get(p, blk))
		{
			bitmap_put(p, blk, 1);
			p->used_blocks++;
			p->alloc_hint = blk + 1;
			return blk;
		}
		blk++;
	}
	return 0; /* full */
}

void ubfs_free_block(ubfs_pool_t *p, uint64_t blk)
{
	/* The single free chokepoint (snapshot hook #3).  v1 frees immediately; a
	 * snapshot-aware version changes only this function (free unless a snapshot
	 * still references the block, by blkptr birth_txg). */
	if (blk < p->first_data || blk >= p->config.asize)
		return; /* never free label/bitmap or out-of-range */
	if (bitmap_get(p, blk))
	{
		bitmap_put(p, blk, 0);
		p->used_blocks--;
		if (blk < p->alloc_hint)
			p->alloc_hint = blk;
	}
}

int ubfs_read_block(ubfs_pool_t *p, uint64_t blk, void *buf)
{
	return p->io.read(p->io.ctx, blk, buf);
}

int ubfs_write_block(ubfs_pool_t *p, uint64_t blk, const void *buf)
{
	return p->io.write(p->io.ctx, blk, buf);
}
