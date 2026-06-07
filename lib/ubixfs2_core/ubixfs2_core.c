/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * ubixfs2_core — UbixFS v2 format logic (block-I/O-agnostic).  See the header
 * and docs/design/ubixfs2-plan.md.  v0 scope: single allocation group, one inode
 * per block, inline directories, direct extents only, no journal.
 */
#include "ubixfs2_core.h"
#include <string.h>

#define U2_IFMT 0170000u
#define U2_IFDIR 0040000u
#define U2_IFREG 0100000u

/* ---- low-level block helpers ---- */

static int rd(ubixfs2_t *fs, uint32_t blk, void *buf)
{
	return fs->read_block(fs->io_ctx, blk, buf);
}

static int wr(ubixfs2_t *fs, uint32_t blk, const void *buf)
{
	return fs->write_block(fs->io_ctx, blk, buf);
}

/* ---- block bitmap (bit i set == block i allocated) ---- */

static int bitmap_set(ubixfs2_t *fs, uint32_t blk, int val)
{
	uint8_t  buf[UBIXFS2_BLOCK_SIZE];
	uint32_t bit = blk;
	uint32_t bmblk = fs->super.bitmap_start + bit / (UBIXFS2_BLOCK_SIZE * 8);
	uint32_t off = bit % (UBIXFS2_BLOCK_SIZE * 8);

	if (rd(fs, bmblk, buf) < 0)
		return -1;
	if (val)
		buf[off / 8] |= (uint8_t)(1u << (off % 8));
	else
		buf[off / 8] &= (uint8_t)~(1u << (off % 8));
	return wr(fs, bmblk, buf);
}

/* Allocate one free block; returns block number or 0 on full/error (block 0 is
 * the superblock, so 0 is a safe "none" sentinel). */
static uint32_t block_alloc(ubixfs2_t *fs)
{
	uint8_t buf[UBIXFS2_BLOCK_SIZE];

	for (uint32_t i = 0; i < (uint32_t)fs->super.num_blocks; i++)
	{
		uint32_t bmblk = fs->super.bitmap_start + i / (UBIXFS2_BLOCK_SIZE * 8);
		uint32_t off = i % (UBIXFS2_BLOCK_SIZE * 8);

		if (off == 0 || i == 0)
			if (rd(fs, bmblk, buf) < 0)
				return 0;
		if (!(buf[off / 8] & (1u << (off % 8))))
		{
			if (bitmap_set(fs, i, 1) < 0)
				return 0;
			fs->super.used_blocks++;
			return i;
		}
	}
	return 0;
}

static void block_free(ubixfs2_t *fs, uint32_t blk)
{
	if (blk == 0)
		return;
	if (bitmap_set(fs, blk, 0) == 0)
		fs->super.used_blocks--;
}

/* ---- superblock ---- */

int ubixfs2_sync_super(ubixfs2_t *fs)
{
	uint8_t buf[UBIXFS2_BLOCK_SIZE];

	memset(buf, 0, sizeof(buf));
	memcpy(buf, &fs->super, sizeof(fs->super));
	return wr(fs, 0, buf);
}

int ubixfs2_mount(ubixfs2_t *fs)
{
	uint8_t buf[UBIXFS2_BLOCK_SIZE];

	if (rd(fs, 0, buf) < 0)
		return -1;
	memcpy(&fs->super, buf, sizeof(fs->super));
	if (fs->super.magic1 != UBIXFS2_MAGIC1 || fs->super.magic2 != UBIXFS2_MAGIC2)
		return -2;
	return 0;
}

/* ---- inode I/O ---- */

int ubixfs2_read_inode(ubixfs2_t *fs, uint32_t blk, ubixfs2_inode_t *out)
{
	uint8_t buf[UBIXFS2_BLOCK_SIZE];

	if (rd(fs, blk, buf) < 0)
		return -1;
	memcpy(out, buf, sizeof(*out));
	if (out->magic != UBIXFS2_INODE_MAGIC)
		return -2;
	return 0;
}

int ubixfs2_write_inode(ubixfs2_t *fs, const ubixfs2_inode_t *in)
{
	uint8_t buf[UBIXFS2_BLOCK_SIZE];

	memset(buf, 0, sizeof(buf));
	memcpy(buf, in, sizeof(*in));
	return wr(fs, (uint32_t)in->inode_num.start, buf);
}

/* ---- inline directory helpers ---- */

static ubixfs2_dirhdr_t *dir_hdr(ubixfs2_inode_t *d)
{
	return (ubixfs2_dirhdr_t *)d->small_data;
}

static ubixfs2_dirent_t *dir_ents(ubixfs2_inode_t *d)
{
	return (ubixfs2_dirent_t *)(d->small_data + sizeof(ubixfs2_dirhdr_t));
}

