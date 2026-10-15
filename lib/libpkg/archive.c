/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * libpkg — streaming reader for a gzip-compressed ustar archive.  Accepts
 * regular files, directories and symlinks, plus the pax `path` record for long
 * names; everything else is refused (docs/design/packages-plan.md section 1).
 *
 * Extraction callers must still create files without following symlinks: a
 * member path that passes through an earlier symlink member is not detected
 * here.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <ubix/pkg.h>
#include "pkg_internal.h"

#define TAR_BLOCK 512
#define PAX_MAX 8192

/** Start reading an archive. */
int pkg_reader_open(struct pkg_reader *r, pkg_read_fn read, void *ctx, unsigned flags, const char *prefix)
{
	z_stream *z;

	memset(r, 0, sizeof *r);
	z = calloc(1, sizeof *z);
	if (z == NULL)
		return -ENOMEM;
	if (inflateInit2(z, 15 + 16) != Z_OK) /* gzip framing only */
	{
		free(z);
		return -ENOMEM;
	}
	r->z = z;
	r->read = read;
	r->ctx = ctx;
	r->flags = flags;
	r->prefix = prefix;
	r->first = 1;
	return 0;
}

/** Release the reader. */
void pkg_reader_close(struct pkg_reader *r)
{
	if (r->z != NULL)
	{
		(void)inflateEnd(r->z); /* nothing to report on teardown */
		free(r->z);
		r->z = NULL;
	}
}

/**
 * Inflate exactly n bytes (fewer only at the end of the compressed stream).
 * @return bytes produced, or -errno.
 */
static long inf_read(struct pkg_reader *r, void *buf, size_t n)
{
	z_stream *z = r->z;
	size_t got = 0;

	if (r->err != 0)
		return r->err;
	z->next_out = buf;
	while (got < n && !r->stream_end)
	{
		int zr;

		if (z->avail_in == 0 && !r->in_eof)
		{
			long k = r->read(r->ctx, r->in, sizeof r->in);

			if (k < 0)
				return r->err = (int)k;
			if (k == 0)
				r->in_eof = 1;
			z->next_in = r->in;
			z->avail_in = (uInt)k;
		}
		z->avail_out = (uInt)(n - got);
		zr = inflate(z, Z_NO_FLUSH);
		got = n - z->avail_out;
		if (zr == Z_STREAM_END)
			r->stream_end = 1;
		else if (zr == Z_BUF_ERROR && r->in_eof && z->avail_in == 0)
			return r->err = -EINVAL; /* truncated stream */
		else if (zr != Z_OK && zr != Z_BUF_ERROR)
			return r->err = -EINVAL; /* corrupt deflate data */
	}
	return (long)got;
}

/** Discard n decompressed bytes.  @return 0 or -errno (-EINVAL if short). */
static int inf_skip(struct pkg_reader *r, uint64_t n)
{
	uint8_t scratch[TAR_BLOCK];

	while (n > 0)
	{
		size_t c = n > sizeof scratch ? sizeof scratch : (size_t)n;
		long k = inf_read(r, scratch, c);

		if (k < 0)
			return (int)k;
		if ((size_t)k != c)
			return r->err = -EINVAL;
		n -= c;
	}
	return 0;
}

/** Parse a NUL/space-terminated octal field.  @return 0 or -EINVAL. */
static int octal(const uint8_t *f, size_t n, uint64_t *out)
{
	uint64_t v = 0;
	size_t i = 0;
	int any = 0;

	while (i < n && f[i] == ' ')
		i++;
	for (; i < n && f[i] >= '0' && f[i] <= '7'; i++)
	{
		v = (v << 3) | (uint64_t)(f[i] - '0');
		any = 1;
		if (v >> 60)
			return -EINVAL;
	}
	for (; i < n; i++)
	{
		if (f[i] != ' ' && f[i] != '\0')
			return -EINVAL;
	}
	if (!any)
		return -EINVAL;
	*out = v;
	return 0;
}

/** @return nonzero if the 512-byte block is all zero. */
static int is_zero(const uint8_t *b)
{
	size_t i;

	for (i = 0; i < TAR_BLOCK; i++)
	{
		if (b[i] != 0)
			return 0;
	}
	return 1;
}

/** Copy a possibly unterminated header string field.  @return length. */
static size_t field_len(const uint8_t *f, size_t n)
{
	size_t l = 0;

	while (l < n && f[l] != '\0')
		l++;
	return l;
}

/** Parse pax records, accepting only `path`.  @return 0 or -EINVAL. */
static int pax_parse(const char *buf, size_t len, char *path, size_t pathsz, int *have)
{
	size_t i = 0;

	while (i < len)
	{
		uint64_t reclen = 0;
		size_t start = i;
		size_t k;
		size_t keylen;

		while (i < len && buf[i] >= '0' && buf[i] <= '9' && reclen < PAX_MAX)
			reclen = reclen * 10 + (uint64_t)(buf[i++] - '0');
		if (i >= len || buf[i] != ' ' || reclen < i - start + 4 || reclen > len - start)
			return -EINVAL;
		k = i + 1;
		i = start + (size_t)reclen;
		if (buf[i - 1] != '\n')
			return -EINVAL;
		for (keylen = 0; k + keylen < i - 1 && buf[k + keylen] != '='; keylen++)
			;
		if (k + keylen >= i - 1 || keylen != 4 || memcmp(buf + k, "path", 4) != 0)
			return -EINVAL;
		k += keylen + 1;
		if (pkg_kv_copy(path, pathsz, buf + k, i - 1 - k) != 0)
			return -EINVAL;
		*have = 1;
	}
	return 0;
}

