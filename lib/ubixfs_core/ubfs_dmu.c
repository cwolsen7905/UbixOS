/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS DMU implementation — copy-on-write object engine.  See ubfs_dmu.h.
 *
 * An object's data lives in a radix block-pointer tree rooted at dnode.blkptr[0]
 * (height = nlevels-1; fan-out = 4096/128 = 32).  Leaves are *records* of
 * dnode.datablksz bytes (a multiple of 4 KiB, up to UBFS_RECORDSIZE_MAX); a
 * record occupies one *contiguous run* of 4 KiB blocks (ceil(psize/4096) of
 * them).  Indirect blocks, the dnode array, objset headers and directories
 * always use a single 4 KiB block.  Writes are CoW: a new record is written,
 * then every indirect block up to the dnode is rewritten and the old blocks
 * freed (through the SPA's single free chokepoint).  A zeroed block pointer
 * (dva.offset == 0) is a hole → reads as zeros (sparse).
 */
#include "ubfs_dmu.h"
#include "fletcher.h"
#include <stdlib.h>
#include <string.h>

#define BS UBFS_BLOCK_SIZE
#define FANOUT (UBFS_BLOCK_SIZE / (int)sizeof(ubfs_blkptr_t)) /* 32 */
#define FANOUT_SHIFT 5                                        /* log2(32) */

/* ── block-pointer helpers ──────────────────────────────────────────────────*/

static int bp_is_hole(const ubfs_blkptr_t *bp)
{
	return bp->dva.offset == 0; /* block 0 is the label — never real data */
}

/* Number of 4 KiB blocks a record of `psize` bytes occupies. */
static uint64_t leaf_nblocks(uint32_t psize)
{
	return (psize + BS - 1) / BS;
}

static void bp_free(ubfs_pool_t *pool, const ubfs_blkptr_t *bp)
{
	if (bp_is_hole(bp))
		return;
	/* A data record (level 0) spans a run; indirect blocks are a single block. */
	if (bp->level == 0)
		ubfs_free_run(pool, bp->dva.offset, leaf_nblocks(bp->psize));
	else
		ubfs_free_block(pool, bp->dva.offset); /* the single free chokepoint */
}

/* ── single-block (metadata/indirect) I/O with integrity ────────────────────*/

/* CoW-write one 4 KiB block; fills *out with a fresh, checksummed block pointer.
 * Used for indirect blocks and objset headers (always exactly one block). */
static int blk_write_cow(ubfs_pool_t *pool, const void *buf, uint8_t type, uint8_t level, ubfs_blkptr_t *out)
{
	uint64_t blk = ubfs_alloc_block(pool);

	if (blk == 0)
		return -1;
	if (ubfs_write_block(pool, blk, buf) < 0)
		return -1;
	memset(out, 0, sizeof(*out));
	out->dva.offset = blk;
	out->birth_txg = ubfs_pool_txg(pool) + 1; /* the txg currently being built */
	out->lsize = BS;
	out->psize = BS;
	out->type = type;
	out->level = level;
	out->comp = UBFS_CO_OFF;
	out->cksum = UBFS_CK_FLETCHER4;
	out->fill = 1;
	ubfs_fletcher4(buf, BS, out->checksum);
	return 0;
}

static int blk_read_verify(ubfs_pool_t *pool, const ubfs_blkptr_t *bp, void *buf)
{
	uint64_t ck[4];

	if (bp_is_hole(bp))
	{
		memset(buf, 0, BS);
		return 0;
	}
	if (ubfs_read_block(pool, bp->dva.offset, buf) < 0)
		return -1;
	ubfs_fletcher4(buf, BS, ck);
	if (memcmp(ck, bp->checksum, sizeof(ck)) != 0)
		return -2; /* checksum mismatch — corruption */
	return 0;
}

/* ── variable-size record (data leaf) I/O ───────────────────────────────────*/

/* CoW-write a record of `lsize` bytes.  `buf` must be readable for the whole run
 * (lsize..nblk*BS already zero-padded by the caller); a record occupies one
 * contiguous run.  Checksum covers the full on-disk run (multiple of 4 KiB). */
static int leaf_write_cow(ubfs_pool_t *pool, const void *buf, uint32_t lsize, uint8_t type, ubfs_blkptr_t *out)
{
	uint64_t nblk = leaf_nblocks(lsize);
	uint64_t run = ubfs_alloc_run(pool, nblk);

	if (run == 0)
		return -1;
	/* Any on-disk record larger than one block makes the pool incompatible with
	 * readers that predate large blocks — the authoritative place to mark it. */
	if (lsize > BS)
		ubfs_pool_set_incompat(pool, UBFS_FEAT_INCOMPAT_LARGE_BLOCKS);
	for (uint64_t i = 0; i < nblk; i++)
		if (ubfs_write_block(pool, run + i, (const uint8_t *)buf + i * BS) < 0)
			return -1;
	memset(out, 0, sizeof(*out));
	out->dva.offset = run;
	out->birth_txg = ubfs_pool_txg(pool) + 1;
	out->lsize = lsize;
	out->psize = lsize; /* == lsize until compression lands */
	out->type = type;
	out->level = 0;
	out->comp = UBFS_CO_OFF;
	out->cksum = UBFS_CK_FLETCHER4;
	out->fill = nblk;
	ubfs_fletcher4(buf, nblk * BS, out->checksum);
	return 0;
}

/* Read+verify a record into `buf` (capacity `cap` >= the record's on-disk run).
 * On success `buf[0..lsize)` holds the data and `buf[lsize..cap)` is zeroed.
 * @return the logical size (>= 0), or negative on error. */
static int leaf_read_verify(ubfs_pool_t *pool, const ubfs_blkptr_t *bp, uint8_t *buf, uint32_t cap)
{
	uint64_t nblk;
	uint64_t ck[4];

	if (bp_is_hole(bp))
	{
		memset(buf, 0, cap);
		return 0;
	}
	nblk = leaf_nblocks(bp->psize);
	for (uint64_t i = 0; i < nblk; i++)
		if (ubfs_read_block(pool, bp->dva.offset + i, buf + i * BS) < 0)
			return -1;
	ubfs_fletcher4(buf, nblk * BS, ck);
	if (memcmp(ck, bp->checksum, sizeof(ck)) != 0)
		return -2; /* checksum mismatch — corruption */
	if (bp->lsize < cap)
		memset(buf + bp->lsize, 0, cap - bp->lsize); /* zero tail for the caller */
	return (int)bp->lsize;
}

/* ── block-pointer tree (CoW write / read of a logical record id) ───────────*/

/* Recursively CoW logical record `blkid` of the subtree rooted at *bp.
 * `height` is the indirection height of the block *bp points at (0 = data leaf).
 * At the leaf, `data` holds `lsize` valid bytes (zero-padded to its run). */
static int tree_cow(ubfs_pool_t *pool,
                    ubfs_blkptr_t *bp,
                    int height,
                    uint64_t blkid,
                    uint8_t data_type,
                    const void *data,
                    uint32_t lsize)
{
	uint8_t ind[BS];
	ubfs_blkptr_t *slots;
	ubfs_blkptr_t nbp;
	uint32_t idx;

	if (height == 0)
	{
		if (leaf_write_cow(pool, data, lsize, data_type, &nbp) < 0)
			return -1;
		bp_free(pool, bp);
		*bp = nbp;
		return 0;
	}

	if (bp_is_hole(bp))
		memset(ind, 0, BS);
	else if (blk_read_verify(pool, bp, ind) < 0)
		return -1;
	slots = (ubfs_blkptr_t *)ind;
	idx = (uint32_t)((blkid >> (FANOUT_SHIFT * (height - 1))) & (FANOUT - 1));

	if (tree_cow(pool, &slots[idx], height - 1, blkid, data_type, data, lsize) < 0)
		return -1;
	if (blk_write_cow(pool, ind, UBFS_OT_INDIRECT, (uint8_t)height, &nbp) < 0)
		return -1;
	bp_free(pool, bp);
	*bp = nbp;
	return 0;
}

/* Grow the dnode's tree so it can address record `blkid` (adds indirect levels). */
static int tree_grow(ubfs_pool_t *pool, ubfs_dnode_t *dn, uint64_t blkid)
{
	int need_h = 0;
	uint64_t cap = 1; /* FANOUT^0 = 1 (height 0 addresses only blkid 0) */

	while (cap <= blkid)
	{
		cap *= FANOUT;
		need_h++;
	}
	while (dn->nlevels < need_h + 1)
	{
		uint8_t ind[BS];
		ubfs_blkptr_t nbp;

		memset(ind, 0, BS);
		((ubfs_blkptr_t *)ind)[0] = dn->blkptr[0]; /* old root becomes child 0 */
		if (blk_write_cow(pool, ind, UBFS_OT_INDIRECT, dn->nlevels, &nbp) < 0)
			return -1;
		dn->blkptr[0] = nbp; /* old root block stays referenced via slot 0 */
		dn->nlevels++;
	}
	return 0;
}

/* Read logical record `blkid` of object `dn` into `buf` (capacity = dn->datablksz).
 * Fills the whole buffer (data + zero tail).  @return logical size, or negative. */
static int tree_read_leaf(ubfs_pool_t *pool, const ubfs_dnode_t *dn, uint64_t blkid, uint8_t *buf, uint32_t cap)
{
	ubfs_blkptr_t bp = dn->blkptr[0];
	int height = dn->nlevels - 1;
	uint8_t ind[BS];

	while (height > 0)
	{
		uint32_t idx;
		if (bp_is_hole(&bp))
		{
			memset(buf, 0, cap);
			return 0;
		}
		if (blk_read_verify(pool, &bp, ind) < 0)
			return -1;
		idx = (uint32_t)((blkid >> (FANOUT_SHIFT * (height - 1))) & (FANOUT - 1));
		bp = ((ubfs_blkptr_t *)ind)[idx];
		height--;
	}
	return leaf_read_verify(pool, &bp, buf, cap);
}

/* Write logical record `blkid` of object `dn`.  `buf` holds `lsize` valid bytes
 * (the buffer is dn->datablksz wide and zero-padded beyond `lsize`). */
static int dnode_block_write(ubfs_pool_t *pool, ubfs_dnode_t *dn, uint64_t blkid, const void *buf, uint32_t lsize)
{
	if (tree_grow(pool, dn, blkid) < 0)
		return -1;
	if (tree_cow(pool, &dn->blkptr[0], dn->nlevels - 1, blkid, dn->type, buf, lsize) < 0)
		return -1;
	if (blkid > dn->maxblkid)
		dn->maxblkid = blkid;
	return 0;
}

/* ── object-set / dnode-array access (the metadnode is itself an object) ─────*/

static void init_dnode(ubfs_dnode_t *dn, uint8_t type, uint8_t bonustype)
{
	memset(dn, 0, sizeof(*dn));
	dn->type = type;
	dn->nlevels = 1;
	dn->nblkptr = UBFS_DN_NBLKPTR;
	dn->bonustype = bonustype;
	dn->checksum = UBFS_CK_FLETCHER4;
	dn->compress = UBFS_CO_OFF;
	dn->datablksz = BS;
}

void ubfs_dmu_objset_create(ubfs_pool_t *pool, uint64_t ostype, ubfs_dmu_os_t *os)
{
	memset(os, 0, sizeof(*os));
	os->pool = pool;
	os->type = ostype;
	os->next_object = 1; /* object 0 is reserved */
	init_dnode(&os->metadnode, UBFS_OT_DNODE, UBFS_BT_NONE);
}

int ubfs_dmu_objset_open(ubfs_pool_t *pool, const ubfs_blkptr_t *bp, ubfs_dmu_os_t *os)
{
	uint8_t block[BS];
	ubfs_objset_t od;

	if (blk_read_verify(pool, bp, block) < 0)
		return -1;
	memcpy(&od, block, sizeof(od));
	memset(os, 0, sizeof(*os));
	os->pool = pool;
	os->metadnode = od.metadnode;
	os->type = od.type;
	os->next_object = od.next_object;
	os->rootbp = *bp; /* remember the header block so the next sync can free it */
	return 0;
}

int ubfs_dmu_objset_sync(ubfs_dmu_os_t *os, ubfs_blkptr_t *bp_out)
{
	uint8_t block[BS];
	ubfs_objset_t od;
	ubfs_blkptr_t newbp;

	memset(&od, 0, sizeof(od));
	od.metadnode = os->metadnode;
	od.type = os->type;
	od.next_object = os->next_object;
	memset(block, 0, BS);
	memcpy(block, &od, sizeof(od));
	if (blk_write_cow(os->pool, block, UBFS_OT_OBJSET, 0, &newbp) < 0)
		return -1;
	/* Free the PREVIOUS on-disk header block; without this every commit (which
	 * re-serialises each objset into a fresh block) orphaned one block per objset
	 * — a per-transaction leak that, in the kernel, bled two blocks on every file
	 * op.  Deferred + hole-safe (a never-yet-synced objset has a zero rootbp). */
	bp_free(os->pool, &os->rootbp);
	os->rootbp = newbp;
	*bp_out = newbp;
	return 0;
}

/* dnode N lives at byte N*512 within the metadnode's data (always 4 KiB blocks). */
int ubfs_dmu_dnode_get(ubfs_dmu_os_t *os, uint64_t obj, ubfs_dnode_t *dn)
{
	uint64_t mblk = (obj * UBFS_DNODE_SIZE) / BS;
	uint32_t moff = (uint32_t)((obj * UBFS_DNODE_SIZE) % BS);
	uint8_t block[BS];

	if (tree_read_leaf(os->pool, &os->metadnode, mblk, block, BS) < 0)
		return -1;
	memcpy(dn, block + moff, UBFS_DNODE_SIZE);
	return 0;
}

int ubfs_dmu_dnode_put(ubfs_dmu_os_t *os, uint64_t obj, const ubfs_dnode_t *dn)
{
	uint64_t mblk = (obj * UBFS_DNODE_SIZE) / BS;
	uint32_t moff = (uint32_t)((obj * UBFS_DNODE_SIZE) % BS);
	uint8_t block[BS];

	if (tree_read_leaf(os->pool, &os->metadnode, mblk, block, BS) < 0)
		return -1;
	memcpy(block + moff, dn, UBFS_DNODE_SIZE);
	return dnode_block_write(os->pool, &os->metadnode, mblk, block, BS);
}

uint64_t ubfs_dmu_object_alloc(ubfs_dmu_os_t *os, uint8_t otype, uint8_t bonustype)
{
	uint64_t obj;
	ubfs_dnode_t dn;

	/* Recycle a freed slot if the list has one, else bump.  Both O(1) — no scan of
	 * the metadnode (an earlier hint-scan design walked every existing object on the
	 * first alloc after mount, which on a large root fs stalled for minutes). */
	if (os->free_obj_n > 0)
		obj = os->free_objs[--os->free_obj_n];
	else
		obj = os->next_object++;

	init_dnode(&dn, otype, bonustype);
	if (ubfs_dmu_dnode_put(os, obj, &dn) < 0)
		return 0;
	return obj;
}

/* Recursively free every block under *bp: an indirect subtree of `height`
 * levels, or a data leaf when height == 0.  Frees the children first, then the
 * block *bp itself, all through the SPA's deferred-free chokepoint. */
static int tree_free(ubfs_pool_t *pool, ubfs_blkptr_t *bp, int height)
{
	if (bp_is_hole(bp))
		return 0;
	if (height > 0)
	{
		uint8_t ind[BS];
		ubfs_blkptr_t *slots;

		if (blk_read_verify(pool, bp, ind) < 0)
			return -1;
		slots = (ubfs_blkptr_t *)ind;
		for (int i = 0; i < FANOUT; i++)
			tree_free(pool, &slots[i], height - 1);
	}
	bp_free(pool, bp);
	return 0;
}

int ubfs_dmu_object_free(ubfs_dmu_os_t *os, uint64_t obj)
{
	ubfs_dnode_t dn;
	ubfs_dnode_t empty;

	if (ubfs_dmu_dnode_get(os, obj, &dn) < 0)
		return -1;
	if (dn.type == UBFS_OT_NONE)
		return 0; /* already free — idempotent */
	if (tree_free(os->pool, &dn.blkptr[0], dn.nlevels - 1) < 0)
		return -1;
	/* Mark the dnode unused so a stale blkptr can never be followed into
	 * now-freed (and later reallocated) blocks.  The dnode SLOT itself is not
	 * reclaimed — object numbers are bump-allocated and never reused — but that
	 * is 512 bytes; the object's data blocks (the real space) are freed above. */
	memset(&empty, 0, sizeof(empty));
	empty.type = UBFS_OT_NONE;
	if (ubfs_dmu_dnode_put(os, obj, &empty) < 0)
		return -1;
	if (os->free_obj_n < UBFS_OBJ_FREELIST)
		os->free_objs[os->free_obj_n++] = obj; /* recycle it on the next alloc */
	return 0;
}

/* ── byte-granular object I/O (records are read-modify-write) ────────────────*/

int ubfs_dmu_write(ubfs_dmu_os_t *os, uint64_t obj, uint64_t off, const void *buf, uint64_t len)
{
	const uint8_t *src = (const uint8_t *)buf;
	ubfs_dnode_t dn;
	uint32_t rsz;
	uint8_t *leaf;
	int rc = 0;

	uint8_t stackbuf[BS]; /* avoid a heap alloc for the common one-block recordsize */

	if (ubfs_dmu_dnode_get(os, obj, &dn) < 0)
		return -1;
	rsz = dn.datablksz ? dn.datablksz : BS;
	leaf = (rsz <= BS) ? stackbuf : (uint8_t *)malloc(rsz);
	if (!leaf)
		return -1;

	while (len)
	{
		uint64_t blkid = off / rsz;
		uint32_t boff = (uint32_t)(off % rsz);
		uint32_t chunk = rsz - boff;
		int old;
		uint32_t nlsize;

		if (chunk > len)
			chunk = (uint32_t)len;

		if (boff == 0 && chunk == rsz)
		{
			/* full-record overwrite — no read needed */
			memcpy(leaf, src, rsz);
			nlsize = rsz;
		}
		else
		{
			old = tree_read_leaf(os->pool, &dn, blkid, leaf, rsz); /* zero-fills to rsz */
			if (old < 0)
			{
				rc = -1;
				break;
			}
			memcpy(leaf + boff, src, chunk);
			nlsize = (uint32_t)old > boff + chunk ? (uint32_t)old : boff + chunk;
		}
		if (dnode_block_write(os->pool, &dn, blkid, leaf, nlsize) < 0)
		{
			rc = -1;
			break;
		}

		off += chunk;
		src += chunk;
		len -= chunk;
	}
	if (leaf != stackbuf)
		free(leaf);
	if (rc < 0)
		return rc;

	/* If this object carries POSIX attrs, keep the size up to date. */
	if (dn.bonustype == UBFS_BT_INODE && dn.bonuslen >= sizeof(ubfs_inode_t))
	{
		ubfs_inode_t *zn = (ubfs_inode_t *)dn.bonus;
		if (off > zn->size)
			zn->size = off;
	}
	return ubfs_dmu_dnode_put(os, obj, &dn);
}

int ubfs_dmu_read(ubfs_dmu_os_t *os, uint64_t obj, uint64_t off, void *buf, uint64_t len)
{
	uint8_t *dst = (uint8_t *)buf;
	ubfs_dnode_t dn;
	uint32_t rsz;
	uint8_t *leaf;
	int rc = 0;

	uint8_t stackbuf[BS]; /* avoid a heap alloc for the common one-block recordsize */

	if (ubfs_dmu_dnode_get(os, obj, &dn) < 0)
		return -1;
	rsz = dn.datablksz ? dn.datablksz : BS;
	leaf = (rsz <= BS) ? stackbuf : (uint8_t *)malloc(rsz);
	if (!leaf)
		return -1;

	while (len)
	{
		uint64_t blkid = off / rsz;
		uint32_t boff = (uint32_t)(off % rsz);
		uint32_t chunk = rsz - boff;

		if (chunk > len)
			chunk = (uint32_t)len;
		if (tree_read_leaf(os->pool, &dn, blkid, leaf, rsz) < 0) /* zero-padded to rsz */
		{
			rc = -1;
			break;
		}
		memcpy(dst, leaf + boff, chunk);

		off += chunk;
		dst += chunk;
		len -= chunk;
	}
	if (leaf != stackbuf)
		free(leaf);
	return rc;
}