/* Find a child by name in a directory inode; returns its inode block or 0. */
static uint32_t dir_find(ubixfs2_inode_t *d, const char *name)
{
	ubixfs2_dirent_t *e = dir_ents(d);

	for (int i = 0; i < UBIXFS2_INLINE_DIRENTS; i++)
		if (e[i].inode_block != 0 && strcmp(e[i].name, name) == 0)
			return e[i].inode_block;
	return 0;
}

static int dir_add(ubixfs2_inode_t *d, const char *name, uint32_t blk)
{
	ubixfs2_dirent_t *e = dir_ents(d);

	if (strlen(name) >= UBIXFS2_DIRENT_NAMELEN)
		return -1;
	for (int i = 0; i < UBIXFS2_INLINE_DIRENTS; i++)
	{
		if (e[i].inode_block == 0)
		{
			e[i].inode_block = blk;
			memset(e[i].name, 0, sizeof(e[i].name));
			strcpy(e[i].name, name);
			dir_hdr(d)->count++;
			return 0;
		}
	}
	return -2; /* inline dir full — B+tree path is a later phase */
}

static int dir_remove(ubixfs2_inode_t *d, const char *name)
{
	ubixfs2_dirent_t *e = dir_ents(d);

	for (int i = 0; i < UBIXFS2_INLINE_DIRENTS; i++)
		if (e[i].inode_block != 0 && strcmp(e[i].name, name) == 0)
		{
			e[i].inode_block = 0;
			dir_hdr(d)->count--;
			return 0;
		}
	return -1;
}

/* ---- path lookup ---- */

/* Copy the next '/'-delimited component of *p into comp; advance *p. */
static int next_comp(const char **p, char *comp, int cap)
{
	const char *s = *p;
	int         n = 0;

	while (*s == '/')
		s++;
	while (*s && *s != '/' && n < cap - 1)
		comp[n++] = *s++;
	comp[n] = '\0';
	while (*s == '/')
		s++;
	*p = s;
	return n; /* 0 == no more components */
}

int ubixfs2_lookup(ubixfs2_t *fs, const char *path, uint32_t *blk)
{
	ubixfs2_inode_t ino;
	uint32_t        cur = (uint32_t)fs->super.root_dir.start;
	char            comp[UBIXFS2_DIRENT_NAMELEN];

	while (next_comp(&path, comp, sizeof(comp)) > 0)
	{
		if (ubixfs2_read_inode(fs, cur, &ino) < 0)
			return -1;
		if ((ino.mode & U2_IFMT) != U2_IFDIR)
			return -2;
		cur = dir_find(&ino, comp);
		if (cur == 0)
			return -3; /* not found */
	}
	*blk = cur;
	return 0;
}

/* Split "/a/b/c" into parent path "/a/b" (in pbuf) and leaf "c" (returned ptr). */
static const char *split_parent(const char *path, char *pbuf, int cap)
{
	const char *slash = path;
	const char *p;

	for (p = path; *p; p++)
		if (*p == '/')
			slash = p;
	int plen = (int)(slash - path);
	if (plen <= 0)
	{
		pbuf[0] = '/';
		pbuf[1] = '\0';
	}
	else
	{
		if (plen >= cap)
			plen = cap - 1;
		memcpy(pbuf, path, plen);
		pbuf[plen] = '\0';
	}
	return slash + 1; /* leaf name */
}

/* ---- readdir / read_file ---- */

int ubixfs2_readdir(ubixfs2_t *fs, const ubixfs2_inode_t *dir, ubixfs2_dir_cb cb, void *arg)
{
	const ubixfs2_dirent_t *e = (const ubixfs2_dirent_t *)(dir->small_data + sizeof(ubixfs2_dirhdr_t));

	(void)fs;
	for (int i = 0; i < UBIXFS2_INLINE_DIRENTS; i++)
		if (e[i].inode_block != 0)
		{
			int r = cb(arg, e[i].name, e[i].inode_block);
			if (r != 0)
				return r;
		}
	return 0;
}

int ubixfs2_read_file(ubixfs2_t *fs, const ubixfs2_inode_t *ino, void *buf, int64_t off, int64_t len)
{
	uint8_t  blk[UBIXFS2_BLOCK_SIZE];
	uint8_t *out = (uint8_t *)buf;
	int64_t  size = ino->blocks.size;
	int64_t  done = 0;

	if (off >= size)
		return 0;
	if (off + len > size)
		len = size - off;

	/* Build a logical-block view of the direct extents and copy the range. */
	int64_t logical = 0; /* logical block index walked so far */
	for (int i = 0; i < UBIXFS2_NDIRECT && done < len; i++)
	{
		uint32_t start = ino->blocks.direct[i].start;
		uint32_t run = ino->blocks.direct[i].len;

		for (uint32_t b = 0; b < run && done < len; b++, logical++)
		{
			int64_t blk_off = logical * UBIXFS2_BLOCK_SIZE;
			if (blk_off + UBIXFS2_BLOCK_SIZE <= off)
				continue; /* before the requested range */
			if (blk_off >= off + len)
				break;
			if (rd(fs, start + b, blk) < 0)
				return -1;
			int64_t cstart = (off > blk_off) ? (off - blk_off) : 0;
			int64_t cend = ((off + len) < (blk_off + UBIXFS2_BLOCK_SIZE)) ? (off + len - blk_off)
			                                                              : UBIXFS2_BLOCK_SIZE;
			memcpy(out + done, blk + cstart, (size_t)(cend - cstart));
			done += cend - cstart;
		}
	}
	return (int)done;
}

