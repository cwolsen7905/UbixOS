/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * pkg add --local: verify, check for conflicts, install atomically, record.
 *
 * A package file is checked in three steps, each before anything is changed:
 * (1) the detached signature `<file>.sig` over the package's SHA-256 against
 * the trust list in /etc/pkg/keys; (2) its +MANIFEST (arch, abi, base, depends,
 * conflicts, and every path against the installed set, the base manifest and
 * the disk); (3) extraction, each file written beside its final name, hashed
 * as written, compared with the manifest and renamed into place.  The journal
 * lets any failure (or a crash) undo exactly what was created; the database
 * record is renamed into place last and is the commit point.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pkg.h"

/** Prefix third-party payload paths must live under. */
#define PKG_PREFIX "usr/local/"

/** pkg_read_fn over a file descriptor. */
static long fd_read(void *ctx, void *buf, size_t len)
{
	for (;;)
	{
		ssize_t r = read(*(int *)ctx, buf, len);

		if (r < 0 && errno == EINTR)
			continue;
		return r < 0 ? -errno : (long)r;
	}
}

/**
 * Load every *.pub in <root>/etc/pkg/keys.
 * @return 0, -errno; *nkeys may be 0.  Free *keys.
 */
static int keys_load(const struct ctx *c, struct pkg_key **keys, size_t *nkeys)
{
	char dir[PKG_ABS_MAX], path[PKG_ABS_MAX + 300], *txt;
	struct dirent *de;
	size_t len, n = 0;
	DIR *d;

	*keys = NULL;
	*nkeys = 0;
	if (ctx_path(c, dir, sizeof dir, "etc/pkg/keys") != 0)
		return -ENAMETOOLONG;
	if ((d = opendir(dir)) == NULL)
		return errno == ENOENT ? 0 : -errno;
	while ((de = readdir(d)) != NULL)
	{
		struct pkg_key k, *nv;
		size_t nl = strlen(de->d_name);

		if (nl < 5 || strcmp(de->d_name + nl - 4, ".pub") != 0)
			continue;
		snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
		if (read_whole(path, &txt, &len, 4096) != 0)
			continue;
		if (pkg_key_parse(txt, len, &k) == 0 && (nv = realloc(*keys, (n + 1) * sizeof *nv)) != NULL)
		{
			*keys = nv;
			nv[n++] = k;
		}
		else
			fprintf(stderr, "pkg: ignoring unusable key %s\n", de->d_name);
		free(txt);
	}
	closedir(d);
	*nkeys = n;
	return 0;
}

/**
 * Check the detached signature `<file>.sig`.  The signed message is the
 * lowercase hex SHA-256 of the whole package file plus a newline, so the
 * (possibly very large) package is streamed rather than held in memory.
 * @return 0, -EACCES if there is no valid signature from a trusted key.
 */
static int verify_signature(const struct ctx *c, const char *file)
{
	char sigpath[PKG_ABS_MAX], msg[PKG_SHA256_HEX + 1], *sig;
	uint8_t digest[PKG_SHA256_LEN];
	struct pkg_key *keys;
	size_t nkeys, siglen;
	int rc;

	if (snprintf(sigpath, sizeof sigpath, "%s.sig", file) >= (int)sizeof sigpath)
		return -ENAMETOOLONG;
	if ((rc = file_sha256(file, digest, NULL)) != 0)
		return rc;
	pkg_hex_encode(digest, sizeof digest, msg);
	msg[PKG_SHA256_HEX - 1] = '\n';
	if ((rc = keys_load(c, &keys, &nkeys)) != 0)
		return rc;
	if (nkeys == 0)
	{
		fprintf(stderr, "pkg: no trusted keys in %s/etc/pkg/keys\n", c->root);
		free(keys);
		return -EACCES;
	}
	if ((rc = read_whole(sigpath, &sig, &siglen, 65536)) != 0)
	{
		fprintf(
		    stderr, "pkg: cannot read signature %s: %s (use --no-verify to skip)\n", sigpath, strerror(-rc));
		free(keys);
		return -EACCES;
	}
	rc = pkg_sigfile_verify(sig, siglen, msg, PKG_SHA256_HEX, keys, nkeys);
	free(sig);
	free(keys);
	if (rc != 0)
	{
		fprintf(stderr, "pkg: signature does not verify against any trusted key\n");
		return -EACCES;
	}
	return 0;
}

/** Report a refusal and return -EEXIST. */
static int conflict(const char *name, const char *path, const char *why)
{
	fprintf(stderr, "pkg: %s: %s: %s\n", name, path, why);
	return -EEXIST;
}

/**
 * Check the manifest against this system: arch, abi, base version, installed
 * dependencies and conflicts, and every path against the installed packages,
 * the base manifest and the disk.  Nothing is changed.
 * @return 0, or a negative errno after printing the reason.
 */
