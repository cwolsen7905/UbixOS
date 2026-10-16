/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubpkg helpers: diagnostics, growable buffers and whole-file I/O.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ubpkg.h"

/**
 * Print `ubpkg: <message>` to stderr.
 */
void ub_err(const char *fmt, ...)
{
	va_list ap;

	fputs("ubpkg: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

/**
 * Append bytes to a buffer.
 * @return 0 or -ENOMEM.
 */
int ub_buf_add(struct ub_buf *b, const void *data, size_t len)
{
	if (b->n + len + 1 > b->cap)
	{
		size_t cap = b->cap ? b->cap : 4096;
		uint8_t *np;

		while (cap < b->n + len + 1)
			cap *= 2;
		np = realloc(b->p, cap);
		if (np == NULL)
			return -ENOMEM;
		b->p = np;
		b->cap = cap;
	}
	memcpy(b->p + b->n, data, len);
	b->n += len;
	b->p[b->n] = '\0';
	return 0;
}

/**
 * Append formatted text to a buffer.
 * @return 0 or -ENOMEM/-EINVAL.
 */
int ub_buf_printf(struct ub_buf *b, const char *fmt, ...)
{
	char tmp[2048];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(tmp, sizeof tmp, fmt, ap);
	va_end(ap);
	if (n < 0 || (size_t)n >= sizeof tmp)
		return -EINVAL;
	return ub_buf_add(b, tmp, (size_t)n);
}

/**
 * Read a whole file into a NUL-terminated heap buffer (caller frees).
 * @return 0 or -errno.
 */
int ub_read_file(const char *path, uint8_t **buf, size_t *len)
{
	struct ub_buf b = {0};
	uint8_t chunk[8192];
	int fd = open(path, O_RDONLY);

	if (fd < 0)
		return -errno;
	for (;;)
	{
		ssize_t k = read(fd, chunk, sizeof chunk);

		if (k < 0)
		{
			int e = errno;

			close(fd);
			free(b.p);
			return -e;
		}
		if (k == 0)
			break;
		if (ub_buf_add(&b, chunk, (size_t)k) != 0)
		{
			close(fd);
			free(b.p);
			return -ENOMEM;
		}
	}
	close(fd);
	if (b.p == NULL && ub_buf_add(&b, "", 0) != 0)
		return -ENOMEM;
	*buf = b.p;
	*len = b.n;
	return 0;
}

/**
 * Create a new file (never overwrites) and write data to it.
 * @return 0 or -errno (-EEXIST if the path exists).
 */
int ub_write_new(const char *path, const void *data, size_t len, unsigned mode)
{
	const uint8_t *p = data;
	int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, mode);

	if (fd < 0)
		return -errno;
	while (len > 0)
	{
		ssize_t k = write(fd, p, len);

		if (k < 0)
		{
			int e = errno;

			close(fd);
			unlink(path);
			return -e;
		}
		p += k;
		len -= (size_t)k;
	}
	if (close(fd) != 0)
		return -errno;
	return 0;
}

/**
 * SHA-256 and size of a file.
 * @return 0 or -errno.
 */
int ub_sha256_file(const char *path, uint8_t out[PKG_SHA256_LEN], uint64_t *size)
{
	struct pkg_sha256 s;
	uint8_t chunk[16384];
	uint64_t total = 0;
	int fd = open(path, O_RDONLY);

	if (fd < 0)
		return -errno;
	pkg_sha256_init(&s);
	for (;;)
	{
		ssize_t k = read(fd, chunk, sizeof chunk);

		if (k < 0)
		{
			int e = errno;

			close(fd);
			return -e;
		}
		if (k == 0)
			break;
		pkg_sha256_update(&s, chunk, (size_t)k);
		total += (uint64_t)k;
	}
	close(fd);
	pkg_sha256_final(&s, out);
	if (size != NULL)
		*size = total;
	return 0;
}

/**
 * Load a public key file (`ecdsa-p256: <hex>`).
 * @return 0 or -errno/-EINVAL.
 */
int ub_load_pubkey(const char *path, struct pkg_key *key)
{
	uint8_t *buf;
	size_t len;
	int rc = ub_read_file(path, &buf, &len);

	if (rc != 0)
		return rc;
	rc = pkg_key_parse((const char *)buf, len, key);
	free(buf);
	return rc;
}

/**
 * mkdir -p.
 * @return 0 or -errno.
 */
int ub_mkdirs(const char *path)
{
	char tmp[PKG_PATH_MAX * 2];
	size_t n = strlen(path);
	size_t i;

	if (n >= sizeof tmp)
		return -ENAMETOOLONG;
	memcpy(tmp, path, n + 1);
	for (i = 1; tmp[i] != '\0'; i++)
	{
		if (tmp[i] != '/')
			continue;
		tmp[i] = '\0';
		if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
			return -errno;
		tmp[i] = '/';
	}
	if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
		return -errno;
	return 0;
}

/**
 * @return the last path component.
 */
const char *ub_basename(const char *path)
{
	const char *s = strrchr(path, '/');

	return s != NULL ? s + 1 : path;
}
