/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * pkg — the flat-file database under /var/db/pkg and the transaction journal.
 *
 *   /var/db/pkg/<name>/MANIFEST   installed manifest, verbatim
 *   /var/db/pkg/<name>/FILES      one line per entry: `F|D|L <octal mode> <sha256|-> <path>[ -> <target>]`
 *   /var/db/pkg/base/MANIFEST     the base set, same line format as FILES
 *   /var/db/pkg/.journal          the in-progress transaction
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

/**
 * Build /var/db/pkg[/<name>[/<leaf>]] under the root.
 * @return 0, -ENAMETOOLONG.
 */
int db_path(const struct ctx *c, char *out, size_t n, const char *name, const char *leaf)
{
	char rel[PKG_PATH_MAX];
	int len;

	if (name == NULL)
		len = snprintf(rel, sizeof rel, "var/db/pkg");
	else if (leaf == NULL)
		len = snprintf(rel, sizeof rel, "var/db/pkg/%s", name);
	else
		len = snprintf(rel, sizeof rel, "var/db/pkg/%s/%s", name, leaf);
	if (len < 0 || (size_t)len >= sizeof rel)
		return -ENAMETOOLONG;
	return ctx_path(c, out, n, rel);
}

/** @return 1 if a package record named `name` exists. */
int db_installed(const struct ctx *c, const char *name)
{
	char path[PKG_ABS_MAX];
	struct stat st;

	return db_path(c, path, sizeof path, name, "MANIFEST") == 0 && stat(path, &st) == 0;
}

/** qsort order for names. */
static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

/**
 * List installed package names (sorted; "base" and hidden entries excluded).
 * @return 0, -errno.  Free with db_names_free.
 */
int db_list(const struct ctx *c, char ***names, size_t *n)
{
	char path[PKG_ABS_MAX];
	struct dirent *de;
	char **v = NULL;
	size_t cnt = 0;
	DIR *d;

	*names = NULL;
	*n = 0;
	if (db_path(c, path, sizeof path, NULL, NULL) != 0)
		return -ENAMETOOLONG;
	if ((d = opendir(path)) == NULL)
		return errno == ENOENT ? 0 : -errno;
	while ((de = readdir(d)) != NULL)
	{
		char **nv;

		if (!name_is_valid(de->d_name) || !db_installed(c, de->d_name))
			continue;
		if ((nv = realloc(v, (cnt + 1) * sizeof *v)) == NULL || (nv[cnt] = strdup(de->d_name)) == NULL)
		{
			if (nv != NULL)
				v = nv;
			db_names_free(v, cnt);
			closedir(d);
			return -ENOMEM;
		}
		v = nv;
		cnt++;
	}
	closedir(d);
	if (cnt > 1)
		qsort(v, cnt, sizeof *v, cmp_str);
	*names = v;
	*n = cnt;
	return 0;
}

/** Free a list from db_list. */
void db_names_free(char **names, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		free(names[i]);
	free(names);
}

/**
 * Load an installed package's manifest.
 * @param text  If non-NULL receives the raw text (caller frees; m points into
 *              nothing of it, so it may be freed independently).
 * @return 0, -errno, -EINVAL.
 */
int db_read_manifest(const struct ctx *c, const char *name, struct pkg_manifest *m, char **text)
{
	char path[PKG_ABS_MAX], *buf;
	size_t len;
	int rc;

	if ((rc = db_path(c, path, sizeof path, name, "MANIFEST")) != 0 ||
	    (rc = read_whole(path, &buf, &len, PKG_MANIFEST_MAX)) != 0)
		return rc;
	rc = pkg_manifest_parse(buf, len, m, NULL);
	if (rc == 0 && text != NULL)
		*text = buf;
	else
		free(buf);
	return rc;
}

/** Free a flist. */
void flist_free(struct flist *fl)
{
	size_t i;

	for (i = 0; i < fl->n; i++)
	{
		free(fl->v[i].path);
		free(fl->v[i].target);
	}
	free(fl->v);
	fl->v = NULL;
	fl->n = 0;
}

