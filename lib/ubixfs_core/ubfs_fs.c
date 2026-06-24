/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * ubfs_fs — UbixFS POSIX filesystem layer.  See ubfs_fs.h.
 */
#include "ubfs_fs.h"
#include "ubfs_dsl.h" /* ubfs_recordsize_valid */
#include <string.h>

/* ── inode (POSIX attrs) in the object's dnode bonus ────────────────────────*/

static int inode_get(ubfs_fs_t *fs, uint64_t obj, ubfs_inode_t *in)
{
	ubfs_dnode_t dn;

	if (ubfs_dmu_dnode_get(fs->os, obj, &dn) < 0)
		return -1;
	if (dn.bonustype != UBFS_BT_INODE)
		return -1;
	memcpy(in, dn.bonus, sizeof(*in));
	return 0;
}

static int inode_put(ubfs_fs_t *fs, uint64_t obj, const ubfs_inode_t *in)
{
	ubfs_dnode_t dn;

	if (ubfs_dmu_dnode_get(fs->os, obj, &dn) < 0)
		return -1;
	memset(dn.bonus, 0, sizeof(dn.bonus));
	memcpy(dn.bonus, in, sizeof(*in));
	dn.bonuslen = sizeof(*in);
	dn.bonustype = UBFS_BT_INODE;
	return ubfs_dmu_dnode_put(fs->os, obj, &dn);
}

static uint8_t dtype_of(uint32_t mode)
{
	if ((mode & UBFS_S_IFMT) == UBFS_S_IFDIR)
		return UBFS_DT_DIR;
	if ((mode & UBFS_S_IFMT) == UBFS_S_IFLNK)
		return UBFS_DT_LNK;
	return UBFS_DT_REG;
}

/* ── path utilities ─────────────────────────────────────────────────────────*/

static int next_comp(const char **p, char *comp, int cap)
{
	const char *s = *p;
	int n = 0;

	while (*s == '/')
		s++;
	while (*s && *s != '/' && n < cap - 1)
		comp[n++] = *s++;
	comp[n] = '\0';
	while (*s == '/')
		s++;
	*p = s;
	return n;
}

static const char *split_parent(const char *path, char *pbuf, int cap)
{
	const char *slash = path, *q;
	int plen;

	for (q = path; *q; q++)
		if (*q == '/')
			slash = q;
	plen = (int)(slash - path);
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
	return slash + 1; /* leaf */
}

/* ── public API ─────────────────────────────────────────────────────────────*/

void ubfs_fs_init(ubfs_fs_t *fs, ubfs_dmu_os_t *os, uint64_t now)
{
	fs->os = os;
	fs->now = now;
	fs->recordsize = UBFS_BLOCK_SIZE;
}

void ubfs_fs_set_recordsize(ubfs_fs_t *fs, uint32_t recordsize)
{
	fs->recordsize = ubfs_recordsize_valid(recordsize) ? recordsize : UBFS_BLOCK_SIZE;
}

int ubfs_fs_mkroot(ubfs_fs_t *fs, uint32_t uid, uint32_t gid)
{
	uint64_t obj = ubfs_dmu_object_alloc(fs->os, UBFS_OT_DIRECTORY, UBFS_BT_INODE);
	ubfs_inode_t in;

	if (obj != UBFS_OBJ_ROOT)
		return -1; /* root must be object 1 of a fresh fs objset */
	memset(&in, 0, sizeof(in));
	in.mode = UBFS_S_IFDIR | 0755;
	in.uid = uid;
	in.gid = gid;
	in.nlink = 2; /* "." and parent */
	in.atime = in.mtime = in.ctime = in.crtime = fs->now;
	in.parent = UBFS_OBJ_ROOT;
	return inode_put(fs, obj, &in);
}

int ubfs_fs_lookup(ubfs_fs_t *fs, const char *path, uint64_t *obj)
{
	uint64_t cur = UBFS_OBJ_ROOT;
	char comp[UBFS_NAME_MAX + 1];

	while (next_comp(&path, comp, sizeof(comp)) > 0)
	{
		ubfs_inode_t in;
		uint64_t next;
		if (inode_get(fs, cur, &in) < 0)
			return -1;
		if ((in.mode & UBFS_S_IFMT) != UBFS_S_IFDIR)
			return -2; /* not a directory */
		/* "." stays in place; ".." ascends to the parent (root's parent is root).
		 * UbixFS stores no on-disk dot entries, so resolve them here — without it
		 * lstat("/."), `cd ..` and `cat ./x` all fail on the pool. */
		if (comp[0] == '.' && comp[1] == '\0')
			continue;
		if (comp[0] == '.' && comp[1] == '.' && comp[2] == '\0')
		{
			cur = (in.parent != 0) ? in.parent : cur;
			continue;
		}
		if (ubfs_dir_lookup(fs->os, cur, comp, &next, NULL) < 0)
			return -3; /* no such entry */
		cur = next;
	}
	*obj = cur;
	return 0;
}

