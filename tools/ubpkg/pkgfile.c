/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubpkg: verify a .upkg against its own manifest and (optionally) install it
 * under a root directory.
 *
 * Install is descriptor-relative: every path component is opened with
 * O_NOFOLLOW, so a symlink planted in the root (or by an earlier package
 * member) can never redirect a write outside the root.  Files are written to
 * a temporary name and renamed into place.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ubpkg.h"

#define TMP_PREFIX ".ubpkg-tmp."
#define NEW_SUFFIX ".upkg-new"

/** Directories created by an install, chmod'ed to their final mode at the end. */
struct dirmode
{
	char *path;
	uint32_t mode;
};

struct extract
{
	struct pkg_reader rd;
	int fd;
	int rootfd; /**< -1 when only verifying */
	const struct pkg_manifest *m;
	uint8_t *seen;
	struct dirmode *dirs;
	size_t ndirs;
};

/** pkg_read_fn over a file descriptor. */
static long fd_read(void *ctx, void *buf, size_t len)
{
	ssize_t k = read(*(int *)ctx, buf, len);

	return k < 0 ? -errno : (long)k;
}

/**
 * Open (creating as needed) every directory component of `path` below rootfd.
 * A component that is a symlink or other non-directory is refused.
 * @param leaf  receives the final component.
 * @return a directory descriptor for the parent (caller closes), or -errno.
 */
