/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS DMU implementation — copy-on-write object engine.  See ubfs_dmu.h.
 *
 * An object's data lives in a radix block-pointer tree rooted at dnode.blkptr[0]
 * (height = nlevels-1; fan-out = 4096/128 = 32).  Writes are CoW: a new data
 * block is written, then every indirect block up to the dnode is rewritten to a
 * new block and the old ones freed (through the SPA's single free chokepoint).
 * A zeroed block pointer (dva.offset == 0) is a hole → reads as zeros (sparse).
 */
#include "ubfs_dmu.h"
#include "fletcher.h"
#include <string.h>

#define BS UBFS_BLOCK_SIZE
#define FANOUT (UBFS_BLOCK_SIZE / (int)sizeof(ubfs_blkptr_t)) /* 32 */
#define FANOUT_SHIFT 5                                        /* log2(32) */

/* ── leaf block I/O with integrity ──────────────────────────────────────────*/

static int bp_is_hole(const ubfs_blkptr_t *bp)
{
	return bp->dva.offset == 0; /* block 0 is the label — never real data */
}

static void bp_free(ubfs_pool_t *pool, const ubfs_blkptr_t *bp)
{
	if (!bp_is_hole(bp))
		ubfs_free_block(pool, bp->dva.offset); /* the single free chokepoint */
}

/* CoW-write one block; fills *out with a fresh, checksummed block pointer. */
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

/* ── block-pointer tree (CoW write / read of a logical block id) ────────────*/

/* Recursively CoW logical block `blkid` of the subtree rooted at *bp.
 * `height` is the indirection height of the block *bp points at (0 = data). */
static int tree_cow(ubfs_pool_t *pool, ubfs_blkptr_t *bp, int height, uint64_t blkid, uint8_t data_type,
                    const void *data)
{
	uint8_t        ind[BS];
	ubfs_blkptr_t *slots;
	ubfs_blkptr_t  nbp;
	uint32_t       idx;

	if (height == 0)
	{
		if (blk_write_cow(pool, data, data_type, 0, &nbp) < 0)
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

	if (tree_cow(pool, &slots[idx], height - 1, blkid, data_type, data) < 0)
		return -1;
	if (blk_write_cow(pool, ind, UBFS_OT_INDIRECT, (uint8_t)height, &nbp) < 0)
		return -1;
	bp_free(pool, bp);
	*bp = nbp;
	return 0;
}

/* Grow the dnode's tree so it can address `blkid` (adds indirect levels on top). */
static int tree_grow(ubfs_pool_t *pool, ubfs_dnode_t *dn, uint64_t blkid)
{
	int      need_h = 0;
	uint64_t cap = 1; /* FANOUT^0 = 1 (height 0 addresses only blkid 0) */

	while (cap <= blkid)
	{
		cap *= FANOUT;
		need_h++;
	}
	while (dn->nlevels < need_h + 1)
	{
		uint8_t       ind[BS];
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

static int tree_read(ubfs_pool_t *pool, const ubfs_dnode_t *dn, uint64_t blkid, void *out)
{
	ubfs_blkptr_t bp = dn->blkptr[0];
	int           height = dn->nlevels - 1;
	uint8_t       ind[BS];

	while (height > 0)
	{
		uint32_t idx;
		if (bp_is_hole(&bp))
		{
			memset(out, 0, BS);
			return 0;
		}
		if (blk_read_verify(pool, &bp, ind) < 0)
			return -1;
		idx = (uint32_t)((blkid >> (FANOUT_SHIFT * (height - 1))) & (FANOUT - 1));
		bp = ((ubfs_blkptr_t *)ind)[idx];
		height--;
	}
	return blk_read_verify(pool, &bp, out);
}

/* Write logical block `blkid` (a full BS buffer) of the object `dn`. */
static int dnode_block_write(ubfs_pool_t *pool, ubfs_dnode_t *dn, uint64_t blkid, const void *buf)
{
	if (tree_grow(pool, dn, blkid) < 0)
		return -1;
	if (tree_cow(pool, &dn->blkptr[0], dn->nlevels - 1, blkid, dn->type, buf) < 0)
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
	uint8_t       block[BS];
	ubfs_objset_t od;

	if (blk_read_verify(pool, bp, block) < 0)
		return -1;
	memcpy(&od, block, sizeof(od));
	memset(os, 0, sizeof(*os));
	os->pool = pool;
	os->metadnode = od.metadnode;
	os->type = od.type;
	os->next_object = od.next_object;
	return 0;
}

int ubfs_dmu_objset_sync(ubfs_dmu_os_t *os, ubfs_blkptr_t *bp_out)
{
	uint8_t       block[BS];
	ubfs_objset_t od;

	memset(&od, 0, sizeof(od));
	od.metadnode = os->metadnode;
	od.type = os->type;
	od.next_object = os->next_object;
	memset(block, 0, BS);
	memcpy(block, &od, sizeof(od));
	return blk_write_cow(os->pool, block, UBFS_OT_OBJSET, 0, bp_out);
}

/* dnode N lives at byte N*512 within the metadnode's data. */
int ubfs_dmu_dnode_get(ubfs_dmu_os_t *os, uint64_t obj, ubfs_dnode_t *dn)
{
	uint64_t mblk = (obj * UBFS_DNODE_SIZE) / BS;
	uint32_t moff = (uint32_t)((obj * UBFS_DNODE_SIZE) % BS);
	uint8_t  block[BS];

	if (tree_read(os->pool, &os->metadnode, mblk, block) < 0)
		return -1;
	memcpy(dn, block + moff, UBFS_DNODE_SIZE);
	return 0;
}

int ubfs_dmu_dnode_put(ubfs_dmu_os_t *os, uint64_t obj, const ubfs_dnode_t *dn)
{
	uint64_t mblk = (obj * UBFS_DNODE_SIZE) / BS;
	uint32_t moff = (uint32_t)((obj * UBFS_DNODE_SIZE) % BS);
	uint8_t  block[BS];

	if (tree_read(os->pool, &os->metadnode, mblk, block) < 0)
		return -1;
	memcpy(block + moff, dn, UBFS_DNODE_SIZE);
	return dnode_block_write(os->pool, &os->metadnode, mblk, block);
}

uint64_t ubfs_dmu_object_alloc(ubfs_dmu_os_t *os, uint8_t otype, uint8_t bonustype)
{
	uint64_t     obj = os->next_object++;
	ubfs_dnode_t dn;

	init_dnode(&dn, otype, bonustype);
	if (ubfs_dmu_dnode_put(os, obj, &dn) < 0)
		return 0;
	return obj;
}

/* ── byte-granular object I/O (partial blocks are read-modify-write) ─────────*/

int ubfs_dmu_write(ubfs_dmu_os_t *os, uint64_t obj, uint64_t off, const void *buf, uint64_t len)
{
	const uint8_t *src = (const uint8_t *)buf;
	ubfs_dnode_t   dn;

	if (ubfs_dmu_dnode_get(os, obj, &dn) < 0)
		return -1;

	while (len)
	{
		uint64_t blkid = off / BS;
		uint32_t boff = (uint32_t)(off % BS);
		uint64_t chunk = BS - boff;
		uint8_t  block[BS];

		if (chunk > len)
			chunk = len;
		if (chunk < BS) /* partial block → read-modify-write */
		{
			if (tree_read(os->pool, &dn, blkid, block) < 0)
				return -1;
		}
		memcpy(block + boff, src, chunk);
		if (dnode_block_write(os->pool, &dn, blkid, block) < 0)
			return -1;

		off += chunk;
		src += chunk;
		len -= chunk;
	}

	/* If this object carries POSIX attrs, keep the size up to date. */
	if (dn.bonustype == UBFS_BT_ZNODE && dn.bonuslen >= sizeof(ubfs_znode_t))
	{
		ubfs_znode_t *zn = (ubfs_znode_t *)dn.bonus;
		if (off > zn->size)
			zn->size = off;
	}
	return ubfs_dmu_dnode_put(os, obj, &dn);
}

int ubfs_dmu_read(ubfs_dmu_os_t *os, uint64_t obj, uint64_t off, void *buf, uint64_t len)
{
	uint8_t     *dst = (uint8_t *)buf;
	ubfs_dnode_t dn;

	if (ubfs_dmu_dnode_get(os, obj, &dn) < 0)
		return -1;

	while (len)
	{
		uint64_t blkid = off / BS;
		uint32_t boff = (uint32_t)(off % BS);
		uint64_t chunk = BS - boff;
		uint8_t  block[BS];

		if (chunk > len)
			chunk = len;
		if (tree_read(os->pool, &dn, blkid, block) < 0)
			return -1;
		memcpy(dst, block + boff, chunk);

		off += chunk;
		dst += chunk;
		len -= chunk;
	}
	return 0;
}
