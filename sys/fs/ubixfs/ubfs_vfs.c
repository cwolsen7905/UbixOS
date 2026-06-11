/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS kernel VFS driver (pooled copy-on-write / lite-ZFS) — step K2: a
 * read-only, file-backed (loopback) mount.  The portable core (lib/ubixfs_core)
 * does all pool/dataset/file work; this file is the thin VFS glue:
 *
 *   - a ubfs_vdev_io_t adapter that reads 4 KB pool blocks from a backing image
 *     file (an ordinary file on an already-mounted FS) via the kernel file API
 *     (fopen/fread), so no block-device or partition plumbing is needed yet;
 *   - vfsInitFS = open backing -> pool_open -> dsl_open -> lookup("root") ->
 *     open_dataset -> fs_init, with the handle stashed in mp->fsInfo;
 *   - vfsOpenFile/Read + vfsOpenDir/ReadDir/CloseDir mapped onto the core's
 *     ubfs_fs_lookup / ubfs_fs_read / ubfs_fs_getattr / ubfs_fs_readdir.
 *
 * Write (vfsWrite/MakeDir/RemDir/Sync) is K3.  See docs/design/ubixfs-pool-plan.md.
 */
#include <sys/types.h>
#include <string.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <sys/bus.h>       /* struct ubx_device (raw block-device vdev) */
#include <ubixos/vitals.h> /* systemVitals->mountPoints (pool enumeration) */
#include <fs/vfs/vfs.h>
#include <fs/vfs/mount.h>
#include <fs/vfs/file.h>
#include <fs/vfs/bcache.h> /* bcache_read/write (raw vdev) */
#include <fs/ubixfs/ubfs_vfs.h>
#include <api/ubfs_pool.h> /* struct ubix_pool_info (ubpool/ubfs query) */

#include <ubfs_fs.h>
#include <ubfs_dsl.h>
#include <ubfs_spa.h> /* ubfs_pool_config / ubfs_pool_free_blocks */

/* The dataset every fresh pool carries (created by the host `ubfs mkpool`). */
#define ROOT_DS "root"

/* Max directory entries materialised per opendir (K2 caps; grown if needed). */
#define UBFS_DIR_MAX 256

/* Backing-image path for the next mount (set via ubfs_vfs_set_backing). */
static char g_backing[256] = "/pool.img";

/* Vdev backend for the next mount: 0 = loopback file, 1 = raw block device.
 * Consumed + reset by vfsInitFS.  See ubfs_vfs_set_raw(). */
static int g_raw_next;

/* Sectors per 4 KB pool block on a 512-byte-sector device. */
#define UBFS_SECTORS_PER_BLOCK (UBFS_BLOCK_SIZE / 512u)

/* Per-mount state, stored in vfs_mountPoint::fsInfo. */
struct ubfs_mount
{
	fileDescriptor_t *backing; /* open backing image (loopback vdev); NULL if raw */
	struct ubx_device *rawdev; /* block device (raw vdev); NULL if loopback */
	int writable;              /* opened read-write (mount perms 'w') */
	ubfs_vdev_io_t io;
	ubfs_pool_t *pool;
	ubfs_dsl_t dsl;
	ubfs_dmu_os_t os;
	ubfs_dataset_phys_t dp;
	ubfs_fs_t fs;
	uint64_t ds_obj;
};

/* Per-open-file state, stored in fileDescriptor_t::res. */
struct ubfs_file
{
	struct ubfs_mount *m;
	uint64_t obj;
};

/* One materialised directory entry. */
struct ubfs_collected
{
	char name[256];
	uint64_t obj;
	u_int8_t type;
};

/* Per-opendir state, stored in kDIR_t::dirHandle. */
struct ubfs_dir
{
	int count;
	int idx;
	struct ubfs_collected ents[UBFS_DIR_MAX];
};

/* The most-recently mounted instance, for the argument-less vfsSync hook. */
static struct ubfs_mount *g_mount;

