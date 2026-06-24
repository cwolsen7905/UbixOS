/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * ubfs — host CLI for UbixFS pools (the mtools-style image tool).  Operates on a
 * pool image file directly (unprivileged, no mount).  A fresh pool is created
 * with one filesystem dataset named "root", so it is immediately usable.
 *
 *   ubfs mkpool <img> <size>            format a pool (size like 64M, 512K)
 *   ubfs mkdir  <img> <path>            create a directory (and parents)
 *   ubfs cp     <hostfile> <img>:<path> copy a file in (mode from the host file)
 *   ubfs cp     <img>:<path> <hostfile> copy a file out
 *   ubfs cpr    <hostdir>  <img>:<path> recursive copy in (files/dirs/symlinks)
 *   ubfs ln     <img> <target> <link>   create a symlink (target stored verbatim)
 *   ubfs ls     <img> <path>            list a directory (or stat one entry)
 *   ubfs rm     <img> <path>            remove a file / empty directory
 *
 * All filesystem ops act on the pool's "root" dataset.
 */
#include <ubfs_fs.h>
#include <ubfs_dsl.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define ROOT_DS "root"

/* ── file-backed vdev ───────────────────────────────────────────────────────*/
struct fdev
{
	int fd;
};
static int fdr(void *c, uint64_t b, void *buf)
{
	return pread(((struct fdev *)c)->fd, buf, UBFS_BLOCK_SIZE, (off_t)b * UBFS_BLOCK_SIZE) == UBFS_BLOCK_SIZE ? 0
	                                                                                                          : -1;
}
static int fdw(void *c, uint64_t b, const void *buf)
{
	return pwrite(((struct fdev *)c)->fd, buf, UBFS_BLOCK_SIZE, (off_t)b * UBFS_BLOCK_SIZE) == UBFS_BLOCK_SIZE ? 0
	                                                                                                           : -1;
}

/* An opened pool + its root filesystem dataset. */
struct mount
{
	struct fdev dev;
	ubfs_vdev_io_t io;
	ubfs_pool_t *p;
	ubfs_dsl_t dsl;
	ubfs_dmu_os_t os;
	ubfs_dataset_phys_t dp;
	ubfs_fs_t fs;
	uint64_t ds_obj;
};

static int mount_pool(struct mount *m, const char *img, int rw)
{
	memset(m, 0, sizeof(*m));
	m->dev.fd = open(img, rw ? O_RDWR : O_RDONLY);
	if (m->dev.fd < 0)
	{
		perror(img);
		return -1;
	}
	m->io.ctx = &m->dev;
	m->io.read = fdr;
	m->io.write = fdw;
	{
		struct stat st;
		fstat(m->dev.fd, &st);
		m->io.size_blocks = (uint64_t)st.st_size / UBFS_BLOCK_SIZE;
	}
	if (ubfs_pool_open(&m->io, &m->p) < 0)
	{
		fprintf(stderr, "%s: not a UbixFS pool\n", img);
		return -1;
	}
	if (ubfs_dsl_open(m->p, &m->dsl) < 0 || ubfs_dsl_lookup(&m->dsl, ROOT_DS, &m->ds_obj) < 0 ||
	    ubfs_dsl_open_dataset(&m->dsl, m->ds_obj, &m->dp, &m->os) < 0)
	{
		fprintf(stderr, "%s: no root filesystem dataset\n", img);
		return -1;
	}
	ubfs_fs_init(&m->fs, &m->os, (uint64_t)time(NULL));
	ubfs_fs_set_recordsize(&m->fs, (uint32_t)m->dp.recordsize);
	return 0;
}

static int mount_sync(struct mount *m)
{
	if (ubfs_dsl_sync_dataset(&m->dsl, m->ds_obj, &m->os) < 0)
		return -1;
	return ubfs_dsl_sync(&m->dsl);
}

static void mount_close(struct mount *m)
{
	ubfs_pool_close(m->p);
	close(m->dev.fd);
}

/* Create `path` and any missing parent directories (mkdir -p). */
static int mkdirs(ubfs_fs_t *fs, const char *path)
{
	char prefix[1024];
	uint64_t obj;
	size_t i = 0, n = 0;

	while (path[i])
	{
		while (path[i] == '/')
			i++;
		while (path[i] && path[i] != '/')
			prefix[n++] = path[i++];
		if (n == 0)
			break;
		prefix[n] = '\0';
		/* prefix is now an absolute path up to this component */
		char abs[1026];
		abs[0] = '/';
		memcpy(abs + 1, prefix, n + 1);
		if (ubfs_fs_lookup(fs, abs, &obj) != 0)
			if (ubfs_fs_mkdir(fs, abs, 0755, 0, 0, &obj) < 0)
				return -1;
		prefix[n++] = '/'; /* keep building the prefix */
	}
	return 0;
}