static int open_parent(int rootfd, const char *path, char *leaf, size_t leafsz)
{
	char comp[PKG_PATH_MAX];
	const char *p = path;
	int cur = dup(rootfd);

	if (cur < 0)
		return -errno;
	for (;;)
	{
		const char *slash = strchr(p, '/');
		size_t n = slash != NULL ? (size_t)(slash - p) : strlen(p);
		int next;

		if (n == 0 || n >= sizeof comp)
		{
			close(cur);
			return -EINVAL;
		}
		if (slash == NULL)
		{
			if (n >= leafsz)
			{
				close(cur);
				return -ENAMETOOLONG;
			}
			memcpy(leaf, p, n);
			leaf[n] = '\0';
			return cur;
		}
		memcpy(comp, p, n);
		comp[n] = '\0';
		next = openat(cur, comp, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
		if (next < 0 && errno == ENOENT)
		{
			if (mkdirat(cur, comp, 0755) != 0)
			{
				int e = errno;

				close(cur);
				return -e;
			}
			next = openat(cur, comp, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
		}
		close(cur);
		if (next < 0)
			return -EPERM; /* symlink or non-directory in the path */
		cur = next;
		p = slash + 1;
	}
}

/** Write all of buf to fd.  @return 0 or -errno. */
static int write_all(int fd, const void *buf, size_t len)
{
	const uint8_t *p = buf;

	while (len > 0)
	{
		ssize_t k = write(fd, p, len);

		if (k < 0)
			return -errno;
		p += k;
		len -= (size_t)k;
	}
	return 0;
}

/**
 * Stream the current file member, check size and SHA-256, and (when
 * installing) write it under rootfd.
 * @return 0, -EBADMSG on a hash mismatch, or -errno.
 */
static int do_file(struct extract *x, const struct pkg_entry *e, const struct pkg_mentry *me)
{
	struct pkg_sha256 s;
	uint8_t digest[PKG_SHA256_LEN];
	uint8_t buf[16384];
	char leaf[PKG_NAME_MAX * 4];
	char tmp[sizeof leaf + 32];
	int dirfd = -1;
	int out = -1;
	int rc = 0;
	uint64_t left = e->size;

	if (x->rootfd >= 0)
	{
		dirfd = open_parent(x->rootfd, e->path, leaf, sizeof leaf);
		if (dirfd < 0)
			return dirfd;
		snprintf(tmp, sizeof tmp, TMP_PREFIX "%s", leaf);
		out = openat(dirfd, tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
		if (out < 0)
		{
			rc = -errno;
			close(dirfd);
			return rc;
		}
	}
	pkg_sha256_init(&s);
	while (left > 0 && rc == 0)
	{
		long k = pkg_reader_read(&x->rd, buf, left < sizeof buf ? (size_t)left : sizeof buf);

		if (k <= 0)
		{
			rc = k < 0 ? (int)k : -EINVAL;
			break;
		}
		pkg_sha256_update(&s, buf, (size_t)k);
		if (out >= 0)
			rc = write_all(out, buf, (size_t)k);
		left -= (uint64_t)k;
	}
	if (rc == 0)
	{
		pkg_sha256_final(&s, digest);
		if (memcmp(digest, me->sha256, PKG_SHA256_LEN) != 0)
			rc = -EBADMSG;
	}
	if (out >= 0)
	{
		if (rc == 0 && fchmod(out, (mode_t)(me->mode & 0777)) != 0)
			rc = -errno;
		if (close(out) != 0 && rc == 0)
			rc = -errno;
		if (rc == 0 && me->conf)
		{
			struct stat st;

			/* keep an existing config file; offer the new one beside it */
			if (fstatat(dirfd, leaf, &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISREG(st.st_mode))
			{
				char dst[sizeof leaf + sizeof NEW_SUFFIX];

				snprintf(dst, sizeof dst, "%s" NEW_SUFFIX, leaf);
				if (renameat(dirfd, tmp, dirfd, dst) != 0)
					rc = -errno;
				close(dirfd);
				if (rc != 0)
					return rc;
				return 0;
			}
		}
		if (rc == 0 && renameat(dirfd, tmp, dirfd, leaf) != 0)
			rc = -errno;
		if (rc != 0)
			unlinkat(dirfd, tmp, 0);
		close(dirfd);
	}
	return rc;
}

/** Create a directory member and remember its final mode.  @return 0 or -errno. */
static int do_dir(struct extract *x, const struct pkg_entry *e)
{
	char leaf[PKG_NAME_MAX * 4];
	struct stat st;
	struct dirmode *nd;
	int dirfd;

	if (x->rootfd < 0)
		return 0;
	dirfd = open_parent(x->rootfd, e->path, leaf, sizeof leaf);
	if (dirfd < 0)
		return dirfd;
	if (fstatat(dirfd, leaf, &st, AT_SYMLINK_NOFOLLOW) == 0)
	{
		close(dirfd);
		return S_ISDIR(st.st_mode) ? 0 : -EPERM; /* existing dirs keep their mode */
	}
	if (mkdirat(dirfd, leaf, 0700) != 0)
	{
		int rc = -errno;

		close(dirfd);
		return rc;
	}
	close(dirfd);
	nd = realloc(x->dirs, (x->ndirs + 1) * sizeof *nd);
	if (nd == NULL)
		return -ENOMEM;
	x->dirs = nd;
	nd[x->ndirs].path = strdup(e->path);
	if (nd[x->ndirs].path == NULL)
		return -ENOMEM;
	nd[x->ndirs].mode = e->mode & 0777;
	x->ndirs++;
	return 0;
}

/** Create a symlink member.  @return 0 or -errno. */
static int do_link(struct extract *x, const struct pkg_entry *e)
{
	char leaf[PKG_NAME_MAX * 4];
	struct stat st;
	int dirfd;
	int rc = 0;

	if (x->rootfd < 0)
		return 0;
	dirfd = open_parent(x->rootfd, e->path, leaf, sizeof leaf);
	if (dirfd < 0)
		return dirfd;
	if (fstatat(dirfd, leaf, &st, AT_SYMLINK_NOFOLLOW) == 0)
	{
		if (S_ISDIR(st.st_mode))
			rc = -EPERM;
		else if (unlinkat(dirfd, leaf, 0) != 0)
			rc = -errno;
	}
	if (rc == 0 && symlinkat(e->link, dirfd, leaf) != 0)
		rc = -errno;
	close(dirfd);
	return rc;
}

/** Slurp the +MANIFEST member and parse it.  @return 0 or -errno. */
static int read_manifest(struct extract *x, const struct pkg_entry *e, struct pkg_manifest *m)
{
	struct ub_buf b = {0};
	uint8_t buf[8192];
	unsigned line = 0;
	int rc = 0;
	uint64_t left = e->size;

	if (e->size > UBPKG_MANIFEST_MAX)
		return -EFBIG;
	while (left > 0)
	{
		long k = pkg_reader_read(&x->rd, buf, left < sizeof buf ? (size_t)left : sizeof buf);

		if (k <= 0)
		{
			free(b.p);
			return k < 0 ? (int)k : -EINVAL;
		}
		if (ub_buf_add(&b, buf, (size_t)k) != 0)
		{
			free(b.p);
			return -ENOMEM;
		}
		left -= (uint64_t)k;
	}
	rc = pkg_manifest_parse((const char *)b.p, b.n, m, &line);
	if (rc != 0)
		ub_err("manifest line %u: malformed", line);
	free(b.p);
	return rc;
}

/** Check an archive member against its manifest entry.  @return 0 or -EBADMSG. */
static int member_matches(const struct pkg_entry *e, const struct pkg_mentry *me)
{
	if (e->type == PKG_FILE && me->type == PKG_M_FILE)
		return e->size == me->size && (e->mode & 0777) == (me->mode & 0777) ? 0 : -EBADMSG;
	if (e->type == PKG_DIR && me->type == PKG_M_DIR)
		return (e->mode & 0777) == (me->mode & 0777) ? 0 : -EBADMSG;
	if (e->type == PKG_SYMLINK && me->type == PKG_M_LINK)
		return strcmp(e->link, me->target) == 0 ? 0 : -EBADMSG;
	return -EBADMSG;
}

/** Final chmod of created directories, deepest first.  @return 0 or -errno. */
static int fix_dirs(struct extract *x)
{
	size_t i = x->ndirs;
	int rc = 0;

	while (i-- > 0)
	{
		if (rc == 0 && fchmodat(x->rootfd, x->dirs[i].path, (mode_t)x->dirs[i].mode, 0) != 0)
			rc = -errno;
		free(x->dirs[i].path);
	}
	free(x->dirs);
	x->dirs = NULL;
	return rc;
}

/**
 * Verify a package against its own manifest, and install it under `root` if
 * given.  Verification covers the archive policy (libpkg reader), that every
 * member is listed in the manifest with matching type/mode/size/target/SHA-256,
 * that none is duplicated and none is missing.  An install verifies the whole
 * package first, so a bad package leaves the root untouched.
 *
 * @param prefix  required payload prefix (NULL or "" = unrestricted).
 * @param out     if non-NULL receives the parsed manifest (caller frees).
 * @return 0, -EBADMSG (content does not match manifest), -EPERM (policy),
 *         -EINVAL (malformed), or -errno.
 */
int ub_pkg_extract(const char *pkg, const char *root, const char *prefix, struct pkg_manifest *out)
{
	struct pkg_manifest m = {0};
	struct extract x = {0};
	struct pkg_entry *e;
	size_t i;
	size_t seen = 0;
	int rc;
	int pass;
	int have_m = 0;

	if (prefix != NULL && prefix[0] == '\0')
		prefix = NULL;
	e = malloc(sizeof *e);
	if (e == NULL)
		return -ENOMEM;
	x.rootfd = -1;
	for (pass = 0; pass < (root != NULL ? 2 : 1); pass++)
	{
		rc = 0;
		x.fd = open(pkg, O_RDONLY);
		if (x.fd < 0)
		{
			rc = -errno;
			break;
		}
		if (pass == 1)
		{
			if ((rc = ub_mkdirs(root)) != 0)
				break;
			x.rootfd = open(root, O_RDONLY | O_DIRECTORY);
			if (x.rootfd < 0)
			{
				rc = -errno;
				break;
			}
		}
		rc = pkg_reader_open(&x.rd, fd_read, &x.fd, 0, prefix);
		if (rc != 0)
			break;
		if ((rc = pkg_reader_next(&x.rd, e)) != 1)
		{
			rc = rc == 0 ? -EINVAL : rc;
			pkg_reader_close(&x.rd);
			break;
		}
		rc = 0;
		if (pass == 0)
		{
			rc = read_manifest(&x, e, &m);
			have_m = rc == 0;
			x.seen = calloc(m.nentries ? m.nentries : 1, 1);
			if (rc == 0 && x.seen == NULL)
				rc = -ENOMEM;
		}
		else
		{
			memset(x.seen, 0, m.nentries ? m.nentries : 1);
		}
		seen = 0;
		x.m = &m;
		while (rc == 0 && (rc = pkg_reader_next(&x.rd, e)) == 1)
		{
			const struct pkg_mentry *me = pkg_manifest_find(&m, e->path);

			rc = 0;
			if (me == NULL)
			{
				ub_err("%s: member not in manifest", e->path);
				rc = -EBADMSG;
				break;
			}
			i = (size_t)(me - m.entries);
			if (x.seen[i])
			{
				ub_err("%s: duplicate member", e->path);
				rc = -EBADMSG;
				break;
			}
			x.seen[i] = 1;
			seen++;
			if ((rc = member_matches(e, me)) != 0)
			{
				ub_err("%s: does not match manifest", e->path);
				break;
			}
			if (e->type == PKG_FILE)
				rc = do_file(&x, e, me);
			else if (e->type == PKG_DIR)
				rc = do_dir(&x, e);
			else
				rc = do_link(&x, e);
			if (rc != 0)
				ub_err("%s: %s", e->path, rc == -EBADMSG ? "checksum mismatch" : "cannot be installed");
		}
		if (rc == 0 && seen != m.nentries)
		{
			ub_err("archive lacks %zu manifest entries", m.nentries - seen);
			rc = -EBADMSG;
		}
		pkg_reader_close(&x.rd);
		if (x.rootfd >= 0)
		{
			int frc = fix_dirs(&x);

			rc = rc != 0 ? rc : frc;
		}
		close(x.fd);
		x.fd = -1;
		if (rc != 0)
			break;
	}
	if (x.fd >= 0)
		close(x.fd);
	if (x.rootfd >= 0)
		close(x.rootfd);
	free(x.seen);
	free(e);
	if (rc == 0 && out != NULL)
		*out = m;
	else if (have_m)
		pkg_manifest_free(&m);
	return rc;
}