/**
 * Commit the open transaction group: flush the dataset's dirty objects, update
 * the MOS, and advance the uberblock.  Non-destructive — the in-memory dataset
 * stays usable for the next operation (the open-txg model), so this is called
 * after every mutation so changes persist without an explicit sync().
 *
 * @return 0 on success, -1 on a backing-write failure.
 */
static int ubfs_commit(struct ubfs_mount *m)
{
	if (ubfs_dsl_sync_dataset(&m->dsl, m->ds_obj, &m->os) < 0)
		return (-1);
	return (ubfs_dsl_sync(&m->dsl));
}

/**
 * Strip the mount-point prefix from @path so it is relative to the pool root.
 *
 * vfsMakeDir is handed the full path ("/pool/etc") while vfsUnlink/RemDir get an
 * already-relative one ("/etc"); stripping mp->mountPoint handles both (a path
 * that does not start with the prefix is returned unchanged).
 */
static const char *pool_relative(struct vfs_mountPoint *mp, const char *path)
{
	size_t mlen = strlen(mp->mountPoint);

	if (mlen > 1 && strncmp(path, mp->mountPoint, mlen) == 0)
	{
		path += mlen;
		if (path[0] == '\0')
			return ("/");
	}
	return (path);
}

/* ── file-backed vdev: read 4 KB pool blocks from the backing image ─────────── */

/**
 * Read one UBFS_BLOCK_SIZE pool block @blk from the backing file into @buf.
 *
 * @return 0 on success, -1 on a short read or missing backing file.
 */
static int backing_read(void *ctx, uint64_t blk, void *buf)
{
	struct ubfs_mount *m = g_mount;
	fileDescriptor_t *fd = (fileDescriptor_t *)ctx;

	if (fd == 0)
		return (-1);
	if (m != 0 && blk >= m->io.size_blocks)
	{
		kprintf("ubixfs: backing_read out of range blk=%u/%u\n", (u_int32_t)blk, (u_int32_t)m->io.size_blocks);
		return (-1);
	}
	fd->offset = (off_t)(blk * UBFS_BLOCK_SIZE);
	{
		size_t got = fread(buf, 1, UBFS_BLOCK_SIZE, fd);
		if (got != UBFS_BLOCK_SIZE)
		{
			kprintf("ubixfs: backing_read SHORT blk=%u got=%u\n", (u_int32_t)blk, (u_int32_t)got);
			return (-1);
		}
	}
	return (0);
}

/**
 * Write one block @blk to the backing file (K3 write path; unused at K2).
 *
 * @return 0 on success, -1 otherwise.
 */
static int backing_write(void *ctx, uint64_t blk, const void *buf)
{
	struct ubfs_mount *m = g_mount;
	fileDescriptor_t *fd = (fileDescriptor_t *)ctx;

	if (fd == 0)
		return (-1);
	if (m != 0 && blk >= m->io.size_blocks)
	{
		kprintf("ubixfs: backing_write out of range blk=%u/%u\n", (u_int32_t)blk, (u_int32_t)m->io.size_blocks);
		return (-1);
	}
	fd->offset = (off_t)(blk * UBFS_BLOCK_SIZE);
	if (fwrite((void *)buf, 1, UBFS_BLOCK_SIZE, fd) != UBFS_BLOCK_SIZE)
		return (-1);
	return (0);
}

/* ── raw vdev: read/write 4 KB pool blocks via the buffer cache (8 × 512 B) ──── */

/**
 * Read one pool block @blk (8 sectors) from the block device into @buf.  The LBA
 * is partition-relative; the block driver adds the partition offset (i386 hd.c).
 *
 * @return 0 on success, -1 on a device error.
 */
static int raw_read(void *ctx, uint64_t blk, void *buf)
{
	struct ubx_device *dev = (struct ubx_device *)ctx;
	u_int32_t base = (u_int32_t)blk * UBFS_SECTORS_PER_BLOCK;
	u_int32_t s;

	for (s = 0; s < UBFS_SECTORS_PER_BLOCK; s++)
		if (bcache_read(dev, base + s, (u_int8_t *)buf + s * 512) != 0)
			return (-1);
	return (0);
}

