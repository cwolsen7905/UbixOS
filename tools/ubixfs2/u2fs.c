/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * u2fs — mtools-style host CLI for UbixFS v2 (the test harness; see
 * docs/design/ubixfs2-plan.md).  Operates on an image file directly, no mount,
 * no root.  Subcommands:
 *   u2fs mkfs  <img> <size>        format (size like 64M, 512K, or bytes)
 *   u2fs ls    <img> <path>        list a directory
 *   u2fs mkdir <img> <path>        create a directory
 *   u2fs cp    <src> <dst>         copy in (host->img:/p) or out (img:/p->host)
 *   u2fs rm    <img> <path>        remove a file or empty directory
 * An image path is written "<img>:<fs-path>" (e.g. disk.img:/bin/sh).
 */
#include <ubixfs2_core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

/* ---- file-backed block device ---- */
struct filedev
{
	int fd;
};

static int fd_read(void *ctx, uint32_t blk, void *buf)
{
	struct filedev *d = (struct filedev *)ctx;
	off_t           off = (off_t)blk * UBIXFS2_BLOCK_SIZE;

	if (pread(d->fd, buf, UBIXFS2_BLOCK_SIZE, off) != UBIXFS2_BLOCK_SIZE)
		return -1;
	return 0;
}

static int fd_write(void *ctx, uint32_t blk, const void *buf)
{
	struct filedev *d = (struct filedev *)ctx;
	off_t           off = (off_t)blk * UBIXFS2_BLOCK_SIZE;

	if (pwrite(d->fd, buf, UBIXFS2_BLOCK_SIZE, off) != UBIXFS2_BLOCK_SIZE)
		return -1;
	return 0;
}

static int open_fs(ubixfs2_t *fs, struct filedev *d, const char *img, int rw)
{
	d->fd = open(img, rw ? O_RDWR : O_RDONLY);
	if (d->fd < 0)
	{
		perror(img);
		return -1;
	}
	fs->io_ctx = d;
	fs->read_block = fd_read;
	fs->write_block = fd_write;
	if (ubixfs2_mount(fs) < 0)
	{
		fprintf(stderr, "%s: not a UbixFS v2 image\n", img);
		return -1;
	}
	return 0;
}

static int64_t parse_size(const char *s)
{
	char   *end;
	int64_t v = strtoll(s, &end, 10);

	if (*end == 'K' || *end == 'k')
		v *= 1024;
	else if (*end == 'M' || *end == 'm')
		v *= 1024 * 1024;
	else if (*end == 'G' || *end == 'g')
		v *= 1024 * 1024 * 1024;
	return v;
}

/* ---- subcommands ---- */

static int cmd_mkfs(const char *img, const char *sizestr)
{
	int64_t  bytes = parse_size(sizestr);
	int64_t  nblocks = bytes / UBIXFS2_BLOCK_SIZE;
	int      fd = open(img, O_RDWR | O_CREAT | O_TRUNC, 0644);
	ubixfs2_t fs;
	struct filedev d;
	int      r;

	if (fd < 0)
	{
		perror(img);
		return 1;
	}
	if (ftruncate(fd, (off_t)nblocks * UBIXFS2_BLOCK_SIZE) < 0)
	{
		perror("ftruncate");
		return 1;
	}
	d.fd = fd;
	fs.io_ctx = &d;
	fs.read_block = fd_read;
	fs.write_block = fd_write;
	r = ubixfs2_format(&fs, nblocks, "UBIXFS2");
	close(fd);
	if (r < 0)
	{
		fprintf(stderr, "mkfs failed (%d)\n", r);
		return 1;
	}
	printf("mkfs: %s, %lld blocks (%lld bytes), %lld used\n", img, (long long)nblocks, (long long)bytes,
	       (long long)fs.super.used_blocks);
	return 0;
}

static int ls_cb(void *arg, const char *name, uint32_t inode_block)
{
	ubixfs2_t      *fs = (ubixfs2_t *)arg;
	ubixfs2_inode_t ino;
	char            t = '?';

	if (ubixfs2_read_inode(fs, inode_block, &ino) == 0)
		t = ((ino.mode & 0170000) == 0040000) ? 'd' : '-';
	printf("%c %04o %u:%u %10lld  %s\n", t, ino.mode & 07777, ino.uid, ino.gid, (long long)ino.blocks.size,
	       name);
	return 0;
}

static int cmd_ls(const char *img, const char *path)
{
	ubixfs2_t      fs;
	struct filedev d;
	uint32_t       blk;
	ubixfs2_inode_t ino;

	if (open_fs(&fs, &d, img, 0) < 0)
		return 1;
	if (ubixfs2_lookup(&fs, path, &blk) < 0)
	{
		fprintf(stderr, "%s: no such path\n", path);
		return 1;
	}
	if (ubixfs2_read_inode(&fs, blk, &ino) < 0)
		return 1;
	if ((ino.mode & 0170000) != 0040000)
	{
		ls_cb(&fs, ino.name, blk);
		return 0;
	}
	ubixfs2_readdir(&fs, &ino, ls_cb, &fs);
	return 0;
}