static int64_t parse_size(const char *s)
{
	char *e;
	int64_t v = strtoll(s, &e, 10);
	if (*e == 'K' || *e == 'k')
		v *= 1024;
	else if (*e == 'M' || *e == 'm')
		v *= 1024 * 1024;
	else if (*e == 'G' || *e == 'g')
		v *= 1024 * 1024 * 1024;
	return v;
}

/* Split "img:/path" → image + fs-path; returns 1 if it is an image ref. */
static int imgref(const char *s, char *img, const char **p)
{
	const char *colon = strchr(s, ':');
	if (!colon)
		return 0;
	size_t n = (size_t)(colon - s);
	memcpy(img, s, n);
	img[n] = '\0';
	*p = colon + 1;
	return 1;
}

/* ── subcommands ────────────────────────────────────────────────────────────*/

static int cmd_mkpool(const char *img, const char *sizestr)
{
	int64_t bytes = parse_size(sizestr);
	int fd = open(img, O_RDWR | O_CREAT | O_TRUNC, 0644);
	struct fdev dev;
	ubfs_vdev_io_t io;
	ubfs_pool_t *p;
	ubfs_dsl_t dsl;
	ubfs_dmu_os_t os;
	ubfs_dataset_phys_t dp;
	ubfs_fs_t fs;
	uint64_t ds;

	if (fd < 0 || ftruncate(fd, bytes) < 0)
	{
		perror(img);
		return 1;
	}
	dev.fd = fd;
	io.ctx = &dev;
	io.read = fdr;
	io.write = fdw;
	io.size_blocks = (uint64_t)bytes / UBFS_BLOCK_SIZE;

	if (ubfs_pool_format(&io, "tank", (uint64_t)time(NULL), &p) < 0 || ubfs_dsl_create(p, &dsl) < 0 ||
	    ubfs_dsl_create_fs(&dsl, ROOT_DS, &ds) < 0 || ubfs_dsl_open_dataset(&dsl, ds, &dp, &os) < 0)
	{
		fprintf(stderr, "mkpool failed\n");
		return 1;
	}
	ubfs_fs_init(&fs, &os, (uint64_t)time(NULL));
	if (ubfs_fs_mkroot(&fs, 0, 0) < 0 || ubfs_dsl_sync_dataset(&dsl, ds, &os) < 0 || ubfs_dsl_sync(&dsl) < 0)
	{
		fprintf(stderr, "mkpool: root setup failed\n");
		return 1;
	}
	ubfs_pool_close(p);
	close(fd);
	printf("mkpool: %s (%lld bytes), filesystem dataset '%s'\n", img, (long long)bytes, ROOT_DS);
	return 0;
}

static int cmd_mkdir(const char *img, const char *path)
{
	struct mount m;
	int r;

	if (mount_pool(&m, img, 1) < 0)
		return 1;
	r = mkdirs(&m.fs, path);
	if (r == 0)
		r = mount_sync(&m);
	mount_close(&m);
	if (r < 0)
		fprintf(stderr, "mkdir %s failed\n", path);
	return r < 0 ? 1 : 0;
}

static int cmd_rm(const char *img, const char *path)
{
	struct mount m;
	int r;

	if (mount_pool(&m, img, 1) < 0)
		return 1;
	r = ubfs_fs_unlink(&m.fs, path);
	if (r == 0)
		r = mount_sync(&m);
	mount_close(&m);
	if (r < 0)
		fprintf(stderr, "rm %s failed (%d)\n", path, r);
	return r < 0 ? 1 : 0;
}

struct ls_ctx
{
	ubfs_fs_t *fs;
};
static int ls_cb(void *arg, const char *name, uint64_t obj, uint8_t type)
{
	struct ls_ctx *c = (struct ls_ctx *)arg;
	ubfs_inode_t in;
	char t = (type == UBFS_DT_DIR) ? 'd' : (type == UBFS_DT_LNK) ? 'l' : '-';

	if (ubfs_fs_getattr(c->fs, obj, &in) == 0)
		printf("%c %04llo %llu:%llu %10llu  %s\n",
		       t,
		       (unsigned long long)(in.mode & 07777),
		       (unsigned long long)in.uid,
		       (unsigned long long)in.gid,
		       (unsigned long long)in.size,
		       name);
	else
		printf("? ????  %s\n", name);
	return 0;
}

