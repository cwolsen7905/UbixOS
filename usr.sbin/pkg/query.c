/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * pkg info and pkg verify: read-only queries over the database.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pkg.h"

/**
 * List installed packages, or show one package's manifest and files.
 * @param name  NULL to list everything.
 * @return 0, -ENOENT if name is not installed.
 */
int cmd_info(struct ctx *c, const char *name)
{
	if (name == NULL)
	{
		char **names;
		size_t n, i;
		int rc = db_list(c, &names, &n);

		if (rc != 0)
			return rc;
		for (i = 0; i < n; i++)
		{
			struct pkg_manifest m;

			if (db_read_manifest(c, names[i], &m, NULL) != 0)
				printf("%s  (unreadable record)\n", names[i]);
			else
			{
				printf("%s-%s-r%u  %s\n", m.name, m.version, (unsigned)m.revision, m.comment);
				pkg_manifest_free(&m);
			}
		}
		db_names_free(names, n);
		return 0;
	}
	{
		char path[PKG_ABS_MAX], *text;
		struct flist fl;
		size_t len, i;
		int rc;

		if (!name_is_valid(name) || !db_installed(c, name))
		{
			fprintf(stderr, "pkg: %s is not installed\n", name);
			return -ENOENT;
		}
		if ((rc = db_path(c, path, sizeof path, name, "MANIFEST")) != 0 ||
		    (rc = read_whole(path, &text, &len, PKG_MANIFEST_MAX)) != 0)
			return rc;
		/* Show the descriptive records, not the (long) file lines, then the file list. */
		{
			char *line, *next;

			for (line = text; *line != '\0'; line = next)
			{
				if ((next = strchr(line, '\n')) != NULL)
					*next++ = '\0';
				else
					next = line + strlen(line);
				if (strncmp(line, "file:", 5) != 0 && strncmp(line, "dir:", 4) != 0 &&
				    strncmp(line, "link:", 5) != 0 && strncmp(line, "conf:", 5) != 0)
					printf("%s\n", line);
			}
		}
		free(text);
		if ((rc = db_read_files(c, name, &fl)) != 0)
			return rc;
		printf("files:\n");
		for (i = 0; i < fl.n; i++)
			printf("  %s%s\n", fl.v[i].path, fl.v[i].type == 'D' ? "/" : "");
		flist_free(&fl);
	}
	return 0;
}

/**
 * Re-hash one package's files against its record.
 * @return the number of problems found, or -errno.
 */
static int verify_one(const struct ctx *c, const char *name)
{
	struct flist fl;
	int bad = 0, rc;
	size_t i;

	if ((rc = db_read_files(c, name, &fl)) != 0)
		return rc;
	for (i = 0; i < fl.n; i++)
	{
		const struct frec *f = &fl.v[i];
		char abs[PKG_ABS_MAX], buf[PKG_PATH_MAX];
		struct stat st;

		if (ctx_path(c, abs, sizeof abs, f->path) != 0)
			continue;
		if (lstat(abs, &st) < 0)
		{
			printf("%s: MISSING %s\n", name, f->path);
			bad++;
		}
		else if (f->type == 'F')
		{
			uint8_t d[PKG_SHA256_LEN];
			char hex[PKG_SHA256_HEX];

			if (file_sha256(abs, d, NULL) != 0)
			{
				printf("%s: NOT A FILE %s\n", name, f->path);
				bad++;
			}
			else if (pkg_hex_encode(d, sizeof d, hex), strcmp(hex, f->hex) != 0)
			{
				printf("%s: MODIFIED %s\n", name, f->path);
				bad++;
			}
		}
		else if (f->type == 'L')
		{
			ssize_t n = S_ISLNK(st.st_mode) ? readlink(abs, buf, sizeof buf - 1) : -1;

			if (n < 0 || (size_t)n != strlen(f->target) || memcmp(buf, f->target, (size_t)n) != 0)
			{
				printf("%s: MODIFIED %s\n", name, f->path);
				bad++;
			}
		}
		else if (!S_ISDIR(st.st_mode))
		{
			printf("%s: NOT A DIRECTORY %s\n", name, f->path);
			bad++;
		}
	}
	flist_free(&fl);
	return bad;
}

/**
 * Verify the named packages, or all of them if none are named.
 * @return 0 if everything matches, -EBADMSG if anything drifted, -errno.
 */
int cmd_verify(struct ctx *c, int argc, char **argv)
{
	char **names = NULL;
	size_t n = 0, i;
	int bad = 0, rc = 0;

	if (argc == 0)
	{
		if ((rc = db_list(c, &names, &n)) != 0)
			return rc;
		for (i = 0; i < n; i++)
		{
			int r = verify_one(c, names[i]);

			bad += r > 0 ? r : 0;
			rc = r < 0 ? r : rc;
		}
		db_names_free(names, n);
	}
	else
	{
		int a;

		for (a = 0; a < argc; a++)
		{
			int r;

			if (!name_is_valid(argv[a]) || !db_installed(c, argv[a]))
			{
				fprintf(stderr, "pkg: %s is not installed\n", argv[a]);
				rc = -ENOENT;
				continue;
			}
			r = verify_one(c, argv[a]);
			bad += r > 0 ? r : 0;
			rc = r < 0 ? r : rc;
		}
	}
	if (bad == 0 && rc == 0)
		printf("verify: ok\n");
	return bad ? -EBADMSG : rc;
}
