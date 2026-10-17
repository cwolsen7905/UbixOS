/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * pkg — file, path and lock helpers.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pkg.h"

/**
 * Join the root prefix and a package-relative path.
 * @return 0, or -ENAMETOOLONG.
 */
int ctx_path(const struct ctx *c, char *out, size_t n, const char *rel)
{
	int len;

	while (*rel == '/')
		rel++;
	len = snprintf(out, n, "%s/%s", c->root, rel);
	return (len < 0 || (size_t)len >= n) ? -ENAMETOOLONG : 0;
}

/**
 * Read a whole file into a NUL-terminated heap buffer.
 * @param max  Refuse files larger than this (-EFBIG).
 * @return 0, -errno.
 */
int read_whole(const char *path, char **buf, size_t *len, size_t max)
{
	struct stat st;
	char *b;
	size_t got = 0;
	int fd;

	if ((fd = open(path, O_RDONLY)) < 0)
		return -errno;
	if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) || (uint64_t)st.st_size > max)
	{
		int e = (fstat(fd, &st) < 0) ? errno : ((uint64_t)st.st_size > max ? EFBIG : EINVAL);

		close(fd);
		return -e;
	}
	if ((b = malloc((size_t)st.st_size + 1)) == NULL)
	{
		close(fd);
		return -ENOMEM;
	}
	while (got < (size_t)st.st_size)
	{
		ssize_t r = read(fd, b + got, (size_t)st.st_size - got);

		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
		{
			free(b);
			close(fd);
			return r < 0 ? -errno : -EIO;
		}
		got += (size_t)r;
	}
	close(fd);
	b[got] = '\0';
	*buf = b;
	*len = got;
	return 0;
}

/**
 * Create (or truncate) a file with the given contents.
 * @return 0, -errno.
 */
int write_whole(const char *path, const void *data, size_t len, unsigned mode)
{
	const char *p = data;
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);

	if (fd < 0)
		return -errno;
	while (len > 0)
	{
		ssize_t w = write(fd, p, len);

		if (w < 0 && errno == EINTR)
			continue;
		if (w <= 0)
		{
			int e = w < 0 ? errno : EIO;

			close(fd);
			return -e;
		}
		p += w;
		len -= (size_t)w;
	}
	return close(fd) < 0 ? -errno : 0;
}

/**
 * Stream a regular file through SHA-256.
 * @param size  If non-NULL receives the byte count.
 * @return 0, -errno (-EINVAL if not a regular file; symlinks are not followed).
 */
int file_sha256(const char *path, uint8_t out[PKG_SHA256_LEN], uint64_t *size)
{
	struct pkg_sha256 s;
	struct stat st;
	static unsigned char buf[8192];
	uint64_t total = 0;
	int fd;

	if (lstat(path, &st) < 0)
		return -errno;
	if (!S_ISREG(st.st_mode))
		return -EINVAL;
	if ((fd = open(path, O_RDONLY)) < 0)
		return -errno;
	pkg_sha256_init(&s);
	for (;;)
	{
		ssize_t r = read(fd, buf, sizeof buf);

		if (r < 0 && errno == EINTR)
			continue;
		if (r < 0)
		{
			int e = errno;

			close(fd);
			return -e;
		}
		if (r == 0)
			break;
		pkg_sha256_update(&s, buf, (size_t)r);
		total += (uint64_t)r;
	}
	close(fd);
	pkg_sha256_final(&s, out);
	if (size != NULL)
		*size = total;
	return 0;
}

/**
 * Check that no existing parent component of rel (below the root) is a symlink,
 * so an install can never be redirected outside its tree.
 * @return 0, -ELOOP if a component is a symlink, -ENOTDIR if one is a file.
 */
int path_through_symlink(const struct ctx *c, const char *rel)
{
	char abs[PKG_ABS_MAX];
	size_t i, base;
	struct stat st;

	if (ctx_path(c, abs, sizeof abs, rel) != 0)
		return -ENAMETOOLONG;
	base = strlen(c->root) + 1;
	for (i = base; abs[i] != '\0'; i++)
	{
		if (abs[i] != '/')
			continue;
		abs[i] = '\0';
		if (lstat(abs, &st) == 0)
		{
			if (S_ISLNK(st.st_mode))
				return -ELOOP;
			if (!S_ISDIR(st.st_mode))
				return -ENOTDIR;
		}
		else if (errno == ENOENT)
		{
			return 0; /* nothing below exists */
		}
		abs[i] = '/';
	}
	return 0;
}

/**
 * mkdir -p for a package-relative directory (mode 0755).
 * @return 0, -errno.
 */
int make_dirs(const struct ctx *c, const char *rel)
{
	char abs[PKG_ABS_MAX];
	size_t i;

	if (ctx_path(c, abs, sizeof abs, rel) != 0)
		return -ENAMETOOLONG;
	for (i = strlen(c->root) + 1;; i++)
	{
		char save = abs[i];

		if (save == '/' || save == '\0')
		{
			abs[i] = '\0';
			if (mkdir(abs, 0755) < 0 && errno != EEXIST)
				return -errno;
			abs[i] = save;
		}
		if (save == '\0')
			return 0;
	}
}

/** @return 1 if name is a legal package name (no '/', not hidden, not "base"). */
int name_is_valid(const char *name)
{
	size_t i;

	if (name[0] == '\0' || name[0] == '.' || strlen(name) >= PKG_NAME_MAX || strcmp(name, "base") == 0)
		return 0;
	for (i = 0; name[i] != '\0'; i++)
		if (name[i] == '/' || name[i] <= ' ' || name[i] == 0x7f)
			return 0;
	return 1;
}

/**
 * Take the single-transaction lock, /var/lock/pkg.lock, holding the owner's
 * pid.  A lock whose owner no longer exists is stolen.
 * @return 0, -EBUSY if another pkg holds it, -errno.
 */
int lock_take(struct ctx *c)
{
	char path[PKG_ABS_MAX], pid[32];
	int fd, tries, rc;

	if ((rc = make_dirs(c, "var/lock")) != 0)
		return rc;
	if (ctx_path(c, path, sizeof path, "var/lock/pkg.lock") != 0)
		return -ENAMETOOLONG;
	for (tries = 0; tries < 2; tries++)
	{
		if ((fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644)) >= 0)
		{
			snprintf(pid, sizeof pid, "%d\n", (int)getpid());
			(void)!write(fd, pid, strlen(pid));
			c->lockfd = fd;
			return 0;
		}
		if (errno == EEXIST)
		{
			char *txt;
			size_t len;
			long owner;

			if (read_whole(path, &txt, &len, 64) == 0)
			{
				owner = strtol(txt, NULL, 10);
				free(txt);
				if (owner > 0 && (kill((pid_t)owner, 0) == 0 || errno == EPERM))
					return -EBUSY;
			}
			(void)unlink(path); /* stale */
			continue;
		}
		return -errno;
	}
	return -EBUSY;
}

/** Release the lock taken by lock_take. */
void lock_drop(struct ctx *c)
{
	char path[PKG_ABS_MAX];

	if (c->lockfd < 0)
		return;
	close(c->lockfd);
	c->lockfd = -1;
	if (ctx_path(c, path, sizeof path, "var/lock/pkg.lock") == 0)
		(void)unlink(path);
}