/**
 * Write one pool block @blk (8 sectors) from @buf to the block device.
 *
 * @return 0 on success, -1 on a device error.
 */
static int raw_write(void *ctx, uint64_t blk, const void *buf)
{
	struct ubx_device *dev = (struct ubx_device *)ctx;
	u_int32_t base = (u_int32_t)blk * UBFS_SECTORS_PER_BLOCK;
	u_int32_t s;

	for (s = 0; s < UBFS_SECTORS_PER_BLOCK; s++)
		if (bcache_write(dev, base + s, (const u_int8_t *)buf + s * 512) != 0)
			return (-1);
	return (0);
}

/* ── VFS operations ─────────────────────────────────────────────────────────── */

/**
 * vfsInitFS: open the backing image and import the pool's root filesystem.
 *
 * @return 1 on success, 0 on failure (the VFS treats 0 as a failed mount).
 */
static int ubfs_vfs_initfs(struct vfs_mountPoint *mp)
{
	struct ubfs_mount *m;

	m = (struct ubfs_mount *)kmalloc(sizeof(*m));
	if (m == 0)
		return (0);
	memset(m, 0, sizeof(*m));
	m->writable = (mp->perms == 'w');

	if (g_raw_next)
	{
		/* Raw vdev: the pool lives on a block-device partition (the path toward a
		 * mountable root).  Read 4 KB blocks via bcache over mp->device; the core
		 * bounds its own reads to the pool's on-disk size, so size_blocks here is a
		 * generous safety cap. */
		g_raw_next = 0;
		if (mp->device == 0)
		{
			kprintf("ubixfs: raw mount has no block device\n");
			kfree(m);
			return (0);
		}
		m->rawdev = mp->device;
		m->io.ctx = mp->device;
		m->io.read = raw_read;
		m->io.write = raw_write;
		m->io.size_blocks = 0x10000000u; /* 1 TB cap; core bounds to pool asize */
	}
	else
	{
		/* Loopback vdev: the pool is a file ("r+" rw — no truncate; the pool is
		 * fixed-size and CoW-rewrites in place, so it never grows the file). */
		m->backing = fopen(g_backing, m->writable ? "r+" : "r");
		if (m->backing == 0)
		{
			kprintf("ubixfs: cannot open backing image %s\n", g_backing);
			kfree(m);
			return (0);
		}
		m->io.ctx = m->backing;
		m->io.read = backing_read;
		m->io.write = backing_write;
		m->io.size_blocks = (uint64_t)m->backing->size / UBFS_BLOCK_SIZE;
	}

	if (ubfs_pool_open(&m->io, &m->pool) < 0)
	{
		kprintf("ubixfs: not a UbixFS pool (%s)\n", m->rawdev ? "raw" : g_backing);
		if (m->backing)
			fclose(m->backing);
		kfree(m);
		return (0);
	}
	if (ubfs_dsl_open(m->pool, &m->dsl) < 0 || ubfs_dsl_lookup(&m->dsl, ROOT_DS, &m->ds_obj) < 0 ||
	    ubfs_dsl_open_dataset(&m->dsl, m->ds_obj, &m->dp, &m->os) < 0)
	{
		kprintf("ubixfs: no '%s' filesystem dataset\n", ROOT_DS);
		ubfs_pool_close(m->pool);
		if (m->backing)
			fclose(m->backing);
		kfree(m);
		return (0);
	}
	ubfs_fs_init(&m->fs, &m->os, 0);

	mp->fsInfo = m;
	g_mount = m;
	kprintf("ubixfs: mounted %s pool (%s) at %s\n",
	        m->rawdev ? "raw" : "loopback",
	        m->writable ? "rw" : "ro",
	        mp->mountPoint);
	return (1);
}

/**
 * vfsOpenFile: resolve @path within the pool and attach an open-file handle.
 *
 * @return 1 on success, 0 if the path does not exist.
 */
