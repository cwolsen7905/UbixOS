/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * pkg del: remove an installed package's files, then its record.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pkg.h"

/**
 * Remove the files a package owns, newest first.  A file whose hash no longer
 * matches the record is kept (locally modified) unless force.  Directories are
 * removed only when empty.  Safe to repeat after an interruption.
 * @return 0, or the count of files kept (positive).
 */
int del_files(const struct ctx *c, const char *name, int force)
{
	struct flist fl;
	int kept = 0;
	size_t i;

	if (db_read_files(c, name, &fl) != 0)
		return 0;
	for (i = fl.n; i-- > 0;)
	{
		const struct frec *f = &fl.v[i];
		char abs[PKG_ABS_MAX];
		struct stat st;

		if (path_through_symlink(c, f->path) != 0 || ctx_path(c, abs, sizeof abs, f->path) != 0 ||
		    lstat(abs, &st) < 0)
			continue;
		if (f->type == 'D')
			(void)rmdir(abs);
		else if (f->type == 'L')
		{
			if (S_ISLNK(st.st_mode))
				(void)unlink(abs);
		}
		else
		{
			uint8_t d[PKG_SHA256_LEN];
			char hex[PKG_SHA256_HEX];

			if (!force && (file_sha256(abs, d, NULL) != 0 ||
			               (pkg_hex_encode(d, sizeof d, hex), strcmp(hex, f->hex) != 0)))
			{
				fprintf(stderr, "pkg: keeping modified %s\n", f->path);
				kept++;
				continue;
			}
			(void)unlink(abs);
		}
	}
	flist_free(&fl);
	return kept;
}

/**
 * Find an installed package that depends on name.
 * @return 1 and copies its name to who, else 0.
 */
static int find_dependent(const struct ctx *c, const char *name, char who[PKG_NAME_MAX])
{
	char **names;
	size_t n, i, j;
	int found = 0;

	if (db_list(c, &names, &n) != 0)
		return 0;
	for (i = 0; i < n && !found; i++)
	{
		struct pkg_manifest m;

		if (strcmp(names[i], name) == 0 || db_read_manifest(c, names[i], &m, NULL) != 0)
			continue;
		for (j = 0; j < m.ndepends; j++)
			if (strcmp(m.depends[j].name, name) == 0)
			{
				snprintf(who, PKG_NAME_MAX, "%s", names[i]);
				found = 1;
			}
		pkg_manifest_free(&m);
	}
	db_names_free(names, n);
	return found;
}

/**
 * Remove a package.
 * @return 0, -ENOENT if not installed, -EBUSY if another package needs it
 *         (unless --force), -errno.
 */
int cmd_del(struct ctx *c, const char *name)
{
	char who[PKG_NAME_MAX];
	int rc, kept;

	if (!name_is_valid(name))
		return -EINVAL;
	if ((rc = lock_take(c)) != 0)
		return rc;
	if ((rc = journal_recover(c)) != 0)
		goto out;
	if (!db_installed(c, name))
	{
		fprintf(stderr, "pkg: %s is not installed\n", name);
		rc = -ENOENT;
		goto out;
	}
	if (!c->force && find_dependent(c, name, who))
	{
		fprintf(stderr, "pkg: %s is needed by %s (use --force)\n", name, who);
		rc = -EBUSY;
		goto out;
	}
	if ((rc = journal_begin(c, "del", name)) != 0)
		goto out;
	kept = del_files(c, name, c->force);
	if ((rc = db_remove(c, name)) == 0)
	{
		journal_end(c);
		printf("removed %s%s\n", name, kept ? " (modified files kept)" : "");
	}
out:
	lock_drop(c);
	return rc;
}
