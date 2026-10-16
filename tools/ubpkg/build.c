/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubpkg build: turn a staging directory (a DESTDIR tree) into a
 * `<name>-<version>-r<rev>.upkg`: a gzip-compressed ustar stream whose first
 * member is the generated +MANIFEST.
 *
 * The output is reproducible: members are sorted, owners are root, and mtimes
 * are SOURCE_DATE_EPOCH (default 0).  Staging contents are hashed first and
 * re-checked while archiving, so a tree that changes underneath is refused.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
#include "ubpkg.h"

#define TAR_BLOCK 512

struct item
{
	char *path; /**< relative to the staging root */
	enum pkg_mtype type;
	uint32_t mode;
	uint64_t size;
	uint8_t sha[PKG_SHA256_LEN];
	char *target;
	int conf;
};

struct items
{
	struct item *v;
	size_t n;
	size_t cap;
};

/** gzip-writing sink. */
struct gzw
{
	FILE *f;
	z_stream z;
	uint8_t out[16384];
	int err;
};

/** Free the item list. */
static void items_free(struct items *it)
{
	size_t i;

	for (i = 0; i < it->n; i++)
	{
		free(it->v[i].path);
		free(it->v[i].target);
	}
	free(it->v);
}

/** Policy for names ubpkg will put in a line-oriented manifest.  @return 1 if ok. */
static int name_ok(const char *s)
{
	if (*s == '\0')
		return 0;
	for (; *s != '\0'; s++)
	{
		if ((unsigned char)*s <= ' ' || *s == 0x7f)
			return 0;
	}
	return 1;
}

/** @return 1 if `path` is inside the payload prefix. */
static int in_prefix(const char *path, const char *prefix)
{
	size_t pl = strlen(prefix);

	if (pl == 0 || strncmp(path, prefix, pl) == 0)
		return 1;
	return strlen(path) + 1 == pl && strncmp(path, prefix, pl - 1) == 0;
}

/** @return 1 if `path` is a proper ancestor of the prefix (not shipped, created on install). */
static int is_prefix_ancestor(const char *path, const char *prefix)
{
	size_t n = strlen(path);

	return strncmp(prefix, path, n) == 0 && prefix[n] == '/' && !in_prefix(path, prefix);
}

/** qsort by path. */
static int item_cmp(const void *a, const void *b)
{
	return strcmp(((const struct item *)a)->path, ((const struct item *)b)->path);
}