static int ubfs_vfs_open(const char *path, fileDescriptor_t *fd)
{
	struct ubfs_mount *m;
	struct ubfs_file *f;
	ubfs_inode_t in;
	uint64_t obj;

	if (fd == 0 || fd->mp == 0 || fd->mp->fsInfo == 0)
		return (0);
	m = (struct ubfs_mount *)fd->mp->fsInfo;

	if (ubfs_fs_lookup(&m->fs, path, &obj) < 0)
	{
		/* Not found: create it if this is a writable open ("w"/"a"/"r+" on a
		 * writable mount); otherwise the open fails. */
		if (!m->writable || (fd->mode & (fileWrite | fileTrunc)) == 0)
			return (0);
		if (ubfs_fs_create(&m->fs, path, 0644, 0, 0, &obj) < 0)
			return (0);
		ubfs_commit(m);
	}
	else if (m->writable && (fd->mode & fileTrunc))
	{
		/* Exists + "w" (truncate): the core has no truncate primitive, so drop
		 * and recreate — matches how the host `ubfs cp` overwrites. */
		ubfs_fs_unlink(&m->fs, path);
		if (ubfs_fs_create(&m->fs, path, 0644, 0, 0, &obj) < 0)
			return (0);
		ubfs_commit(m);
	}

	f = (struct ubfs_file *)kmalloc(sizeof(*f));
	if (f == 0)
		return (0);
	f->m = m;
	f->obj = obj;

	fd->res = f;
	fd->ino = (u_int32_t)obj;
	fd->perms = 0x1;
	fd->size = (ubfs_fs_getattr(&m->fs, obj, &in) == 0) ? (u_int32_t)in.size : 0;
	return (1);
}

/**
 * vfsRead: read @size bytes at @offset from the open file into @data.
 *
 * @return the number of bytes read (clamped to file size), or a negative error.
 */
static int ubfs_vfs_read(fileDescriptor_t *fd, char *data, off_t offset, long size)
{
	struct ubfs_file *f;

	if (fd == 0 || fd->res == 0)
		return (-1);
	f = (struct ubfs_file *)fd->res;
	return ((int)ubfs_fs_read(&f->m->fs, f->obj, (uint64_t)offset, data, (uint64_t)size));
}

/**
 * vfsWrite: write @size bytes at @offset from @data into the open file, then
 * commit the transaction group so the change persists.
 *
 * @return the number of bytes written, or a negative error.
 */
static int ubfs_vfs_write(fileDescriptor_t *fd, char *data, off_t offset, long size)
{
	struct ubfs_file *f;

	if (fd == 0 || fd->res == 0)
		return (-1);
	f = (struct ubfs_file *)fd->res;
	if (!f->m->writable)
		return (-1);

	if (ubfs_fs_write(&f->m->fs, f->obj, (uint64_t)offset, data, (uint64_t)size) < 0)
		return (-1);
	if (ubfs_commit(f->m) < 0)
		return (-1);

	if ((u_int32_t)(offset + size) > fd->size)
		fd->size = (u_int32_t)(offset + size);
	return ((int)size);
}

/**
 * vfsClose: release the open-file handle.
 */
static int ubfs_vfs_close(fileDescriptor_t *fd)
{
	if (fd != 0 && fd->res != 0)
	{
		kfree(fd->res);
		fd->res = 0;
	}
	return (0);
}

/* readdir collector: materialise entries into the ubfs_dir array. */
static int collect_cb(void *arg, const char *name, uint64_t obj, u_int8_t type)
{
	struct ubfs_dir *d = (struct ubfs_dir *)arg;

	if (d->count >= UBFS_DIR_MAX)
		return (0);
	strncpy(d->ents[d->count].name, name, sizeof(d->ents[0].name) - 1);
	d->ents[d->count].name[sizeof(d->ents[0].name) - 1] = '\0';
	d->ents[d->count].obj = obj;
	d->ents[d->count].type = type;
	d->count++;
	return (0);
}

/**
 * vfsOpenDir: resolve @path to a directory and materialise its entries.
 *
 * @return 1 on success, 0 if @path is not a directory in the pool.
 */