/* ---- write layer ---- */

static int make_inode(ubixfs2_t *fs, const char *path, uint32_t mode, uint32_t uid, uint32_t gid,
                      uint32_t *new_blk, ubixfs2_inode_t *parent_out, uint32_t *parent_blk_out, char *leaf_out)
{
	char            pbuf[1024];
	const char     *leaf = split_parent(path, pbuf, sizeof(pbuf));
	uint32_t        pblk;
	ubixfs2_inode_t parent;
	uint32_t        nblk;
	ubixfs2_inode_t ino;

	if (strlen(leaf) == 0)
		return -1;
	if (ubixfs2_lookup(fs, pbuf, &pblk) < 0)
		return -2; /* parent missing */
	if (ubixfs2_read_inode(fs, pblk, &parent) < 0)
		return -3;
	if ((parent.mode & U2_IFMT) != U2_IFDIR)
		return -4;
	if (dir_find(&parent, leaf) != 0)
		return -5; /* exists */

	nblk = block_alloc(fs);
	if (nblk == 0)
		return -6;

	memset(&ino, 0, sizeof(ino));
	ino.magic = UBIXFS2_INODE_MAGIC;
	ino.inode_num.start = (uint16_t)nblk;
	ino.inode_num.len = 1;
	ino.parent.start = (uint16_t)pblk;
	ino.parent.len = 1;
	strncpy(ino.name, leaf, UBIXFS2_NAMELEN - 1);
	ino.uid = uid;
	ino.gid = gid;
	ino.mode = mode;
	ino.ref_count = 1;
	if (ubixfs2_write_inode(fs, &ino) < 0)
		return -7;

	*new_blk = nblk;
	if (parent_out)
		*parent_out = parent;
	if (parent_blk_out)
		*parent_blk_out = pblk;
	if (leaf_out)
		strcpy(leaf_out, leaf);
	return 0;
}

static int link_into_parent(ubixfs2_t *fs, ubixfs2_inode_t *parent, const char *leaf, uint32_t nblk)
{
	if (dir_add(parent, leaf, nblk) < 0)
		return -1;
	return ubixfs2_write_inode(fs, parent);
}

int ubixfs2_mkdir(ubixfs2_t *fs, const char *path, uint32_t mode, uint32_t uid, uint32_t gid)
{
	uint32_t        nblk, pblk;
	ubixfs2_inode_t parent;
	char            leaf[UBIXFS2_DIRENT_NAMELEN];
	int             r;

	r = make_inode(fs, path, U2_IFDIR | (mode & 07777), uid, gid, &nblk, &parent, &pblk, leaf);
	if (r < 0)
		return r;
	if (link_into_parent(fs, &parent, leaf, nblk) < 0)
		return -8;
	return ubixfs2_sync_super(fs);
}

int ubixfs2_create_file(ubixfs2_t *fs, const char *path, const void *data, int64_t len, uint32_t mode,
                        uint32_t uid, uint32_t gid)
{
	uint32_t        nblk, pblk;
	ubixfs2_inode_t parent, ino;
	char            leaf[UBIXFS2_DIRENT_NAMELEN];
	int             r;
	const uint8_t  *src = (const uint8_t *)data;
	int64_t         nblocks = (len + UBIXFS2_BLOCK_SIZE - 1) / UBIXFS2_BLOCK_SIZE;

	if (nblocks > UBIXFS2_NDIRECT)
		return -100; /* v0: direct extents only (max 64 blocks / 256 KB) */

	r = make_inode(fs, path, U2_IFREG | (mode & 07777), uid, gid, &nblk, &parent, &pblk, leaf);
	if (r < 0)
		return r;

	/* Allocate data blocks (one len==1 run each in v0) and write the data. */
	if (ubixfs2_read_inode(fs, nblk, &ino) < 0)
		return -9;
	for (int64_t i = 0; i < nblocks; i++)
	{
		uint8_t  blk[UBIXFS2_BLOCK_SIZE];
		uint32_t db = block_alloc(fs);
		if (db == 0)
			return -10;
		memset(blk, 0, sizeof(blk));
		int64_t chunk = len - i * UBIXFS2_BLOCK_SIZE;
		if (chunk > UBIXFS2_BLOCK_SIZE)
			chunk = UBIXFS2_BLOCK_SIZE;
		memcpy(blk, src + i * UBIXFS2_BLOCK_SIZE, (size_t)chunk);
		if (wr(fs, db, blk) < 0)
			return -11;
		ino.blocks.direct[i].start = (uint16_t)db;
		ino.blocks.direct[i].len = 1;
	}
	ino.blocks.size = len;
	if (ubixfs2_write_inode(fs, &ino) < 0)
		return -12;
	if (link_into_parent(fs, &parent, leaf, nblk) < 0)
		return -13;
	return ubixfs2_sync_super(fs);
}