static int cmd_mkdir(const char *img, const char *path)
{
	ubixfs2_t      fs;
	struct filedev d;
	int            r;

	if (open_fs(&fs, &d, img, 1) < 0)
		return 1;
	r = ubixfs2_mkdir(&fs, path, 0755, 0, 0);
	if (r < 0)
	{
		fprintf(stderr, "mkdir %s failed (%d)\n", path, r);
		return 1;
	}
	return 0;
}

static int cmd_rm(const char *img, const char *path)
{
	ubixfs2_t      fs;
	struct filedev d;
	int            r;

	if (open_fs(&fs, &d, img, 1) < 0)
		return 1;
	r = ubixfs2_unlink(&fs, path);
	if (r < 0)
	{
		fprintf(stderr, "rm %s failed (%d)\n", path, r);
		return 1;
	}
	return 0;
}

/* Split "img:/path" into image and fs-path; returns 1 if it's an image ref. */
static int is_imgref(const char *s, char *img, const char **fspath)
{
	const char *colon = strchr(s, ':');

	if (!colon)
		return 0;
	size_t n = (size_t)(colon - s);
	memcpy(img, s, n);
	img[n] = '\0';
	*fspath = colon + 1;
	return 1;
}

static int cmd_cp(const char *src, const char *dst)
{
	char           img[1024];
	const char    *fspath;
	ubixfs2_t      fs;
	struct filedev d;

	/* copy OUT: img:/path -> hostfile */
	if (is_imgref(src, img, &fspath))
	{
		uint32_t        blk;
		ubixfs2_inode_t ino;
		FILE           *out;
		char            buf[UBIXFS2_BLOCK_SIZE];
		int64_t         off = 0;

		if (open_fs(&fs, &d, img, 0) < 0)
			return 1;
		if (ubixfs2_lookup(&fs, fspath, &blk) < 0 || ubixfs2_read_inode(&fs, blk, &ino) < 0)
		{
			fprintf(stderr, "%s: not found\n", fspath);
			return 1;
		}
		out = fopen(dst, "wb");
		if (!out)
		{
			perror(dst);
			return 1;
		}
		while (off < ino.blocks.size)
		{
			int n = ubixfs2_read_file(&fs, &ino, buf, off, sizeof(buf));
			if (n <= 0)
				break;
			fwrite(buf, 1, (size_t)n, out);
			off += n;
		}
		fclose(out);
		return 0;
	}

	/* copy IN: hostfile -> img:/path */
	if (is_imgref(dst, img, &fspath))
	{
		struct stat st;
		int         hfd = open(src, O_RDONLY);
		void       *data;
		int         r;

		if (hfd < 0)
		{
			perror(src);
			return 1;
		}
		fstat(hfd, &st);
		data = malloc((size_t)st.st_size ? (size_t)st.st_size : 1);
		if (read(hfd, data, (size_t)st.st_size) != st.st_size)
		{
			perror("read");
			return 1;
		}
		close(hfd);
		if (open_fs(&fs, &d, img, 1) < 0)
			return 1;
		r = ubixfs2_create_file(&fs, fspath, data, st.st_size, st.st_mode & 07777, 0, 0);
		free(data);
		if (r < 0)
		{
			fprintf(stderr, "cp -> %s failed (%d)\n", fspath, r);
			return 1;
		}
		return 0;
	}

	fprintf(stderr, "cp: one argument must be an image ref (img:/path)\n");
	return 1;
}

static void usage(void)
{
	fprintf(stderr, "usage:\n"
	                "  u2fs mkfs  <img> <size>\n"
	                "  u2fs ls    <img> <path>\n"
	                "  u2fs mkdir <img> <path>\n"
	                "  u2fs cp    <src> <dst>      (img:/path on one side)\n"
	                "  u2fs rm    <img> <path>\n");
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		usage();
		return 2;
	}
	if (!strcmp(argv[1], "mkfs") && argc == 4)
		return cmd_mkfs(argv[2], argv[3]);
	if (!strcmp(argv[1], "ls") && argc == 4)
		return cmd_ls(argv[2], argv[3]);
	if (!strcmp(argv[1], "mkdir") && argc == 4)
		return cmd_mkdir(argv[2], argv[3]);
	if (!strcmp(argv[1], "cp") && argc == 4)
		return cmd_cp(argv[2], argv[3]);
	if (!strcmp(argv[1], "rm") && argc == 4)
		return cmd_rm(argv[2], argv[3]);
	usage();
	return 2;
}