static int ubfs_vfs_opendir(const char *path, kDIR_t *dir)
{
	struct ubfs_mount *m;
	struct ubfs_dir *d;
	ubfs_inode_t in;
	uint64_t obj;

	if (dir == 0 || dir->mp == 0 || dir->mp->fsInfo == 0)
		return (0);
	m = (struct ubfs_mount *)dir->mp->fsInfo;

	if (ubfs_fs_lookup(&m->fs, path, &obj) < 0)
		return (0);
	if (ubfs_fs_getattr(&m->fs, obj, &in) < 0 || (in.mode & UBFS_S_IFMT) != UBFS_S_IFDIR)
		return (0);

	d = (struct ubfs_dir *)kmalloc(sizeof(*d));
	if (d == 0)
		return (0);
	d->count = 0;
	d->idx = 0;
	ubfs_fs_readdir(&m->fs, obj, collect_cb, d);

	dir->dirHandle = d;
	return (1);
}

/**
 * vfsReadDir: hand back the next materialised entry.
 *
 * @return 0 on a returned entry, -1 at end of directory.
 */
static int ubfs_vfs_readdir(kDIR_t *dir, struct kdirent *ent)
{
	struct ubfs_dir *d;

	if (dir == 0 || dir->dirHandle == 0)
		return (-1);
	d = (struct ubfs_dir *)dir->dirHandle;
	if (d->idx >= d->count)
		return (-1);

	ent->d_ino = (u_int32_t)d->ents[d->idx].obj;
	ent->d_type = (d->ents[d->idx].type == UBFS_DT_DIR) ? KDT_DIR : KDT_REG;
	strncpy(ent->d_name, d->ents[d->idx].name, sizeof(ent->d_name) - 1);
	ent->d_name[sizeof(ent->d_name) - 1] = '\0';
	d->idx++;
	return (0);
}

/**
 * vfsCloseDir: free the materialised entry list.
 */
static int ubfs_vfs_closedir(kDIR_t *dir)
{
	if (dir != 0 && dir->dirHandle != 0)
	{
		kfree(dir->dirHandle);
		dir->dirHandle = 0;
	}
	return (0);
}

/**
 * vfsMakeDir: create directory @path (handed the full path) and commit.
 *
 * @return 0 on success, -1 on failure.
 */
static int ubfs_vfs_mkdir(char *path, void *vfd)
{
	fileDescriptor_t *fd = (fileDescriptor_t *)vfd;
	struct ubfs_mount *m;
	uint64_t obj;

	if (fd == 0 || fd->mp == 0 || fd->mp->fsInfo == 0)
		return (-1);
	m = (struct ubfs_mount *)fd->mp->fsInfo;
	if (!m->writable)
		return (-1);

	if (ubfs_fs_mkdir(&m->fs, pool_relative(fd->mp, path), 0755, 0, 0, &obj) < 0)
		return (-1);
	return (ubfs_commit(m));
}

/**
 * vfsUnlink / vfsRemDir: remove @path (a mount-relative path) and commit.  The
 * core's unlink handles both files and empty directories.
 *
 * @return 0 on success, -1 on failure.
 */
static int ubfs_vfs_unlink(char *path, void *vmp)
{
	struct vfs_mountPoint *mp = (struct vfs_mountPoint *)vmp;
	struct ubfs_mount *m;

	if (mp == 0 || mp->fsInfo == 0)
		return (-1);
	m = (struct ubfs_mount *)mp->fsInfo;
	if (!m->writable)
		return (-1);

	if (ubfs_fs_unlink(&m->fs, pool_relative(mp, path)) < 0)
		return (-1);
	return (ubfs_commit(m));
}

/**
 * vfsSync: commit the most-recently mounted pool (the hook takes no mount arg).
 * Mutations already commit eagerly, so this is a belt-and-braces flush.
 *
 * @return 0 on success or if nothing is mounted, -1 on a commit failure.
 */