/* Create an object of `mode` at `path`, link it into its parent. */
static int make_node(
    ubfs_fs_t *fs, const char *path, uint8_t otype, uint32_t mode, uint32_t uid, uint32_t gid, uint64_t *out)
{
	char pbuf[1024];
	const char *leaf = split_parent(path, pbuf, sizeof(pbuf));
	uint64_t pobj, obj, dummy;
	ubfs_inode_t pin, in;

	if (leaf[0] == '\0')
		return -1;
	if (ubfs_fs_lookup(fs, pbuf, &pobj) < 0)
		return -2;
	if (inode_get(fs, pobj, &pin) < 0 || (pin.mode & UBFS_S_IFMT) != UBFS_S_IFDIR)
		return -3;
	if (ubfs_dir_lookup(fs->os, pobj, leaf, &dummy, NULL) == 0)
		return -4; /* already exists */

	obj = ubfs_dmu_object_alloc(fs->os, otype, UBFS_BT_INODE);
	if (obj == 0)
		return -5;
	/* Stamp the dataset's recordsize onto new *regular* files (dirs/symlinks stay
	 * one block).  inode_put below re-reads this dnode and preserves datablksz. */
	if ((mode & UBFS_S_IFMT) == UBFS_S_IFREG && fs->recordsize > UBFS_BLOCK_SIZE)
	{
		ubfs_dnode_t dn;
		if (ubfs_dmu_dnode_get(fs->os, obj, &dn) == 0)
		{
			dn.datablksz = fs->recordsize;
			ubfs_dmu_dnode_put(fs->os, obj, &dn);
			/* The incompat bit is set authoritatively when the first >4K record is
			 * written (see leaf_write_cow); nothing to do here. */
		}
	}
	memset(&in, 0, sizeof(in));
	in.mode = mode;
	in.uid = uid;
	in.gid = gid;
	in.nlink = ((mode & UBFS_S_IFMT) == UBFS_S_IFDIR) ? 2 : 1;
	in.atime = in.mtime = in.ctime = in.crtime = fs->now;
	in.parent = pobj;
	if (inode_put(fs, obj, &in) < 0)
		return -6;
	if (ubfs_dir_add(fs->os, pobj, leaf, obj, dtype_of(mode)) < 0)
		return -7;
	*out = obj;
	return 0;
}

int ubfs_fs_mkdir(ubfs_fs_t *fs, const char *path, uint32_t mode, uint32_t uid, uint32_t gid, uint64_t *obj)
{
	return make_node(fs, path, UBFS_OT_DIRECTORY, UBFS_S_IFDIR | (mode & 07777), uid, gid, obj);
}

int ubfs_fs_create(ubfs_fs_t *fs, const char *path, uint32_t mode, uint32_t uid, uint32_t gid, uint64_t *obj)
{
	return make_node(fs, path, UBFS_OT_PLAIN_FILE, UBFS_S_IFREG | (mode & 07777), uid, gid, obj);
}

int ubfs_fs_symlink(ubfs_fs_t *fs, const char *path, const char *target, uint32_t uid, uint32_t gid, uint64_t *obj)
{
	uint64_t o;
	int r = make_node(fs, path, UBFS_OT_PLAIN_FILE, UBFS_S_IFLNK | 0777, uid, gid, &o);

	if (r < 0)
		return r;
	if (ubfs_fs_write(fs, o, 0, target, strlen(target)) < 0)
		return -8;
	if (obj)
		*obj = o;
	return 0;
}

int ubfs_fs_write(ubfs_fs_t *fs, uint64_t obj, uint64_t off, const void *buf, uint64_t len)
{
	ubfs_inode_t in;

	if (ubfs_dmu_write(fs->os, obj, off, buf, len) < 0)
		return -1;
	if (inode_get(fs, obj, &in) < 0)
		return -1;
	if (off + len > in.size)
		in.size = off + len;
	in.mtime = in.ctime = fs->now;
	return inode_put(fs, obj, &in);
}

int ubfs_fs_read(ubfs_fs_t *fs, uint64_t obj, uint64_t off, void *buf, uint64_t len)
{
	ubfs_inode_t in;

	if (inode_get(fs, obj, &in) < 0)
		return -1;
	if (off >= in.size)
		return 0;
	if (off + len > in.size)
		len = in.size - off;
	if (ubfs_dmu_read(fs->os, obj, off, buf, len) < 0)
		return -1;
	return (int)len;
}

int ubfs_fs_readlink(ubfs_fs_t *fs, uint64_t obj, char *buf, uint64_t bufsz)
{
	ubfs_inode_t in;
	uint64_t n;

	if (inode_get(fs, obj, &in) < 0)
		return -1;
	if ((in.mode & UBFS_S_IFMT) != UBFS_S_IFLNK)
		return -2;
	n = in.size;
	if (n >= bufsz)
		n = bufsz - 1;
	if (ubfs_dmu_read(fs->os, obj, 0, buf, n) < 0)
		return -1;
	buf[n] = '\0';
	return (int)n;
}

int ubfs_fs_getattr(ubfs_fs_t *fs, uint64_t obj, ubfs_inode_t *out)
{
	return inode_get(fs, obj, out);
}

int ubfs_fs_readdir(ubfs_fs_t *fs, uint64_t dirobj, ubfs_dir_cb cb, void *arg)
{
	return ubfs_dir_foreach(fs->os, dirobj, cb, arg);
}

int ubfs_fs_unlink(ubfs_fs_t *fs, const char *path)
{
	char pbuf[1024];
	const char *leaf = split_parent(path, pbuf, sizeof(pbuf));
	uint64_t pobj, obj;
	ubfs_inode_t in;

	if (ubfs_fs_lookup(fs, pbuf, &pobj) < 0)
		return -1;
	if (ubfs_dir_lookup(fs->os, pobj, leaf, &obj, NULL) < 0)
		return -2;
	if (inode_get(fs, obj, &in) < 0)
		return -3;
	if ((in.mode & UBFS_S_IFMT) == UBFS_S_IFDIR)
	{
		uint64_t n = 0;
		if (ubfs_dir_count(fs->os, obj, &n) < 0 || n != 0)
			return -4; /* directory not empty */
	}
	/* v1: drop the directory entry; object reclamation (free dnode + data) is a
	 * follow-up (needs a dmu_object_free). nlink bookkeeping kept for later. */
	return ubfs_dir_remove(fs->os, pobj, leaf);
}