static int cmd_ls(const char *img, const char *path)
{
	struct mount m;
	uint64_t obj;
	ubfs_inode_t in;
	struct ls_ctx ctx;

	if (mount_pool(&m, img, 0) < 0)
		return 1;
	if (ubfs_fs_lookup(&m.fs, path, &obj) < 0)
	{
		fprintf(stderr, "%s: no such path\n", path);
		mount_close(&m);
		return 1;
	}
	ubfs_fs_getattr(&m.fs, obj, &in);
	if ((in.mode & UBFS_S_IFMT) == UBFS_S_IFDIR)
	{
		ctx.fs = &m.fs;
		ubfs_fs_readdir(&m.fs, obj, ls_cb, &ctx);
	}
	else
	{
		ctx.fs = &m.fs;
		ls_cb(&ctx, path, obj, 0);
	}
	mount_close(&m);
	return 0;
}

/* Copy one host file into an ALREADY-mounted pool (no per-file sync/close).  The
 * shared guts of `cp` (single file) and `cpr` (recursive); cpr opens the pool
 * once and copies the whole tree, since each mount_pool/mount_sync cycle is
 * expensive (full open + commit). */
static int copy_one(struct mount *m, const char *hostfile, const char *fspath)
{
	int hfd, r = 0;
	struct stat st;
	uint64_t obj;
	char *data;
	char parent[1024];
	const char *slash;

	hfd = open(hostfile, O_RDONLY);
	if (hfd < 0)
	{
		perror(hostfile);
		return -1;
	}
	fstat(hfd, &st);
	data = malloc((size_t)st.st_size ? (size_t)st.st_size : 1);
	if (read(hfd, data, (size_t)st.st_size) != st.st_size)
	{
		perror("read");
		close(hfd);
		free(data);
		return -1;
	}
	close(hfd);

	slash = strrchr(fspath, '/');
	if (slash && slash != fspath)
	{
		size_t n = (size_t)(slash - fspath);
		memcpy(parent, fspath, n);
		parent[n] = '\0';
		mkdirs(&m->fs, parent);
	}
	if (ubfs_fs_lookup(&m->fs, fspath, &obj) == 0)
		ubfs_fs_unlink(&m->fs, fspath);
	if (ubfs_fs_create(&m->fs, fspath, st.st_mode & 07777, 0, 0, &obj) < 0)
		r = -1;
	else if (st.st_size > 0 && ubfs_fs_write(&m->fs, obj, 0, data, (uint64_t)st.st_size) < 0)
		r = -1;
	free(data);
	return r;
}

static int cp_in(const char *hostfile, const char *img, const char *fspath)
{
	struct mount m;
	int r;

	if (mount_pool(&m, img, 1) < 0)
		return 1;
	r = copy_one(&m, hostfile, fspath);
	if (r == 0)
		r = mount_sync(&m);
	mount_close(&m);
	if (r < 0)
		fprintf(stderr, "cp -> %s failed\n", fspath);
	return r < 0 ? 1 : 0;
}

/**
 * Recreate a host symlink inside the pool, preserving its literal target.
 *
 * The host link is read with readlink(2) (its target is copied verbatim — a
 * relative target stays relative), the destination's parent dirs are created,
 * and any pre-existing entry at @fspath is replaced.
 *
 * @return 0 on success, -1 on failure.
 */
static int copy_link(struct mount *m, const char *hostlink, const char *fspath)
{
	char target[1024], parent[1024];
	ssize_t n;
	uint64_t obj;
	const char *slash;

	n = readlink(hostlink, target, sizeof(target) - 1);
	if (n < 0)
	{
		perror(hostlink);
		return -1;
	}
	target[n] = '\0';

	slash = strrchr(fspath, '/');
	if (slash && slash != fspath)
	{
		size_t plen = (size_t)(slash - fspath);
		memcpy(parent, fspath, plen);
		parent[plen] = '\0';
		mkdirs(&m->fs, parent);
	}
	if (ubfs_fs_lookup(&m->fs, fspath, &obj) == 0)
		ubfs_fs_unlink(&m->fs, fspath);
	return ubfs_fs_symlink(&m->fs, fspath, target, 0, 0, &obj) < 0 ? -1 : 0;
}

/* Recursively copy a host directory tree into the pool (regular files, dirs, and
 * symlinks; other node types are skipped).  Reuses the open mount in @m. */