static int ubfs_vfs_sync(void)
{
	if (g_mount == 0 || !g_mount->writable)
		return (0);
	return (ubfs_commit(g_mount));
}

/**
 * Set the backing-image path used by the next UbixFS mount.
 */
void ubfs_vfs_set_backing(const char *path)
{
	if (path == 0)
		return;
	strncpy(g_backing, path, sizeof(g_backing) - 1);
	g_backing[sizeof(g_backing) - 1] = '\0';
}

/**
 * Select the vdev backend (raw block device vs loopback file) for the next mount.
 */
void ubfs_vfs_set_raw(int on)
{
	g_raw_next = (on != 0);
}

/**
 * Fill @buf (a struct ubix_pool_info[]) with up to @max mounted-pool records by
 * walking the VFS mount list for UbixFS mounts.  Backs the native pool-query
 * syscall (the in-OS `ubpool`/`ubfs` commands).  @buf is a userland pointer the
 * thread's address space maps (the uBixOS native-syscall convention).
 *
 * @return the number of pools written.
 */
int ubfs_vfs_query(void *buf, int max)
{
	struct ubix_pool_info *out = (struct ubix_pool_info *)buf;
	struct vfs_mountPoint *mp;
	int n = 0;

	if (out == 0 || max <= 0 || systemVitals == 0)
		return (0);

	for (mp = systemVitals->mountPoints; mp != 0 && n < max; mp = mp->next)
	{
		struct ubfs_mount *m;
		const ubfs_vdev_config_t *cfg;

		if (mp->fs == 0 || mp->fs->vfsType != VFS_TYPE_UBIXFS || mp->fsInfo == 0)
			continue;
		m = (struct ubfs_mount *)mp->fsInfo;
		cfg = ubfs_pool_config(m->pool);

		memset(&out[n], 0, sizeof(out[n]));
		if (cfg != 0)
		{
			strncpy(out[n].pool, cfg->name, sizeof(out[n].pool) - 1);
			out[n].total_blocks = cfg->asize;
		}
		strncpy(out[n].mountpoint, mp->mountPoint, sizeof(out[n].mountpoint) - 1);
		strncpy(out[n].dataset, m->dp.name, sizeof(out[n].dataset) - 1);
		out[n].free_blocks = ubfs_pool_free_blocks(m->pool);
		out[n].dataset_used = m->dp.used;
		out[n].block_size = UBFS_BLOCK_SIZE;
		out[n].flags = (uint32_t)((m->rawdev ? UBIX_POOL_RAW : 0) | (m->writable ? UBIX_POOL_RW : 0));
		n++;
	}
	return (n);
}

/**
 * Register the UbixFS driver (VFS_TYPE_UBIXFS) with the VFS.
 *
 * @return 0 on success, 1 if registration failed.
 */
int ubfs_vfs_init(void)
{
	struct fileSystem fs = {
	    0,                         /* prev */
	    0,                         /* next */
	    (void *)ubfs_vfs_initfs,   /* vfsInitFS */
	    (void *)ubfs_vfs_read,     /* vfsRead */
	    (void *)ubfs_vfs_write,    /* vfsWrite */
	    (void *)ubfs_vfs_open,     /* vfsOpenFile */
	    (void *)ubfs_vfs_unlink,   /* vfsUnlink */
	    (void *)ubfs_vfs_mkdir,    /* vfsMakeDir */
	    (void *)ubfs_vfs_unlink,   /* vfsRemDir (unlink handles empty dirs) */
	    (void *)ubfs_vfs_sync,     /* vfsSync */
	    VFS_TYPE_UBIXFS,           /* vfsType */
	    (void *)ubfs_vfs_opendir,  /* vfsOpenDir */
	    (void *)ubfs_vfs_readdir,  /* vfsReadDir */
	    (void *)ubfs_vfs_closedir, /* vfsCloseDir */
	    (void *)ubfs_vfs_close,    /* vfsClose */
	};

	if (vfsRegisterFS(fs) != 0)
	{
		kprintf("ubixfs: failed to register VFS driver\n");
		return (1);
	}
	return (0);
}