static int preflight(const struct ctx *c, const struct pkg_manifest *m)
{
	struct owners own;
	size_t i, j;
	int rc = 0;

	if (!name_is_valid(m->name))
	{
		fprintf(stderr, "pkg: invalid package name '%s'\n", m->name);
		return -EINVAL;
	}
	if (strcmp(m->arch, PKG_OS_ARCH) != 0 || m->abi != PKG_OS_ABI)
	{
		fprintf(stderr,
		        "pkg: %s is for %s/abi %u; this system is %s/abi %d\n",
		        m->name,
		        m->arch,
		        m->abi,
		        PKG_OS_ARCH,
		        PKG_OS_ABI);
		return -ENOEXEC;
	}
	if (m->base.op != PKG_OP_ANY && !pkg_dep_satisfied(&m->base, PKG_OS_VERSION))
	{
		fprintf(stderr, "pkg: %s needs a newer base than %s\n", m->name, PKG_OS_VERSION);
		return -ENOEXEC;
	}
	if (db_installed(c, m->name))
	{
		fprintf(stderr, "pkg: %s is already installed (pkg del it first)\n", m->name);
		return -EEXIST;
	}
	for (i = 0; i < m->ndepends; i++)
	{
		struct pkg_manifest dm;

		if (db_read_manifest(c, m->depends[i].name, &dm, NULL) != 0)
		{
			fprintf(stderr, "pkg: %s needs %s, which is not installed\n", m->name, m->depends[i].name);
			return -ENOENT;
		}
		j = pkg_dep_satisfied(&m->depends[i], dm.version);
		pkg_manifest_free(&dm);
		if (!j)
		{
			fprintf(stderr, "pkg: %s needs a different version of %s\n", m->name, m->depends[i].name);
			return -ENOENT;
		}
	}
	for (i = 0; i < m->nconflicts; i++)
	{
		struct pkg_manifest cm;

		if (db_read_manifest(c, m->conflicts[i].name, &cm, NULL) == 0)
		{
			j = pkg_dep_satisfied(&m->conflicts[i], cm.version);
			pkg_manifest_free(&cm);
			if (j)
				return conflict(m->name, m->conflicts[i].name, "conflicts with an installed package");
		}
	}
	if ((rc = owners_load(c, &own)) != 0)
		return rc;
	for (i = 0; i < m->nentries && rc == 0; i++)
	{
		const struct pkg_mentry *e = &m->entries[i];
		const struct owner *o;
		char abs[PKG_ABS_MAX], anc[PKG_PATH_MAX];
		struct stat st;
		const char *slash;

		if (strncmp(e->path, PKG_PREFIX, sizeof PKG_PREFIX - 1) != 0)
		{
			rc = conflict(m->name, e->path, "outside " PKG_PREFIX);
			break;
		}
		/* An earlier component must not be a link the package itself makes. */
		for (slash = strchr(e->path, '/'); slash != NULL; slash = strchr(slash + 1, '/'))
		{
			const struct pkg_mentry *a;

			snprintf(anc, sizeof anc, "%.*s", (int)(slash - e->path), e->path);
			if ((a = pkg_manifest_find(m, anc)) != NULL && a->type == PKG_M_LINK)
			{
				rc = conflict(m->name, e->path, "path passes through a symlink");
				break;
			}
		}
		if (rc != 0)
			break;
		if ((rc = path_through_symlink(c, e->path)) != 0)
		{
			fprintf(
			    stderr, "pkg: %s: %s: path passes through a symlink or non-directory\n", m->name, e->path);
			break;
		}
		if ((o = owners_find(&own, e->path)) != NULL)
		{
			char why[PKG_NAME_MAX + 32];

			snprintf(why, sizeof why, "already owned by %s", o->name);
			rc = conflict(m->name, e->path, why);
			break;
		}
		if (ctx_path(c, abs, sizeof abs, e->path) != 0)
			rc = -ENAMETOOLONG;
		else if (lstat(abs, &st) == 0 && !(e->type == PKG_M_DIR && S_ISDIR(st.st_mode)))
			rc = conflict(m->name, e->path, "exists and is not owned by a package");
	}
	owners_free(&own);
	return rc;
}

/**
 * Create the missing parent directories of rel, journaling each one made.
 * @return 0, -errno.
 */
static int make_parents(const struct ctx *c, const char *rel)
{
	char abs[PKG_ABS_MAX], relpart[PKG_PATH_MAX];
	size_t i, base = strlen(c->root) + 1;
	int rc;

	if ((rc = ctx_path(c, abs, sizeof abs, rel)) != 0)
		return rc;
	for (i = base; abs[i] != '\0'; i++)
	{
		if (abs[i] != '/')
			continue;
		abs[i] = '\0';
		if (mkdir(abs, 0755) == 0)
		{
			snprintf(relpart, sizeof relpart, "%s", abs + base);
			if ((rc = journal_note(c, 'D', relpart)) != 0)
				return rc;
		}
		else if (errno != EEXIST)
			return -errno;
		abs[i] = '/';
	}
	return 0;
}