static int copy_tree(struct mount *m, const char *hostdir, const char *fsdir)
{
	DIR *d = opendir(hostdir);
	struct dirent *de;
	uint64_t obj;
	int r = 0;

	if (!d)
	{
		perror(hostdir);
		return -1;
	}
	if (ubfs_fs_lookup(&m->fs, fsdir, &obj) != 0)
		mkdirs(&m->fs, fsdir);

	while ((de = readdir(d)) != NULL)
	{
		char hpath[2048], fpath[1024];
		struct stat st;

		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		snprintf(hpath, sizeof(hpath), "%s/%s", hostdir, de->d_name);
		snprintf(fpath, sizeof(fpath), "%s/%s", fsdir, de->d_name);
		if (lstat(hpath, &st) != 0)
			continue;
		if (S_ISDIR(st.st_mode))
		{
			if (copy_tree(m, hpath, fpath) < 0)
				r = -1;
		}
		else if (S_ISREG(st.st_mode))
		{
			if (copy_one(m, hpath, fpath) < 0)
			{
				fprintf(stderr, "cpr: %s failed\n", fpath);
				r = -1;
			}
		}
		else if (S_ISLNK(st.st_mode))
		{
			if (copy_link(m, hpath, fpath) < 0)
			{
				fprintf(stderr, "cpr: symlink %s failed\n", fpath);
				r = -1;
			}
		}
	}
	closedir(d);
	return r;
}

/* cpr <hostdir> <img>:<fsdir> — recursive copy, one mount/sync cycle. */
static int cmd_cpr(const char *hostdir, const char *dest)
{
	char img[1024];
	const char *fsdir;
	struct mount m;
	int r;

	if (!imgref(dest, img, &fsdir))
	{
		fprintf(stderr, "cpr: destination must be an image ref (img:/path)\n");
		return 1;
	}
	if (mount_pool(&m, img, 1) < 0)
		return 1;
	r = copy_tree(&m, hostdir, fsdir);
	if (r == 0)
		r = mount_sync(&m);
	mount_close(&m);
	return r < 0 ? 1 : 0;
}

static int cp_out(const char *img, const char *fspath, const char *hostfile)
{
	struct mount m;
	uint64_t obj;
	ubfs_inode_t in;
	FILE *out;
	uint64_t off = 0;

	if (mount_pool(&m, img, 0) < 0)
		return 1;
	if (ubfs_fs_lookup(&m.fs, fspath, &obj) < 0 || ubfs_fs_getattr(&m.fs, obj, &in) < 0)
	{
		fprintf(stderr, "%s: not found\n", fspath);
		mount_close(&m);
		return 1;
	}
	out = fopen(hostfile, "wb");
	if (!out)
	{
		perror(hostfile);
		mount_close(&m);
		return 1;
	}
	while (off < in.size)
	{
		char buf[UBFS_BLOCK_SIZE];
		int n = ubfs_fs_read(&m.fs, obj, off, buf, sizeof(buf));
		if (n <= 0)
			break;
		fwrite(buf, 1, (size_t)n, out);
		off += n;
	}
	fclose(out);
	mount_close(&m);
	return 0;
}

static int cmd_cp(const char *a, const char *b)
{
	char img[1024];
	const char *fspath;

	if (imgref(a, img, &fspath))
		return cp_out(img, fspath, b);
	if (imgref(b, img, &fspath))
		return cp_in(a, img, fspath);
	fprintf(stderr, "cp: one argument must be an image ref (img:/path)\n");
	return 1;
}

/* set <img> recordsize=<n> [dataset] — change a dataset property (affects only
 * files created afterward; existing files keep their recordsize). */
static int cmd_set(const char *img, const char *kv, const char *dsname)
{
	struct mount m;
	uint64_t ds_obj, rsz;
	int r;

	if (strncmp(kv, "recordsize=", 11) != 0)
	{
		fprintf(stderr, "set: only recordsize=<n> is supported (e.g. recordsize=128k)\n");
		return 1;
	}
	rsz = (uint64_t)parse_size(kv + 11);
	if (mount_pool(&m, img, 1) < 0)
		return 1;
	if (ubfs_dsl_lookup(&m.dsl, dsname, &ds_obj) < 0)
	{
		fprintf(stderr, "set: no dataset '%s'\n", dsname);
		mount_close(&m);
		return 1;
	}
	r = ubfs_dsl_set_recordsize(&m.dsl, ds_obj, rsz);
	if (r == 0)
		r = ubfs_dsl_sync(&m.dsl);
	mount_close(&m);
	if (r < 0)
	{
		fprintf(
		    stderr, "set: recordsize=%llu invalid (must be a power of two, 4K..1M)\n", (unsigned long long)rsz);
		return 1;
	}
	printf("set %s recordsize=%llu\n", dsname, (unsigned long long)rsz);
	return 0;
}