/** @return 1 if the path is allowed under the reader's prefix. */
static int under_prefix(const struct pkg_reader *r, const char *path)
{
	size_t pl;

	if (r->prefix == NULL)
		return 1;
	pl = strlen(r->prefix);
	if (strncmp(path, r->prefix, pl) == 0)
		return 1;
	return pl > 0 && strlen(path) + 1 == pl && strncmp(path, r->prefix, pl - 1) == 0;
}

/** Advance to the next archive member. */
int pkg_reader_next(struct pkg_reader *r, struct pkg_entry *e)
{
	uint8_t blk[TAR_BLOCK];
	char paxpath[PKG_PATH_MAX];
	int have_pax = 0;
	int rc;

	if (r->done)
		return 0;
	rc = inf_skip(r, r->remaining + r->pad);
	r->remaining = r->pad = 0;
	if (rc != 0)
		return rc;

	for (;;)
	{
		long k = inf_read(r, blk, sizeof blk);
		uint64_t sum;
		uint64_t want = 0;
		uint64_t size;
		uint64_t mode;
		uint64_t mtime = 0;
		size_t i;
		size_t nl;
		char typ;

		if (k < 0)
			return (int)k;
		if (k != (long)sizeof blk)
			return r->err = -EINVAL; /* no end-of-archive marker */
		if (is_zero(blk))
		{
			k = inf_read(r, blk, sizeof blk);
			if (k < 0)
				return (int)k;
			if (k != (long)sizeof blk || !is_zero(blk))
				return r->err = -EINVAL;
			r->done = 1;
			return r->first ? -EPERM : 0;
		}
		for (sum = 0, i = 0; i < TAR_BLOCK; i++)
			sum += (i >= 148 && i < 156) ? ' ' : blk[i];
		if (octal(blk + 148, 8, &want) != 0 || want != sum)
			return r->err = -EINVAL;
		if (memcmp(blk + 257, "ustar", 5) != 0)
			return r->err = -EINVAL;
		if (octal(blk + 124, 12, &size) != 0 || octal(blk + 100, 8, &mode) != 0)
			return r->err = -EINVAL;
		if (blk[136] != '\0' && octal(blk + 136, 12, &mtime) != 0)
			return r->err = -EINVAL;
		typ = (char)blk[156];

		if (typ == 'x')
		{
			char pax[PAX_MAX];

			if (have_pax || size == 0 || size > sizeof pax)
				return r->err = -EINVAL;
			k = inf_read(r, pax, (size_t)size);
			if (k < 0)
				return (int)k;
			if ((uint64_t)k != size)
				return r->err = -EINVAL;
			rc = inf_skip(r, (TAR_BLOCK - size % TAR_BLOCK) % TAR_BLOCK);
			if (rc != 0)
				return rc;
			if (pax_parse(pax, (size_t)size, paxpath, sizeof paxpath, &have_pax) != 0)
				return r->err = -EINVAL;
			continue;
		}
		if (typ == 'g' || typ == 'L' || typ == 'K')
			return r->err = -EINVAL;
		if (typ == '0' || typ == '\0')
			e->type = PKG_FILE;
		else if (typ == '5')
			e->type = PKG_DIR;
		else if (typ == '2')
			e->type = PKG_SYMLINK;
		else
			return r->err = -EPERM; /* hard link, device, FIFO, ... */

		if (have_pax)
		{
			memcpy(e->path, paxpath, sizeof paxpath);
		}
		else
		{
			size_t pl = field_len(blk + 345, 155);

			nl = field_len(blk, 100);
			if (pl > 0)
			{
				memcpy(e->path, blk + 345, pl);
				e->path[pl] = '/';
				memcpy(e->path + pl + 1, blk, nl);
				e->path[pl + 1 + nl] = '\0';
			}
			else
			{
				memcpy(e->path, blk, nl);
				e->path[nl] = '\0';
			}
		}
		nl = strlen(e->path);
		if (e->type == PKG_DIR && nl > 1 && e->path[nl - 1] == '/')
			e->path[nl - 1] = '\0';
		if (!pkg_path_is_safe(e->path))
			return r->err = -EPERM;

		e->link[0] = '\0';
		if (e->type == PKG_SYMLINK)
		{
			nl = field_len(blk + 157, 100);
			if (nl == 0 || pkg_kv_copy(e->link, sizeof e->link, (const char *)blk + 157, nl) != 0)
				return r->err = -EINVAL;
			size = 0;
		}
		if (e->type == PKG_DIR)
			size = 0;

		e->mode = (uint32_t)(mode & 07777);
		if ((e->mode & 06000) != 0 && !(r->flags & PKG_READ_ALLOW_SETID))
			return r->err = -EPERM;
		e->size = size;
		e->mtime = (int64_t)mtime;

		if (r->first)
		{
			if (e->type != PKG_FILE || strcmp(e->path, PKG_MANIFEST_NAME) != 0)
				return r->err = -EPERM;
			r->first = 0;
		}
		else if (!under_prefix(r, e->path))
		{
			return r->err = -EPERM;
		}
		r->remaining = size;
		r->pad = (TAR_BLOCK - size % TAR_BLOCK) % TAR_BLOCK;
		return 1;
	}
}

/** Read payload of the current member. */
long pkg_reader_read(struct pkg_reader *r, void *buf, size_t len)
{
	long k;

	if (r->remaining == 0)
		return 0;
	if ((uint64_t)len > r->remaining)
		len = (size_t)r->remaining;
	k = inf_read(r, buf, len);
	if (k < 0)
		return k;
	if ((size_t)k != len)
		return r->err = -EINVAL;
	r->remaining -= (uint64_t)k;
	return k;
}