/**
 * Extract one regular file: write beside the final name, hash while writing,
 * compare with the manifest, then rename into place.
 * @return 0, -errno, -EBADMSG on a hash or size mismatch.
 */
static int extract_file(const struct ctx *c,
                        struct pkg_reader *r,
                        const struct pkg_entry *e,
                        const struct pkg_mentry *me)
{
	char abs[PKG_ABS_MAX], tmp[PKG_ABS_MAX];
	static unsigned char buf[8192];
	uint8_t digest[PKG_SHA256_LEN];
	struct pkg_sha256 s;
	uint64_t total = 0;
	int fd, rc = 0;

	if ((rc = ctx_path(c, abs, sizeof abs, e->path)) != 0)
		return rc;
	if (snprintf(tmp, sizeof tmp, "%s.upkg-tmp", abs) >= (int)sizeof tmp)
		return -ENAMETOOLONG;
	if ((rc = make_parents(c, e->path)) != 0 || (rc = journal_note(c, 'F', e->path)) != 0)
		return rc;
	if ((fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL, 0600)) < 0)
		return -errno;
	pkg_sha256_init(&s);
	for (;;)
	{
		long n = pkg_reader_read(r, buf, sizeof buf);
		ssize_t off = 0;

		if (n < 0)
		{
			rc = (int)n;
			break;
		}
		if (n == 0)
			break;
		pkg_sha256_update(&s, buf, (size_t)n);
		total += (uint64_t)n;
		while (off < n)
		{
			ssize_t w = write(fd, buf + off, (size_t)(n - off));

			if (w < 0 && errno == EINTR)
				continue;
			if (w <= 0)
			{
				rc = w < 0 ? -errno : -EIO;
				break;
			}
			off += w;
		}
		if (rc != 0)
			break;
	}
	if (close(fd) < 0 && rc == 0)
		rc = -errno;
	pkg_sha256_final(&s, digest);
	if (rc == 0 && (total != me->size || memcmp(digest, me->sha256, PKG_SHA256_LEN) != 0))
	{
		fprintf(stderr, "pkg: %s: content does not match the manifest\n", e->path);
		rc = -EBADMSG;
	}
	if (rc == 0 && chmod(tmp, (mode_t)me->mode) < 0)
		rc = -errno;
	if (rc == 0 && rename(tmp, abs) < 0)
		rc = -errno;
	if (rc != 0)
		(void)unlink(tmp);
	return rc;
}

/**
 * Unpack every payload member, cross-checking each against the manifest.
 * @return 0, -errno.
 */
static int extract_all(const struct ctx *c, struct pkg_reader *r, const struct pkg_manifest *m)
{
	uint8_t *seen = calloc(m->nentries ? m->nentries : 1, 1);
	struct pkg_entry *e = malloc(sizeof *e);
	size_t nseen = 0;
	int rc = 0, n;

	if (seen == NULL || e == NULL)
	{
		free(seen);
		free(e);
		return -ENOMEM;
	}
	while (rc == 0 && (n = pkg_reader_next(r, e)) > 0)
	{
		const struct pkg_mentry *me = pkg_manifest_find(m, e->path);
		char abs[PKG_ABS_MAX];

		if (me == NULL || seen[me - m->entries] ||
		    (int)e->type != (int)me->type /* PKG_FILE/DIR/SYMLINK mirror PKG_M_* */ ||
		    (e->type == PKG_FILE && e->size != me->size))
		{
			fprintf(stderr, "pkg: %s: member is not in (or does not match) the manifest\n", e->path);
			rc = -EBADMSG;
			break;
		}
		seen[me - m->entries] = 1;
		nseen++;
		if ((rc = ctx_path(c, abs, sizeof abs, e->path)) != 0)
			break;
		if (e->type == PKG_FILE)
			rc = extract_file(c, r, e, me);
		else if (e->type == PKG_DIR)
		{
			struct stat st;

			if (lstat(abs, &st) == 0)
				continue; /* already there; not ours to chmod */
			if ((rc = make_parents(c, e->path)) == 0 && (rc = journal_note(c, 'D', e->path)) == 0 &&
			    mkdir(abs, (mode_t)me->mode) < 0)
				rc = -errno;
		}
		else
		{
			if (strcmp(e->link, me->target) != 0)
				rc = -EBADMSG;
			else if ((rc = make_parents(c, e->path)) == 0 && (rc = journal_note(c, 'F', e->path)) == 0 &&
			         symlink(e->link, abs) < 0)
				rc = -errno;
		}
	}
	if (rc == 0 && n < 0)
		rc = n;
	if (rc == 0 && nseen != m->nentries)
	{
		fprintf(stderr, "pkg: archive is missing %zu manifest entries\n", m->nentries - nseen);
		rc = -EBADMSG;
	}
	free(seen);
	free(e);
	return rc;
}