/** qsort of strings. */
static int str_cmp(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

/**
 * Recursively scan `<root>/<rel>`, appending items in sorted order.
 * @return 0 or -errno/-EPERM (unsupported file type, bad name, outside prefix).
 */
static int scan(const char *root, const char *rel, const char *prefix, struct items *it)
{
	char full[PKG_PATH_MAX * 2];
	char **names = NULL;
	size_t nn = 0;
	size_t cap = 0;
	size_t i;
	struct dirent *de;
	DIR *d;
	int rc = 0;

	snprintf(full, sizeof full, "%s/%s", root, rel);
	d = opendir(rel[0] != '\0' ? full : root);
	if (d == NULL)
		return -errno;
	while ((de = readdir(d)) != NULL)
	{
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		if (nn == cap)
		{
			char **np = realloc(names, (cap = cap ? cap * 2 : 16) * sizeof *np);

			if (np == NULL)
			{
				rc = -ENOMEM;
				break;
			}
			names = np;
		}
		names[nn] = strdup(de->d_name);
		if (names[nn] == NULL)
		{
			rc = -ENOMEM;
			break;
		}
		nn++;
	}
	closedir(d);
	if (nn > 1)
		qsort(names, nn, sizeof *names, str_cmp);
	for (i = 0; rc == 0 && i < nn; i++)
	{
		char path[PKG_PATH_MAX];
		struct stat st;
		struct item *e;

		if (snprintf(path, sizeof path, "%s%s%s", rel, rel[0] != '\0' ? "/" : "", names[i]) >=
		        (int)sizeof path ||
		    snprintf(full, sizeof full, "%s/%s", root, path) >= (int)sizeof full)
		{
			rc = -ENAMETOOLONG;
			break;
		}
		if (lstat(full, &st) != 0)
		{
			rc = -errno;
			break;
		}
		if (!name_ok(path) || !pkg_path_is_safe(path))
		{
			ub_err("%s: name not allowed in a package", path);
			rc = -EPERM;
			break;
		}
		if (S_ISDIR(st.st_mode) && is_prefix_ancestor(path, prefix))
		{
			rc = scan(root, path, prefix, it);
			continue;
		}
		if (!in_prefix(path, prefix))
		{
			ub_err("%s: outside the package prefix %s", path, prefix);
			rc = -EPERM;
			break;
		}
		if (it->n == it->cap)
		{
			struct item *np = realloc(it->v, (it->cap = it->cap ? it->cap * 2 : 64) * sizeof *np);

			if (np == NULL)
			{
				rc = -ENOMEM;
				break;
			}
			it->v = np;
		}
		e = &it->v[it->n];
		memset(e, 0, sizeof *e);
		e->path = strdup(path);
		if (e->path == NULL)
		{
			rc = -ENOMEM;
			break;
		}
		it->n++;
		e->mode = st.st_mode & 0777;
		if (st.st_mode & 06000)
		{
			ub_err("%s: setuid/setgid is not supported by ubpkg build", path);
			rc = -EPERM;
		}
		else if (S_ISDIR(st.st_mode))
		{
			e->type = PKG_M_DIR;
			rc = scan(root, path, prefix, it);
		}
		else if (S_ISREG(st.st_mode))
		{
			e->type = PKG_M_FILE;
			rc = ub_sha256_file(full, e->sha, &e->size);
			if (rc == 0 && e->size != (uint64_t)st.st_size)
				rc = -EAGAIN;
		}
		else if (S_ISLNK(st.st_mode))
		{
			char tgt[PKG_PATH_MAX];
			ssize_t k = readlink(full, tgt, sizeof tgt - 1);

			e->type = PKG_M_LINK;
			if (k <= 0 || k > 99)
			{
				ub_err("%s: symlink target empty or longer than 99 bytes", path);
				rc = -EPERM;
			}
			else
			{
				tgt[k] = '\0';
				if (!name_ok(tgt))
				{
					ub_err("%s: symlink target contains whitespace", path);
					rc = -EPERM;
				}
				else
				{
					e->target = strdup(tgt);
					rc = e->target != NULL ? 0 : -ENOMEM;
				}
			}
		}
		else
		{
			ub_err("%s: not a regular file, directory or symlink", path);
			rc = -EPERM;
		}
	}
	for (i = 0; i < nn; i++)
		free(names[i]);
	free(names);
	return rc;
}

/** Flush deflate output.  @return 0 or -EIO. */
static int gz_run(struct gzw *g, int flush)
{
	do
	{
		int zr;

		g->z.next_out = g->out;
		g->z.avail_out = sizeof g->out;
		zr = deflate(&g->z, flush);
		if (zr == Z_STREAM_ERROR)
			return g->err = -EIO;
		if (fwrite(g->out, 1, sizeof g->out - g->z.avail_out, g->f) != sizeof g->out - g->z.avail_out)
			return g->err = -EIO;
	} while (g->z.avail_out == 0);
	return 0;
}

/** Compress len bytes.  @return 0 or -EIO. */
static int gz_write(struct gzw *g, const void *data, size_t len)
{
	if (g->err != 0)
		return g->err;
	g->z.next_in = (Bytef *)(void *)(uintptr_t)data;
	g->z.avail_in = (uInt)len;
	return gz_run(g, Z_NO_FLUSH);
}

/** Pad the current member to a block boundary.  @return 0 or -EIO. */
static int tar_pad(struct gzw *g, uint64_t size)
{
	static const uint8_t zero[TAR_BLOCK];

	return gz_write(g, zero, (TAR_BLOCK - size % TAR_BLOCK) % TAR_BLOCK);
}

/** Store an octal field of `width` bytes (NUL-terminated). */
static void put_octal(uint8_t *dst, size_t width, uint64_t v)
{
	char tmp[32];

	snprintf(tmp, sizeof tmp, "%0*llo", (int)width - 1, (unsigned long long)v);
	memcpy(dst, tmp, width);
}

/** Emit one 512-byte ustar header (name already <= 100 bytes). */
static int tar_header(
    struct gzw *g, char type, const char *name, const char *link, uint32_t mode, uint64_t size, uint64_t mtime)
{
	uint8_t h[TAR_BLOCK];
	unsigned sum = 0;
	size_t i;

	memset(h, 0, sizeof h);
	memcpy(h, name, strlen(name));
	put_octal(h + 100, 8, mode);
	put_octal(h + 108, 8, 0);
	put_octal(h + 116, 8, 0);
	put_octal(h + 124, 12, size);
	put_octal(h + 136, 12, mtime);
	memset(h + 148, ' ', 8);
	h[156] = (uint8_t)type;
	if (link != NULL)
		memcpy(h + 157, link, strlen(link));
	memcpy(h + 257, "ustar", 6);
	memcpy(h + 263, "00", 2);
	for (i = 0; i < sizeof h; i++)
		sum += h[i];
	snprintf((char *)h + 148, 7, "%06o", sum);
	h[155] = ' ';
	return gz_write(g, h, sizeof h);
}

/**
 * Emit a member header, with a pax `path` record first when the name is longer
 * than the 100-byte ustar field.
 * @return 0 or -EIO.
 */
static int tar_member_header(
    struct gzw *g, char type, const char *path, const char *link, uint32_t mode, uint64_t size, uint64_t mtime)
{
	char name[101];
	size_t pl = strlen(path);

	if (pl <= 100)
		return tar_header(g, type, path, link, mode, size, mtime);
	{
		char rec[PKG_PATH_MAX + 32];
		size_t body = pl + 7; /* " path=" + path + "\n" */
		size_t total;
		int rc;

		for (total = body + 1; (size_t)snprintf(NULL, 0, "%zu", total) + body != total; total++)
			;
		snprintf(rec, sizeof rec, "%zu path=%s\n", total, path);
		if ((rc = tar_header(g, 'x', "PaxHeader", NULL, 0644, total, mtime)) != 0 ||
		    (rc = gz_write(g, rec, total)) != 0 || (rc = tar_pad(g, total)) != 0)
			return rc;
	}
	memcpy(name, path, 100);
	name[100] = '\0';
	return tar_header(g, type, name, link, mode, size, mtime);
}

/** Append one staging file's bytes, re-hashing to catch changes.  @return 0 or -errno. */
static int tar_file_data(struct gzw *g, const char *full, const struct item *it)
{
	struct pkg_sha256 s;
	uint8_t digest[PKG_SHA256_LEN];
	uint8_t buf[16384];
	uint64_t left = it->size;
	int fd = open(full, O_RDONLY);
	int rc = 0;

	if (fd < 0)
		return -errno;
	pkg_sha256_init(&s);
	while (left > 0 && rc == 0)
	{
		ssize_t k = read(fd, buf, left < sizeof buf ? (size_t)left : sizeof buf);

		if (k <= 0)
		{
			rc = k < 0 ? -errno : -EAGAIN;
			break;
		}
		pkg_sha256_update(&s, buf, (size_t)k);
		rc = gz_write(g, buf, (size_t)k);
		left -= (uint64_t)k;
	}
	close(fd);
	if (rc == 0)
	{
		pkg_sha256_final(&s, digest);
		if (memcmp(digest, it->sha, sizeof digest) != 0)
			rc = -EAGAIN;
	}
	return rc == 0 ? tar_pad(g, it->size) : rc;
}

/** Package metadata given on the command line. */
struct meta
{
	const char *name;
	const char *version;
	unsigned revision;
	const char *arch;
	unsigned abi;
	const char *base;
	const char *comment;
	const char *license;
	const char *depends;
	const char *conflicts;
	const char *recipe_hash;
};

/** Render the manifest text.  @return 0 or -errno. */
static int render_manifest(
    struct ub_buf *b, const struct meta *m, const struct items *it, const char **conf, size_t nconf)
{
	uint64_t total = 0;
	size_t i;
	size_t k;
	int rc;
	char hex[PKG_SHA256_HEX];

	for (i = 0; i < it->n; i++)
		total += it->v[i].size;
	rc = ub_buf_printf(b,
	                   "name: %s\nversion: %s\nrevision: %u\narch: %s\nabi: %u\n",
	                   m->name,
	                   m->version,
	                   m->revision,
	                   m->arch,
	                   m->abi);
	if (rc == 0 && m->base != NULL)
		rc = ub_buf_printf(b, "base: %s\n", m->base);
	if (rc == 0 && m->comment != NULL)
		rc = ub_buf_printf(b, "comment: %s\n", m->comment);
	if (rc == 0 && m->license != NULL)
		rc = ub_buf_printf(b, "license: %s\n", m->license);
	if (rc == 0 && m->depends != NULL)
		rc = ub_buf_printf(b, "depends: %s\n", m->depends);
	if (rc == 0 && m->conflicts != NULL)
		rc = ub_buf_printf(b, "conflicts: %s\n", m->conflicts);
	if (rc == 0)
		rc = ub_buf_printf(b, "installed-size: %llu\n", (unsigned long long)total);
	if (rc == 0 && m->recipe_hash != NULL)
		rc = ub_buf_printf(b, "recipe-hash: %s\n", m->recipe_hash);
	for (i = 0; rc == 0 && i < it->n; i++)
	{
		const struct item *e = &it->v[i];

		if (e->type == PKG_M_FILE)
		{
			pkg_hex_encode(e->sha, PKG_SHA256_LEN, hex);
			rc = ub_buf_printf(
			    b, "file: %04o %llu %s %s\n", e->mode, (unsigned long long)e->size, hex, e->path);
		}
		else if (e->type == PKG_M_DIR)
		{
			rc = ub_buf_printf(b, "dir: %04o %s\n", e->mode, e->path);
		}
		else
		{
			rc = ub_buf_printf(b, "link: %s -> %s\n", e->path, e->target);
		}
	}
	for (k = 0; rc == 0 && k < nconf; k++)
		rc = ub_buf_printf(b, "conf: %s\n", conf[k]);
	return rc;
}

/**
 * `ubpkg build -n name -v version -a arch -A abi [-r rev] [-b base] [-c comment]
 *  [-l license] [-d depends] [-C conflicts] [-H recipe-hash] [--conf path]...
 *  [--prefix P] -o outdir stagedir`
 * @return exit status.
 */
int cmd_build(int argc, char **argv)
{
	static const struct option lo[] = {
	    {"conf", required_argument, NULL, 'f'}, {"prefix", required_argument, NULL, 'P'}, {NULL, 0, NULL, 0}};
	struct meta m = {0};
	const char *prefix = UBPKG_DEFAULT_PREFIX;
	const char *outdir = ".";
	const char *conf[256];
	size_t nconf = 0;
	struct items it = {0};
	struct ub_buf man = {0};
	struct gzw g = {0};
	struct pkg_manifest chk = {0};
	char out[PKG_PATH_MAX];
	char full[PKG_PATH_MAX * 2];
	const char *stage;
	unsigned line = 0;
	uint64_t mtime;
	const char *sde = getenv("SOURCE_DATE_EPOCH");
	size_t i;
	int c;
	int rc = 1;
	int opened = 0;

	optind = 1;
	m.arch = NULL;
	m.abi = (unsigned)-1;
	while ((c = getopt_long(argc, argv, "n:v:r:a:A:b:c:l:d:C:H:o:", lo, NULL)) != -1)
	{
		switch (c)
		{
			case 'n':
				m.name = optarg;
				break;
			case 'v':
				m.version = optarg;
				break;
			case 'r':
				m.revision = (unsigned)strtoul(optarg, NULL, 10);
				break;
			case 'a':
				m.arch = optarg;
				break;
			case 'A':
				m.abi = (unsigned)strtoul(optarg, NULL, 10);
				break;
			case 'b':
				m.base = optarg;
				break;
			case 'c':
				m.comment = optarg;
				break;
			case 'l':
				m.license = optarg;
				break;
			case 'd':
				m.depends = optarg;
				break;
			case 'C':
				m.conflicts = optarg;
				break;
			case 'H':
				m.recipe_hash = optarg;
				break;
			case 'o':
				outdir = optarg;
				break;
			case 'f':
				if (nconf == sizeof conf / sizeof conf[0])
				{
					ub_err("too many --conf");
					return 2;
				}
				conf[nconf++] = optarg;
				break;
			case 'P':
				prefix = optarg;
				break;
			default:
				return 2;
		}
	}
	if (m.name == NULL || m.version == NULL || m.arch == NULL || m.abi == (unsigned)-1 || argc - optind != 1)
	{
		ub_err("usage: ubpkg build -n name -v version -a arch -A abi [-r rev] [-b base] [-c comment] "
		       "[-l license] [-d depends] [-C conflicts] [-H recipe-hash] [--conf path]... [--prefix p] "
		       "[-o outdir] stagedir");
		return 2;
	}
	stage = argv[optind];
	mtime = sde != NULL && sde[0] != '\0' ? strtoull(sde, NULL, 10) : 0;
	if (!name_ok(m.name) || !name_ok(m.version) || strchr(m.name, '/') != NULL || strchr(m.version, '/') != NULL)
	{
		ub_err("bad package name or version");
		return 2;
	}
	if ((c = scan(stage, "", prefix, &it)) != 0)
	{
		ub_err("%s: %s", stage, c == -EPERM ? "staging tree rejected" : strerror(-c));
		goto out;
	}
	qsort(it.v, it.n, sizeof *it.v, item_cmp);
	if ((c = render_manifest(&man, &m, &it, conf, nconf)) != 0)
	{
		ub_err("cannot render manifest");
		goto out;
	}
	if (pkg_manifest_parse((const char *)man.p, man.n, &chk, &line) != 0)
	{
		ub_err("manifest line %u is invalid (check -d/-b/-C syntax, conf paths)", line);
		goto out;
	}
	pkg_manifest_free(&chk);

	if ((c = ub_mkdirs(outdir)) != 0)
	{
		ub_err("%s: %s", outdir, strerror(-c));
		goto out;
	}
	snprintf(out, sizeof out, "%s/%s-%s-r%u.upkg", outdir, m.name, m.version, m.revision);
	{
		int fd = open(out, O_WRONLY | O_CREAT | O_EXCL, 0644);

		if (fd < 0)
		{
			ub_err("%s: %s", out, strerror(errno));
			goto out;
		}
		g.f = fdopen(fd, "wb");
		if (g.f == NULL)
		{
			close(fd);
			unlink(out);
			goto out;
		}
	}
	opened = 1;
	if (deflateInit2(&g.z, 9, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
	{
		ub_err("deflate init failed");
		goto out;
	}
	c = tar_member_header(&g, '0', PKG_MANIFEST_NAME, NULL, 0644, man.n, mtime);
	if (c == 0)
		c = gz_write(&g, man.p, man.n);
	if (c == 0)
		c = tar_pad(&g, man.n);
	for (i = 0; c == 0 && i < it.n; i++)
	{
		const struct item *e = &it.v[i];

		snprintf(full, sizeof full, "%s/%s", stage, e->path);
		if (e->type == PKG_M_DIR)
			c = tar_member_header(&g, '5', e->path, NULL, e->mode, 0, mtime);
		else if (e->type == PKG_M_LINK)
			c = tar_member_header(&g, '2', e->path, e->target, 0777, 0, mtime);
		else if ((c = tar_member_header(&g, '0', e->path, NULL, e->mode, e->size, mtime)) == 0)
			c = tar_file_data(&g, full, e);
	}
	if (c == 0)
	{
		static const uint8_t zero[TAR_BLOCK * 2];

		c = gz_write(&g, zero, sizeof zero);
	}
	if (c == 0)
	{
		g.z.next_in = NULL;
		g.z.avail_in = 0;
		do
		{
			int zr;

			g.z.next_out = g.out;
			g.z.avail_out = sizeof g.out;
			zr = deflate(&g.z, Z_FINISH);
			if (fwrite(g.out, 1, sizeof g.out - g.z.avail_out, g.f) != sizeof g.out - g.z.avail_out)
				c = -EIO;
			if (zr == Z_STREAM_END)
				break;
			if (zr != Z_OK && zr != Z_BUF_ERROR)
				c = -EIO;
		} while (c == 0);
	}
	deflateEnd(&g.z);
	if (fclose(g.f) != 0 && c == 0)
		c = -EIO;
	g.f = NULL;
	if (c != 0)
	{
		ub_err("%s: write failed (%s)", out, c == -EAGAIN ? "staging tree changed during build" : strerror(-c));
		unlink(out);
		goto out;
	}
	/* round-trip: the package we just wrote must satisfy the same checks install applies */
	if ((c = ub_pkg_extract(out, NULL, prefix, NULL)) != 0)
	{
		ub_err("%s: self-check failed (%s)", out, strerror(-c));
		unlink(out);
		goto out;
	}
	{
		uint8_t d[PKG_SHA256_LEN];
		uint64_t sz;
		char hex[PKG_SHA256_HEX];

		if (ub_sha256_file(out, d, &sz) == 0)
		{
			pkg_hex_encode(d, sizeof d, hex);
			printf("%s %llu %s\n", out, (unsigned long long)sz, hex);
		}
	}
	rc = 0;
out:
	if (g.f != NULL)
	{
		deflateEnd(&g.z);
		fclose(g.f);
		if (opened)
			unlink(out);
	}
	items_free(&it);
	free(man.p);
	return rc;
}