int ubixfs2_unlink(ubixfs2_t *fs, const char *path)
{
	char            pbuf[1024];
	const char     *leaf = split_parent(path, pbuf, sizeof(pbuf));
	uint32_t        pblk, tblk;
	ubixfs2_inode_t parent, target;

	if (ubixfs2_lookup(fs, pbuf, &pblk) < 0)
		return -1;
	if (ubixfs2_read_inode(fs, pblk, &parent) < 0)
		return -2;
	tblk = dir_find(&parent, leaf);
	if (tblk == 0)
		return -3;
	if (ubixfs2_read_inode(fs, tblk, &target) < 0)
		return -4;
	if ((target.mode & U2_IFMT) == U2_IFDIR && dir_hdr(&target)->count != 0)
		return -5; /* directory not empty */

	/* Free the target's data extents, then its inode block. */
	for (int i = 0; i < UBIXFS2_NDIRECT; i++)
		for (uint32_t b = 0; b < target.blocks.direct[i].len; b++)
			block_free(fs, target.blocks.direct[i].start + b);
	block_free(fs, tblk);

	if (dir_remove(&parent, leaf) < 0)
		return -6;
	if (ubixfs2_write_inode(fs, &parent) < 0)
		return -7;
	return ubixfs2_sync_super(fs);
}

/* ---- format ---- */

int ubixfs2_format(ubixfs2_t *fs, int64_t num_blocks, const char *label)
{
	uint8_t  buf[UBIXFS2_BLOCK_SIZE];
	uint32_t bitmap_blocks;
	uint32_t root_blk;
	ubixfs2_inode_t root;

	if (num_blocks < 8)
		return -1;

	bitmap_blocks = (uint32_t)((num_blocks + UBIXFS2_BLOCK_SIZE * 8 - 1) / (UBIXFS2_BLOCK_SIZE * 8));

	memset(&fs->super, 0, sizeof(fs->super));
	strncpy(fs->super.name, label ? label : "UBIXFS2", sizeof(fs->super.name) - 1);
	fs->super.magic1 = UBIXFS2_MAGIC1;
	fs->super.magic2 = UBIXFS2_MAGIC2;
	fs->super.byte_order = UBIXFS2_BYTE_ORDER_LE;
	fs->super.block_size = UBIXFS2_BLOCK_SIZE;
	fs->super.block_shift = 12;
	fs->super.num_blocks = num_blocks;
	fs->super.num_ags = 1;
	fs->super.blocks_per_ag = (uint32_t)num_blocks;
	fs->super.flags = UBIXFS2_CLEAN;
	fs->super.bitmap_start = 1;
	fs->super.bitmap_blocks = bitmap_blocks;
	fs->super.used_blocks = 0;

	/* Zero the bitmap. */
	memset(buf, 0, sizeof(buf));
	for (uint32_t i = 0; i < bitmap_blocks; i++)
		if (wr(fs, fs->super.bitmap_start + i, buf) < 0)
			return -2;

	/* Reserve the superblock + bitmap blocks. */
	for (uint32_t i = 0; i < fs->super.bitmap_start + bitmap_blocks; i++)
	{
		if (bitmap_set(fs, i, 1) < 0)
			return -3;
		fs->super.used_blocks++;
	}

	/* Root directory inode. */
	root_blk = block_alloc(fs);
	if (root_blk == 0)
		return -4;
	memset(&root, 0, sizeof(root));
	root.magic = UBIXFS2_INODE_MAGIC;
	root.inode_num.start = (uint16_t)root_blk;
	root.inode_num.len = 1;
	root.parent.start = (uint16_t)root_blk; /* root is its own parent */
	root.parent.len = 1;
	strcpy(root.name, "/");
	root.mode = U2_IFDIR | 0755;
	root.ref_count = 1;
	if (ubixfs2_write_inode(fs, &root) < 0)
		return -5;

	fs->super.root_dir.start = (uint16_t)root_blk;
	fs->super.root_dir.len = 1;
	return ubixfs2_sync_super(fs);
}
