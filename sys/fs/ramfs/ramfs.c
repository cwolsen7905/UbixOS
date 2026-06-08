/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ramfs — in-memory filesystem (architecture-neutral): the early-boot initramfs
 * root and the permanent tmpfs.  A simple tree of nodes (dirs + files) held in
 * kmalloc'd memory; files grow their data buffer on write.  Registers with the
 * VFS like any other driver (vfsRegisterFS) and stores its per-file node in
 * fd->res (a void*, so it is 64-bit safe — unlike the 32-bit fd->start that
 * devfs abuses).  Being a writable non-FAT filesystem, it exercises the generic
 * VFS write/create/mkdir path with no block device.
 */
#include <fs/ramfs/ramfs.h>
#include <fs/vfs/vfs.h>
#include <fs/vfs/mount.h>
#include <fs/vfs/file.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>

/* A node in the ramfs tree.  Directories chain children via ->child/->sibling;
 * files own a kmalloc'd data buffer grown on write. */
struct ramfs_node
{
	char name[256];
	int is_dir;
	struct ramfs_node *parent;
	struct ramfs_node *child;   /* dir: first child */
	struct ramfs_node *sibling; /* next entry in the parent's list */
	char *data;                 /* file: contents (NULL until first write) */
	u_int32_t size;             /* file: bytes used */
	u_int32_t cap;              /* file: buffer capacity */
};

/**
 * Allocate + zero a node named @name (truncated to fit) under @parent.
 */
static struct ramfs_node *node_alloc(const char *name, int is_dir, struct ramfs_node *parent)
{
	struct ramfs_node *n = (struct ramfs_node *)kmalloc(sizeof(struct ramfs_node));
	if (n == NULL)
		return (NULL);
	memset(n, 0, sizeof(*n));
	strncpy(n->name, name, sizeof(n->name) - 1);
	n->is_dir = is_dir;
	n->parent = parent;
	if (parent != NULL)
	{
		n->sibling = parent->child;
		parent->child = n;
	}
	return (n);
}

/**
 * Find the direct child of @dir named @name (length @len), or NULL.
 */
static struct ramfs_node *find_child(struct ramfs_node *dir, const char *name, size_t len)
{
	struct ramfs_node *c;
	for (c = dir->child; c != NULL; c = c->sibling)
	{
		if (strlen(c->name) == len && memcmp(c->name, name, len) == 0)
			return (c);
	}
	return (NULL);
}

/**
 * Walk @path from @root.  When @create is non-zero, missing intermediate
 * components are created as directories and the leaf as @leaf_dir ? dir : file.
 *
 * @return the resolved node, or NULL (not found and not creating, or OOM).
 */
static struct ramfs_node *walk(struct ramfs_node *root, const char *path, int create, int leaf_dir)
{
	struct ramfs_node *cur = root;
	const char *p = path;

	while (*p == '/')
		p++;
	if (*p == '\0')
		return (root); /* "/" is the root directory */

	for (;;)
	{
		const char *seg = p;
		while (*p != '\0' && *p != '/')
			p++;
		size_t seglen = (size_t)(p - seg);
		int last = 1;
		{
			const char *q = p;
			while (*q == '/')
				q++;
			if (*q != '\0')
				last = 0;
		}

		struct ramfs_node *next = find_child(cur, seg, seglen);
		if (next == NULL)
		{
			if (!create)
				return (NULL);
			char nm[256];
			size_t n = (seglen < sizeof(nm) - 1) ? seglen : sizeof(nm) - 1;
			memcpy(nm, seg, n);
			nm[n] = '\0';
			next = node_alloc(nm, last ? leaf_dir : 1, cur);
			if (next == NULL)
				return (NULL);
		}
		cur = next;
		if (last)
			return (cur);
		while (*p == '/')
			p++;
	}
}

/* ---- VFS driver ops ----------------------------------------------------- */

static int ramfs_initialize(struct vfs_mountPoint *mp)
{
	/* Each mount gets its own root directory node (so /tmp, /run, initramfs / are
	 * independent).  Stored in mp->fsInfo for the ops below + ramfs_populate. */
	struct ramfs_node *root = node_alloc("/", 1, NULL);
	if (root == NULL)
		return (0); /* vfs_mount treats 0 as failure (non-zero == success) */
	mp->fsInfo = root;
	return (1);
}

static int ramfs_open(char *path, fileDescriptor_t *fd)
{
	struct ramfs_node *root = (struct ramfs_node *)fd->mp->fsInfo;
	struct ramfs_node *n;
	int want_write = (fd->mode & (fileWrite | fileAppend)) != 0;

	/* Create on open for write modes (tmpfs); otherwise plain lookup. */
	n = walk(root, path, want_write, 0);
	if (n == NULL || n->is_dir)
		return (0); /* not found, or it's a directory */

	if (fd->mode & fileTrunc)
		n->size = 0;

	fd->res = n;
	fd->size = n->size;
	fd->offset = 0;
	return (1);
}

