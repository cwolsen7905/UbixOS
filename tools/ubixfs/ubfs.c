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
 *   ubfs ls     <img> <path>            list a directory (or stat one entry)
 *   ubfs rm     <img> <path>            remove a file / empty directory
 *
 * All filesystem ops act on the pool's "root" dataset.
 */
#include <ubfs_fs.h>
#include <ubfs_dsl.h>
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

static int cp_in(const char *hostfile, const char *img, const char *fspath)
{
	struct mount m;
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
		return 1;
	}
	fstat(hfd, &st);
	data = malloc((size_t)st.st_size ? (size_t)st.st_size : 1);
	if (read(hfd, data, (size_t)st.st_size) != st.st_size)
	{
		perror("read");
		close(hfd);
		return 1;
	}
	close(hfd);

	if (mount_pool(&m, img, 1) < 0)
	{
		free(data);
		return 1;
	}
	/* ensure parent dirs exist */
	slash = strrchr(fspath, '/');
	if (slash && slash != fspath)
	{
		size_t n = (size_t)(slash - fspath);
		memcpy(parent, fspath, n);
		parent[n] = '\0';
		mkdirs(&m.fs, parent);
	}
	/* overwrite if it already exists */
	if (ubfs_fs_lookup(&m.fs, fspath, &obj) == 0)
		ubfs_fs_unlink(&m.fs, fspath);
	if (ubfs_fs_create(&m.fs, fspath, st.st_mode & 07777, 0, 0, &obj) < 0)
		r = -1;
	else if (st.st_size > 0 && ubfs_fs_write(&m.fs, obj, 0, data, (uint64_t)st.st_size) < 0)
		r = -1;
	if (r == 0)
		r = mount_sync(&m);
	mount_close(&m);
	free(data);
	if (r < 0)
		fprintf(stderr, "cp -> %s failed\n", fspath);
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

static void usage(void)
{
	fprintf(stderr,
	        "usage:\n"
	        "  ubfs mkpool <img> <size>\n"
	        "  ubfs mkdir  <img> <path>\n"
	        "  ubfs cp     <hostfile> <img>:<path>   (copy in)\n"
	        "  ubfs cp     <img>:<path> <hostfile>   (copy out)\n"
	        "  ubfs ls     <img> <path>\n"
	        "  ubfs rm     <img> <path>\n");
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
	if (!strcmp(argv[1], "ls") && argc == 4)
		return cmd_ls(argv[2], argv[3]);
	if (!strcmp(argv[1], "rm") && argc == 4)
		return cmd_rm(argv[2], argv[3]);
	usage();
	return 2;
}