/** Parse one FILES line (modified in place).  @return 0, -EINVAL, -ENOMEM. */
static int parse_frec(char *line, struct frec *r)
{
	char *p = line, *end, *arrow;

	memset(r, 0, sizeof *r);
	if ((p[0] != 'F' && p[0] != 'D' && p[0] != 'L') || p[1] != ' ')
		return -EINVAL;
	r->type = p[0];
	p += 2;
	r->mode = (uint32_t)strtoul(p, &end, 8);
	if (end == p || *end != ' ')
		return -EINVAL;
	p = end + 1;
	if ((end = strchr(p, ' ')) == NULL || (size_t)(end - p) >= sizeof r->hex)
		return -EINVAL;
	memcpy(r->hex, p, (size_t)(end - p));
	p = end + 1;
	if (r->type == 'L' && (arrow = strstr(p, " -> ")) != NULL)
	{
		*arrow = '\0';
		if ((r->target = strdup(arrow + 4)) == NULL)
			return -ENOMEM;
	}
	if ((r->path = strdup(p)) == NULL || !pkg_path_is_safe(r->path))
	{
		free(r->path);
		free(r->target);
		r->path = r->target = NULL;
		return -EINVAL;
	}
	return 0;
}

/**
 * Read a FILES-format list from an absolute path.
 * @return 0, -errno, -EINVAL.
 */
static int read_flist_at(const char *path, struct flist *fl)
{
	char *buf, *line, *next;
	size_t len;
	int rc;

	fl->v = NULL;
	fl->n = 0;
	if ((rc = read_whole(path, &buf, &len, PKG_MANIFEST_MAX)) != 0)
		return rc;
	for (line = buf; *line != '\0'; line = next)
	{
		struct frec *nv;

		if ((next = strchr(line, '\n')) != NULL)
			*next++ = '\0';
		else
			next = line + strlen(line);
		if (*line == '\0' || *line == '#')
			continue;
		if ((nv = realloc(fl->v, (fl->n + 1) * sizeof *nv)) == NULL)
			rc = -ENOMEM;
		else
		{
			fl->v = nv;
			rc = parse_frec(line, &fl->v[fl->n]);
		}
		if (rc != 0)
		{
			flist_free(fl);
			free(buf);
			return rc;
		}
		fl->n++;
	}
	free(buf);
	return 0;
}

/** Read /var/db/pkg/<name>/FILES.  @return 0, -errno, -EINVAL. */
int db_read_files(const struct ctx *c, const char *name, struct flist *fl)
{
	char path[PKG_ABS_MAX];
	int rc = db_path(c, path, sizeof path, name, "FILES");

	return rc != 0 ? rc : read_flist_at(path, fl);
}

/**
 * Write the FILES list for a package to an absolute path.
 * @return 0, -errno.
 */
int db_write_files(const char *path, const struct pkg_manifest *m)
{
	size_t i;
	FILE *f = fopen(path, "w");
	char hex[PKG_SHA256_HEX];

	if (f == NULL)
		return -errno;
	for (i = 0; i < m->nentries; i++)
	{
		const struct pkg_mentry *e = &m->entries[i];

		if (e->type == PKG_M_FILE)
		{
			pkg_hex_encode(e->sha256, PKG_SHA256_LEN, hex);
			fprintf(f, "F %04o %s %s\n", (unsigned)e->mode, hex, e->path);
		}
		else if (e->type == PKG_M_DIR)
			fprintf(f, "D %04o - %s\n", (unsigned)e->mode, e->path);
		else
			fprintf(f, "L %04o - %s -> %s\n", (unsigned)e->mode, e->path, e->target);
	}
	if (ferror(f) || fclose(f) != 0)
		return -EIO;
	return 0;
}

/** qsort/bsearch order for owners: by path. */
static int cmp_owner(const void *a, const void *b)
{
	return strcmp(((const struct owner *)a)->path, ((const struct owner *)b)->path);
}