static int ramfs_read(fileDescriptor_t *fd, char *data, off_t offset, long size)
{
	struct ramfs_node *n = (struct ramfs_node *)fd->res;
	long avail;

	if (n == NULL || n->data == NULL || offset >= (off_t)n->size)
		return (0);
	avail = (long)n->size - (long)offset;
	if (size > avail)
		size = avail;
	memcpy(data, n->data + offset, (size_t)size);
	return ((int)size);
}

static int ramfs_write(fileDescriptor_t *fd, char *data, off_t offset, long size)
{
	struct ramfs_node *n = (struct ramfs_node *)fd->res;
	u_int32_t end;

	if (n == NULL || size <= 0)
		return (0);
	end = (u_int32_t)offset + (u_int32_t)size;

	/* Grow the buffer if needed (geometric, to amortize repeated appends). */
	if (end > n->cap)
	{
		u_int32_t newcap = (n->cap == 0) ? 64 : n->cap;
		while (newcap < end)
			newcap *= 2;
		char *nb = (char *)kmalloc(newcap);
		if (nb == NULL)
			return (0);
		if (n->data != NULL)
		{
			memcpy(nb, n->data, n->size);
			kfree(n->data);
		}
		n->data = nb;
		n->cap = newcap;
	}
	memcpy(n->data + offset, data, (size_t)size);
	if (end > n->size)
		n->size = end;
	fd->size = n->size;
	return ((int)size);
}

static int ramfs_makedir(char *path, void *mpv)
{
	struct vfs_mountPoint *mp = (struct vfs_mountPoint *)mpv;
	struct ramfs_node *root = (struct ramfs_node *)mp->fsInfo;
	struct ramfs_node *n = walk(root, path, 1, 1);
	return ((n != NULL && n->is_dir) ? 0 : -1);
}

static int ramfs_opendir(const char *path, kDIR_t *dir)
{
	struct ramfs_node *root = (struct ramfs_node *)dir->mp->fsInfo;
	struct ramfs_node *n = walk(root, path, 0, 0);
	if (n == NULL || !n->is_dir)
		return (-1);
	dir->dirHandle = n->child; /* iterate the child list */
	return (0);
}

static int ramfs_readdir(kDIR_t *dir, struct kdirent *ent)
{
	struct ramfs_node *n = (struct ramfs_node *)dir->dirHandle;
	if (n == NULL)
		return (-1);
	strncpy(ent->d_name, n->name, sizeof(ent->d_name) - 1);
	ent->d_name[sizeof(ent->d_name) - 1] = '\0';
	dir->dirHandle = n->sibling;
	return (0);
}

static int ramfs_closedir(kDIR_t *dir)
{
	dir->dirHandle = NULL;
	return (0);
}

int ramfs_init(void)
{
	struct fileSystem ramFS = {
	    NULL,                     /* prev */
	    NULL,                     /* next */
	    (void *)ramfs_initialize, /* vfsInitFS   */
	    (void *)ramfs_read,       /* vfsRead     */
	    (void *)ramfs_write,      /* vfsWrite    */
	    (void *)ramfs_open,       /* vfsOpenFile */
	    NULL,                     /* vfsUnlink   */
	    (void *)ramfs_makedir,    /* vfsMakeDir  */
	    NULL,                     /* vfsRemDir   */
	    NULL,                     /* vfsSync     */
	    VFS_TYPE_RAMFS,           /* vfsType     */
	    ramfs_opendir,
	    ramfs_readdir,
	    ramfs_closedir,
	    NULL, /* vfsClose */
	};

	if (vfsRegisterFS(ramFS) != 0)
		return (1);
	return (0);
}

int ramfs_populate(const char *mountPoint, const char *path, const void *data, u_int32_t len)
{
	struct vfs_mountPoint *mp = vfs_findMount(mountPoint);
	struct ramfs_node *root;
	struct ramfs_node *n;

	if (mp == NULL || mp->fsInfo == NULL)
		return (-1);
	root = (struct ramfs_node *)mp->fsInfo;

	n = walk(root, path, 1, 0); /* create file + any parent dirs */
	if (n == NULL || n->is_dir)
		return (-1);

	if (len > 0)
	{
		char *buf = (char *)kmalloc(len);
		if (buf == NULL)
			return (-1);
		memcpy(buf, data, len);
		if (n->data != NULL)
			kfree(n->data);
		n->data = buf;
		n->size = len;
		n->cap = len;
	}
	return (0);
}
