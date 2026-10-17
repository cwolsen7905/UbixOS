/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubpkg install: install a package under a root directory (host-side, for
 * image assembly and tests).  Two modes:
 *
 *   --repo DIR -k PUB name        newest signed index in DIR, package looked up
 *                                 by name; the index signature, the package's
 *                                 size and SHA-256, and every file hash are checked
 *   --no-verify file.upkg         local file, no index (prints a warning);
 *                                 per-file hashes against the package's own
 *                                 manifest are still enforced
 */
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ubpkg.h"

#define MAX_KEYS 8

/**
 * `ubpkg install --root DIR (--repo DIR -k PUB [-k PUB2] [--allow-expired] name |
 *  --no-verify file.upkg) [--prefix P]`
 * @return exit status.
 */
int cmd_install(int argc, char **argv)
{
	static const struct option lo[] = {{"root", required_argument, NULL, 'r'},
	                                   {"repo", required_argument, NULL, 'R'},
	                                   {"prefix", required_argument, NULL, 'P'},
	                                   {"no-verify", no_argument, NULL, 'n'},
	                                   {"allow-expired", no_argument, NULL, 'x'},
	                                   {NULL, 0, NULL, 0}};
	struct pkg_key keys[MAX_KEYS];
	struct pkg_manifest m = {0};
	const char *root = NULL;
	const char *repo = NULL;
	const char *prefix = UBPKG_DEFAULT_PREFIX;
	size_t nkeys = 0;
	int noverify = 0;
	int allow_expired = 0;
	int c;
	int rc;

	optind = 1;
	while ((c = getopt_long(argc, argv, "k:", lo, NULL)) != -1)
	{
		switch (c)
		{
			case 'r':
				root = optarg;
				break;
			case 'R':
				repo = optarg;
				break;
			case 'P':
				prefix = optarg;
				break;
			case 'n':
				noverify = 1;
				break;
			case 'x':
				allow_expired = 1;
				break;
			case 'k':
				if (nkeys == MAX_KEYS || ub_load_pubkey(optarg, &keys[nkeys]) != 0)
				{
					ub_err("%s: not a usable public key file", optarg);
					return 1;
				}
				nkeys++;
				break;
			default:
				return 2;
		}
	}
	if (root == NULL || argc - optind != 1 || (noverify ? repo != NULL : repo == NULL || nkeys == 0))
	{
		ub_err("usage: ubpkg install --root DIR (--repo DIR -k PUB [-k PUB2] [--allow-expired] name | "
		       "--no-verify file.upkg) [--prefix p]");
		return 2;
	}

	if (noverify)
	{
		fprintf(stderr, "ubpkg: warning: installing %s without index verification\n", argv[optind]);
		rc = ub_pkg_extract(argv[optind], root, prefix, &m);
	}
	else
	{
		struct pkg_index ix = {0};
		const struct pkg_irec *rec;
		char ipath[PKG_PATH_MAX];
		char ppath[PKG_PATH_MAX * 2];
		uint8_t sha[PKG_SHA256_LEN];
		uint64_t size;

		if (ub_index_latest(repo, ipath, sizeof ipath) != 0)
		{
			ub_err("%s: no index found", repo);
			return 1;
		}
		if (ub_index_load(ipath, keys, nkeys, allow_expired, &ix) != 0)
			return 1;
		rec = pkg_index_find(&ix, argv[optind]);
		if (rec == NULL)
		{
			ub_err("%s: not in %s", argv[optind], ipath);
			pkg_index_free(&ix);
			return 1;
		}
		if (!pkg_path_is_safe(rec->file))
		{
			ub_err("%s: unsafe file path in index", rec->file);
			pkg_index_free(&ix);
			return 1;
		}
		snprintf(ppath, sizeof ppath, "%s/%s", repo, rec->file);
		rc = ub_sha256_file(ppath, sha, &size);
		if (rc != 0)
			ub_err("%s: %s", ppath, strerror(-rc));
		else if (size != rec->size || memcmp(sha, rec->sha256, sizeof sha) != 0)
		{
			ub_err("%s: size or SHA-256 differs from the index", ppath);
			rc = -EBADMSG;
		}
		else if ((rc = ub_pkg_extract(ppath, root, prefix, &m)) == 0)
		{
			if (strcmp(m.name, rec->name) != 0 || strcmp(m.version, rec->version) != 0 ||
			    m.revision != rec->revision || strcmp(m.arch, ix.arch) != 0 || m.abi != ix.abi)
			{
				/* only reachable if the index lies about a package whose hash it also vouches for */
				ub_err("%s: manifest disagrees with the index", ppath);
				rc = -EBADMSG;
			}
		}
		pkg_index_free(&ix);
	}
	if (rc != 0)
	{
		ub_err("install failed: %s", rc == -EBADMSG ? "verification failed" : strerror(-rc));
		return 1;
	}
	printf("installed %s %s-r%u into %s\n", m.name, m.version, m.revision, root);
	pkg_manifest_free(&m);
	return 0;
}