/** Add every file/link of one list to the owner table.  @return 0, -ENOMEM. */
static int owners_add(struct owners *o, const char *name, const struct flist *fl)
{
	size_t i;

	for (i = 0; i < fl->n; i++)
	{
		struct owner *nv;

		if (fl->v[i].type == 'D')
			continue;
		if ((nv = realloc(o->v, (o->n + 1) * sizeof *nv)) == NULL)
			return -ENOMEM;
		o->v = nv;
		if ((nv[o->n].path = strdup(fl->v[i].path)) == NULL)
			return -ENOMEM;
		snprintf(nv[o->n].name, sizeof nv[o->n].name, "%s", name);
		o->n++;
	}
	return 0;
}

/**
 * Load every path claimed by an installed package or by the base manifest, for
 * conflict checks.
 * @return 0, -errno.  Free with owners_free.
 */
int owners_load(const struct ctx *c, struct owners *o)
{
	char **names, path[PKG_ABS_MAX];
	struct flist fl;
	size_t n, i;
	int rc;

	o->v = NULL;
	o->n = 0;
	if ((rc = db_list(c, &names, &n)) != 0)
		return rc;
	for (i = 0; i < n && rc == 0; i++)
	{
		if ((rc = db_read_files(c, names[i], &fl)) == 0)
		{
			rc = owners_add(o, names[i], &fl);
			flist_free(&fl);
		}
	}
	db_names_free(names, n);
	if (rc == 0 && db_path(c, path, sizeof path, "base", "MANIFEST") == 0 && read_flist_at(path, &fl) == 0)
	{
		rc = owners_add(o, "base", &fl);
		flist_free(&fl);
	}
	if (rc != 0)
	{
		owners_free(o);
		return rc;
	}
	if (o->n > 1)
		qsort(o->v, o->n, sizeof *o->v, cmp_owner);
	return 0;
}

/** @return the owner of path, or NULL. */
const struct owner *owners_find(const struct owners *o, const char *path)
{
	struct owner key;

	key.path = (char *)(uintptr_t)path;
	return o->n == 0 ? NULL : bsearch(&key, o->v, o->n, sizeof *o->v, cmp_owner);
}

/** Release an owner table. */
void owners_free(struct owners *o)
{
	size_t i;

	for (i = 0; i < o->n; i++)
		free(o->v[i].path);
	free(o->v);
	o->v = NULL;
	o->n = 0;
}

/** Remove a package record (MANIFEST, FILES, directory).  @return 0, -errno. */
int db_remove(const struct ctx *c, const char *name)
{
	char path[PKG_ABS_MAX];
	int rc;

	if ((rc = db_path(c, path, sizeof path, name, "FILES")) == 0)
		(void)unlink(path);
	if (rc == 0 && (rc = db_path(c, path, sizeof path, name, "MANIFEST")) == 0 && unlink(path) < 0 &&
	    errno != ENOENT)
		rc = -errno;
	if (rc == 0 && (rc = db_path(c, path, sizeof path, name, NULL)) == 0 && rmdir(path) < 0 && errno != ENOENT)
		rc = -errno;
	return rc;
}

/** Path of the journal file. */
static int journal_path(const struct ctx *c, char *out, size_t n)
{
	return db_path(c, out, n, ".journal", NULL);
}

/**
 * Start a transaction: the journal records the verb ("add"/"del") and package.
 * @return 0, -errno.
 */
int journal_begin(const struct ctx *c, const char *verb, const char *name)
{
	char path[PKG_ABS_MAX], line[PKG_NAME_MAX + 16];

	if (journal_path(c, path, sizeof path) != 0)
		return -ENAMETOOLONG;
	snprintf(line, sizeof line, "%s %s\n", verb, name);
	return write_whole(path, line, strlen(line), 0600);
}

/**
 * Append a record to the journal: D directory created, F file placed, T temp
 * file in flight (all package-relative paths).  Written before the action.
 * @return 0, -errno.
 */