/**
 * Record the package: build .new-<name>/{MANIFEST,FILES}, then rename it to
 * <name>.  The rename is the commit.
 * @return 0, -errno.
 */
static int record(const struct ctx *c, const struct pkg_manifest *m, const char *text, size_t textlen)
{
	char leaf[PKG_NAME_MAX + 8], dir[PKG_ABS_MAX], path[PKG_ABS_MAX], final[PKG_ABS_MAX];
	int rc;

	snprintf(leaf, sizeof leaf, ".new-%s", m->name);
	if ((rc = db_path(c, dir, sizeof dir, leaf, NULL)) != 0)
		return rc;
	if (mkdir(dir, 0755) < 0 && errno != EEXIST)
		return -errno;
	if ((rc = db_path(c, path, sizeof path, leaf, "MANIFEST")) != 0 ||
	    (rc = write_whole(path, text, textlen, 0644)) != 0)
		return rc;
	if ((rc = db_path(c, path, sizeof path, leaf, "FILES")) != 0 || (rc = db_write_files(path, m)) != 0)
		return rc;
	if ((rc = db_path(c, final, sizeof final, m->name, NULL)) != 0)
		return rc;
	return rename(dir, final) < 0 ? -errno : 0;
}

/**
 * Install one package file.
 * @param file  Path to a .upkg (with a `<file>.sig` unless --no-verify).
 * @return 0, -errno.  On any failure nothing is left installed.
 */
int cmd_add(struct ctx *c, const char *file)
{
	struct pkg_manifest m;
	struct pkg_reader r;
	struct pkg_entry *e;
	char *text = NULL;
	size_t got = 0;
	int fd, rc = 0, have_m = 0, open_r = 0;

	if ((rc = lock_take(c)) != 0)
		return rc;
	if ((rc = make_dirs(c, "var/db/pkg")) != 0 || (rc = journal_recover(c)) != 0)
		goto out;
	if (c->noverify)
		fprintf(stderr, "pkg: warning: installing %s without verifying a signature\n", file);
	else if ((rc = verify_signature(c, file)) != 0)
		goto out;
	if ((fd = open(file, O_RDONLY)) < 0)
	{
		rc = -errno;
		fprintf(stderr, "pkg: cannot open %s: %s\n", file, strerror(-rc));
		goto out;
	}
	if ((e = malloc(sizeof *e)) == NULL || (rc = pkg_reader_open(&r, fd_read, &fd, 0, PKG_PREFIX)) != 0)
	{
		rc = rc ? rc : -ENOMEM;
		free(e);
		close(fd);
		goto out;
	}
	open_r = 1;
	if ((rc = pkg_reader_next(&r, e)) != 1 || strcmp(e->path, PKG_MANIFEST_NAME) != 0 || e->size > PKG_MANIFEST_MAX)
	{
		fprintf(stderr, "pkg: %s: not a package (no +MANIFEST)\n", file);
		rc = rc < 0 ? rc : -EINVAL;
		goto out;
	}
	if ((text = malloc((size_t)e->size + 1)) == NULL)
	{
		rc = -ENOMEM;
		goto out;
	}
	while (got < e->size)
	{
		long n = pkg_reader_read(&r, text + got, (size_t)e->size - got);

		if (n <= 0)
		{
			rc = n < 0 ? (int)n : -EINVAL;
			goto out;
		}
		got += (size_t)n;
	}
	text[got] = '\0';
	if ((rc = pkg_manifest_parse(text, got, &m, NULL)) != 0)
	{
		fprintf(stderr, "pkg: %s: malformed manifest\n", file);
		goto out;
	}
	have_m = 1;
	if ((rc = preflight(c, &m)) != 0)
		goto out;
	if ((rc = journal_begin(c, "add", m.name)) != 0)
		goto out;
	if ((rc = extract_all(c, &r, &m)) == 0)
		rc = record(c, &m, text, got);
	if (rc != 0)
	{
		fprintf(stderr, "pkg: install of %s failed (%s); rolling back\n", m.name, strerror(-rc));
		(void)journal_recover(c);
		goto out;
	}
	journal_end(c);
	printf("installed %s-%s-r%u (%zu entries)\n", m.name, m.version, (unsigned)m.revision, m.nentries);
out:
	if (open_r)
	{
		pkg_reader_close(&r);
		close(fd);
		free(e);
	}
	if (have_m)
		pkg_manifest_free(&m);
	free(text);
	lock_drop(c);
	return rc;
}
