/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * pkg — the on-device package client.  Commands: add --local, del, info,
 * verify.  See docs/design/packages-plan.md section 4.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"

/** Print usage. */
static void usage(void)
{
	fprintf(stderr,
	        "usage: pkg [--root dir] [--force] [--no-verify] command ...\n"
	        "  add --local file.upkg   install a package file (needs file.upkg.sig)\n"
	        "  del name                remove a package\n"
	        "  info [name]             list packages, or show one\n"
	        "  verify [name ...]       re-hash installed files\n");
}

/** Entry point. */
int main(int argc, char **argv)
{
	struct ctx c;
	int rc = 0, i = 1;

	memset(&c, 0, sizeof c);
	c.lockfd = -1;
	for (; i < argc && argv[i][0] == '-' && argv[i][1] == '-'; i++)
	{
		if (strcmp(argv[i], "--root") == 0 && i + 1 < argc)
		{
			size_t n = strlen(argv[++i]);

			while (n > 0 && argv[i][n - 1] == '/') /* "/" becomes "", the live root */
				n--;
			if (n >= sizeof c.root)
			{
				fprintf(stderr, "pkg: --root too long\n");
				return 2;
			}
			memcpy(c.root, argv[i], n);
		}
		else if (strcmp(argv[i], "--force") == 0)
			c.force = 1;
		else if (strcmp(argv[i], "--no-verify") == 0)
			c.noverify = 1;
		else
		{
			usage();
			return 2;
		}
	}
	if (i >= argc)
	{
		usage();
		return 2;
	}
	if (strcmp(argv[i], "add") == 0)
	{
		/* Accept --local/--no-verify/--force after the command too. */
		const char *file = NULL;
		int j;

		for (j = i + 1; j < argc; j++)
		{
			if (strcmp(argv[j], "--local") == 0)
				continue;
			if (strcmp(argv[j], "--no-verify") == 0)
				c.noverify = 1;
			else if (strcmp(argv[j], "--force") == 0)
				c.force = 1;
			else if (file == NULL)
				file = argv[j];
			else
			{
				file = NULL;
				break;
			}
		}
		if (file == NULL)
		{
			usage();
			return 2;
		}
		rc = cmd_add(&c, file);
	}
	else if (strcmp(argv[i], "del") == 0 && i + 1 < argc)
		rc = cmd_del(&c, argv[i + 1]);
	else if (strcmp(argv[i], "info") == 0 && i + 2 >= argc)
		rc = cmd_info(&c, i + 1 < argc ? argv[i + 1] : NULL);
	else if (strcmp(argv[i], "verify") == 0)
		rc = cmd_verify(&c, argc - i - 1, argv + i + 1);
	else
	{
		usage();
		return 2;
	}
	if (rc != 0 && rc != -EBADMSG)
		fprintf(stderr, "pkg: %s failed: %s\n", argv[i], strerror(-rc));
	return rc == 0 ? 0 : 1;
}