/* get <img> [dataset] — show a dataset's properties. */
static int cmd_get(const char *img, const char *dsname)
{
	struct mount m;
	uint64_t ds_obj;
	ubfs_dataset_phys_t dp;
	ubfs_dmu_os_t os;

	if (mount_pool(&m, img, 0) < 0)
		return 1;
	if (ubfs_dsl_lookup(&m.dsl, dsname, &ds_obj) < 0 || ubfs_dsl_open_dataset(&m.dsl, ds_obj, &dp, &os) < 0)
	{
		fprintf(stderr, "get: no dataset '%s'\n", dsname);
		mount_close(&m);
		return 1;
	}
	printf("%-12s recordsize=%-8llu compression=%s\n",
	       dsname,
	       (unsigned long long)dp.recordsize,
	       dp.compression ? "on" : "off");
	mount_close(&m);
	return 0;
}

/**
 * ln <img> <target> <linkpath> — create a symlink at <linkpath> in the pool
 * whose contents are the literal string <target> (stored verbatim, not
 * resolved).  Parent directories are created and any existing entry replaced.
 *
 * @return 0 on success, 1 on failure.
 */
static int cmd_ln(const char *img, const char *target, const char *linkpath)
{
	struct mount m;
	uint64_t obj;
	const char *slash;
	int r;

	if (mount_pool(&m, img, 1) < 0)
		return 1;
	slash = strrchr(linkpath, '/');
	if (slash && slash != linkpath)
	{
		char parent[1024];
		size_t plen = (size_t)(slash - linkpath);
		memcpy(parent, linkpath, plen);
		parent[plen] = '\0';
		mkdirs(&m.fs, parent);
	}
	if (ubfs_fs_lookup(&m.fs, linkpath, &obj) == 0)
		ubfs_fs_unlink(&m.fs, linkpath);
	r = ubfs_fs_symlink(&m.fs, linkpath, target, 0, 0, &obj);
	if (r == 0)
		r = mount_sync(&m);
	mount_close(&m);
	if (r < 0)
		fprintf(stderr, "ln: failed to create %s -> %s\n", linkpath, target);
	return r < 0 ? 1 : 0;
}

static void usage(void)
{
	fprintf(stderr,
	        "usage:\n"
	        "  ubfs mkpool <img> <size>\n"
	        "  ubfs mkdir  <img> <path>\n"
	        "  ubfs cp     <hostfile> <img>:<path>   (copy in)\n"
	        "  ubfs cp     <img>:<path> <hostfile>   (copy out)\n"
	        "  ubfs cpr    <hostdir>  <img>:<path>   (recursive copy in)\n"
	        "  ubfs ln     <img> <target> <linkpath> (create a symlink)\n"
	        "  ubfs ls     <img> <path>\n"
	        "  ubfs rm     <img> <path>\n"
	        "  ubfs set    <img> recordsize=<n> [dataset]\n"
	        "  ubfs get    <img> [dataset]\n");
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		usage();
		return 2;
	}
	if (!strcmp(argv[1], "mkpool") && argc == 4)
		return cmd_mkpool(argv[2], argv[3]);
	if (!strcmp(argv[1], "mkdir") && argc == 4)
		return cmd_mkdir(argv[2], argv[3]);
	if (!strcmp(argv[1], "cp") && argc == 4)
		return cmd_cp(argv[2], argv[3]);
	if (!strcmp(argv[1], "cpr") && argc == 4)
		return cmd_cpr(argv[2], argv[3]);
	if (!strcmp(argv[1], "ln") && argc == 5)
		return cmd_ln(argv[2], argv[3], argv[4]);
	if (!strcmp(argv[1], "ls") && argc == 4)
		return cmd_ls(argv[2], argv[3]);
	if (!strcmp(argv[1], "rm") && argc == 4)
		return cmd_rm(argv[2], argv[3]);
	if (!strcmp(argv[1], "set") && (argc == 4 || argc == 5))
		return cmd_set(argv[2], argv[3], argc == 5 ? argv[4] : ROOT_DS);
	if (!strcmp(argv[1], "get") && (argc == 3 || argc == 4))
		return cmd_get(argv[2], argc == 4 ? argv[3] : ROOT_DS);
	usage();
	return 2;
}