int journal_note(const struct ctx *c, char tag, const char *rel)
{
	char path[PKG_ABS_MAX], line[PKG_PATH_MAX + 4];
	int fd, len;
	ssize_t w;

	if (journal_path(c, path, sizeof path) != 0)
		return -ENAMETOOLONG;
	len = snprintf(line, sizeof line, "%c %s\n", tag, rel);
	if (len < 0 || (size_t)len >= sizeof line)
		return -ENAMETOOLONG;
	if ((fd = open(path, O_WRONLY | O_APPEND)) < 0)
		return -errno;
	w = write(fd, line, (size_t)len);
	close(fd);
	return w == len ? 0 : -EIO;
}

/** Commit point passed: forget the transaction. */
void journal_end(const struct ctx *c)
{
	char path[PKG_ABS_MAX];

	if (journal_path(c, path, sizeof path) == 0)
		(void)unlink(path);
}

/** Remove the staging directory .new-<name> of an add, if any. */
static void drop_staging(const struct ctx *c, const char *name)
{
	char leaf[PKG_NAME_MAX + 8], path[PKG_ABS_MAX];

	snprintf(leaf, sizeof leaf, ".new-%s", name);
	if (db_path(c, path, sizeof path, leaf, "MANIFEST") == 0)
		(void)unlink(path);
	if (db_path(c, path, sizeof path, leaf, "FILES") == 0)
		(void)unlink(path);
	if (db_path(c, path, sizeof path, leaf, NULL) == 0)
		(void)rmdir(path);
}

/**
 * Finish an interrupted transaction found at startup.  An add whose record was
 * not committed is rolled back (files and directories it created are removed);
 * a committed add only loses its journal; an interrupted del is rolled forward.
 * @return 0, -errno.
 */
int journal_recover(struct ctx *c)
{
	char path[PKG_ABS_MAX], *txt, *line, *next, **lines = NULL, name[PKG_NAME_MAX];
	size_t len, nl = 0, i;
	int rc, is_add;

	if ((rc = journal_path(c, path, sizeof path)) != 0)
		return rc;
	if ((rc = read_whole(path, &txt, &len, PKG_MANIFEST_MAX)) != 0)
		return rc == -ENOENT ? 0 : rc;
	for (line = txt; *line != '\0'; line = next)
	{
		char **nv;

		if ((next = strchr(line, '\n')) != NULL)
			*next++ = '\0';
		else
			next = line + strlen(line);
		if ((nv = realloc(lines, (nl + 1) * sizeof *nv)) == NULL)
		{
			free(lines);
			free(txt);
			return -ENOMEM;
		}
		lines = nv;
		lines[nl++] = line;
	}
	if (nl == 0 || (strncmp(lines[0], "add ", 4) != 0 && strncmp(lines[0], "del ", 4) != 0) ||
	    !name_is_valid(lines[0] + 4))
	{
		fprintf(stderr, "pkg: discarding unreadable journal\n");
		goto done;
	}
	is_add = lines[0][0] == 'a';
	snprintf(name, sizeof name, "%s", lines[0] + 4);
	if (is_add && db_installed(c, name))
		goto done; /* committed; only the journal is left */
	if (!is_add)
	{
		fprintf(stderr, "pkg: completing interrupted removal of %s\n", name);
		(void)del_files(c, name, 1);
		(void)db_remove(c, name);
		goto done;
	}
	fprintf(stderr, "pkg: rolling back interrupted install of %s\n", name);
	for (i = nl; i-- > 1;)
	{
		char abs[PKG_ABS_MAX], tmp[PKG_ABS_MAX];

		if (lines[i][0] == '\0' || lines[i][1] != ' ' || !pkg_path_is_safe(lines[i] + 2) ||
		    ctx_path(c, abs, sizeof abs, lines[i] + 2) != 0)
			continue;
		if (lines[i][0] == 'D')
			(void)rmdir(abs);
		else if (lines[i][0] == 'F' || lines[i][0] == 'T')
		{
			(void)unlink(abs);
			if (snprintf(tmp, sizeof tmp, "%s.upkg-tmp", abs) < (int)sizeof tmp)
				(void)unlink(tmp);
		}
	}
	drop_staging(c, name);
done:
	free(lines);
	free(txt);
	journal_end(c);
	return 0;
}
