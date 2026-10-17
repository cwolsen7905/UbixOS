/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubpkg index / verify: build and sign a repository index, and check one.
 *
 * Repository layout (docs/design/packages-plan.md, section 2):
 *   <repo>/index/index.<serial>[.sig]
 *   <repo>/pool/<name>/<name>-<version>-r<rev>.upkg
 * Nothing existing is ever overwritten.  Publish order is pool, index, sig.
 */
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <bearssl.h>
#include "ubpkg.h"

#define MAX_KEYS 8
#define MAX_PKGS 4096

struct irec
{
	struct pkg_manifest m;
	char file[PKG_PATH_MAX];
	uint64_t size;
	uint8_t sha[PKG_SHA256_LEN];
};

/** Operator spelling for index/manifest dependency fields. */
static const char *op_str(enum pkg_op op)
{
	switch (op)
	{
		case PKG_OP_EQ:
			return "=";
		case PKG_OP_GE:
			return ">=";
		case PKG_OP_GT:
			return ">";
		case PKG_OP_LE:
			return "<=";
		case PKG_OP_LT:
			return "<";
		default:
			return "";
	}
}

/** Append a `key: dep, dep` line.  @return 0 or -errno. */
static int put_deps(struct ub_buf *b, const char *key, const struct pkg_dep *d, size_t n)
{
	size_t i;
	int rc = ub_buf_printf(b, "%s:", key);

	for (i = 0; rc == 0 && i < n; i++)
	{
		rc = ub_buf_printf(b, "%s %s", i ? "," : "", d[i].name);
		if (rc == 0 && d[i].op != PKG_OP_ANY)
			rc = ub_buf_printf(b, " %s %s", op_str(d[i].op), d[i].version);
	}
	return rc != 0 ? rc : ub_buf_printf(b, "\n");
}

/** qsort order: name, then version, then revision. */
static int irec_cmp(const void *pa, const void *pb)
{
	const struct irec *a = pa;
	const struct irec *b = pb;
	int c = strcmp(a->m.name, b->m.name);

	if (c == 0)
		c = pkg_version_cmp(a->m.version, b->m.version);
	if (c == 0)
		c = a->m.revision < b->m.revision ? -1 : a->m.revision > b->m.revision;
	return c;
}

/** Format a UTC time as YYYY-MM-DDTHH:MM:SSZ. */
static void fmt_time(time_t t, char *out, size_t sz)
{
	struct tm tm;

	gmtime_r(&t, &tm);
	strftime(out, sz, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

/** "now": SOURCE_DATE_EPOCH if set (reproducible indexes), else the clock. */
static time_t now_time(void)
{
	const char *e = getenv("SOURCE_DATE_EPOCH");

	return e != NULL && e[0] != '\0' ? (time_t)strtoll(e, NULL, 10) : time(NULL);
}

/** Load a private key file into sk (x points into xbuf).  @return 0 or -errno. */
static int load_privkey(const char *path, br_ec_private_key *sk, uint8_t xbuf[32])
{
	uint8_t *buf;
	size_t len;
	size_t keylen = strlen(UBPKG_PRIV_KEY);
	const char *s;
	int rc = ub_read_file(path, &buf, &len);

	if (rc != 0)
		return rc;
	s = (const char *)buf;
	while (s < (const char *)buf + len && strncmp(s, UBPKG_PRIV_KEY ":", keylen + 1) != 0)
	{
		s = strchr(s, '\n');
		if (s == NULL)
			break;
		s++;
	}
	rc = -EINVAL;
	if (s != NULL && s < (const char *)buf + len)
	{
		s += keylen + 1;
		while (*s == ' ')
			s++;
		if (pkg_hex_decode(s, 64, xbuf, 32) == 0)
		{
			sk->curve = BR_EC_secp256r1;
			sk->x = xbuf;
			sk->xlen = 32;
			rc = 0;
		}
	}
	free(buf);
	return rc;
}

/**
 * Newest serial present in `<repo>/index`.
 * @return the serial (0 if none) .
 */
static uint64_t max_serial(const char *repo)
{
	char dir[PKG_PATH_MAX];
	struct dirent *de;
	uint64_t best = 0;
	DIR *d;

	snprintf(dir, sizeof dir, "%s/index", repo);
	d = opendir(dir);
	if (d == NULL)
		return 0;
	while ((de = readdir(d)) != NULL)
	{
		char *end;
		uint64_t n;

		if (strncmp(de->d_name, "index.", 6) != 0 || de->d_name[6] < '0' || de->d_name[6] > '9')
			continue;
		n = strtoull(de->d_name + 6, &end, 10);
		if (*end == '\0' && n > best)
			best = n;
	}
	closedir(d);
	return best;
}

/**
 * Path of the newest index in a repository.
 * @return 0, or -ENOENT if the repository has none.
 */
int ub_index_latest(const char *repo, char *path, size_t pathsz)
{
	uint64_t n = max_serial(repo);

	if (n == 0)
		return -ENOENT;
	snprintf(path, pathsz, "%s/index/index.%llu", repo, (unsigned long long)n);
	return 0;
}

/**
 * Load an index and its `.sig`, check the signature against the trust list and
 * the freshness policy.
 * @return 0, -EBADMSG (bad signature), -EINVAL, -ESTALE (expired), or -errno.
 */
int ub_index_load(
    const char *indexpath, const struct pkg_key *keys, size_t nkeys, int allow_expired, struct pkg_index *ix)
{
	char sigpath[PKG_PATH_MAX];
	uint8_t *text = NULL;
	uint8_t *sig = NULL;
	size_t tlen;
	size_t slen;
	unsigned line = 0;
	int rc;

	snprintf(sigpath, sizeof sigpath, "%s.sig", indexpath);
	if ((rc = ub_read_file(indexpath, &text, &tlen)) != 0 || (rc = ub_read_file(sigpath, &sig, &slen)) != 0)
		goto out;
	rc = pkg_sigfile_verify((const char *)sig, slen, text, tlen, keys, nkeys);
	if (rc != 0)
	{
		ub_err("%s: signature does not verify", indexpath);
		goto out;
	}
	rc = pkg_index_parse((const char *)text, tlen, ix, &line);
	if (rc != 0)
	{
		ub_err("%s:%u: malformed index", indexpath, line);
		goto out;
	}
	rc = pkg_index_check(ix, now_time(), 0);
	if (rc == -ESTALE && allow_expired)
		rc = 0;
	if (rc != 0)
	{
		ub_err("%s: index is %s", indexpath, rc == -ESTALE ? "expired" : "stale");
		pkg_index_free(ix);
	}
out:
	free(text);
	free(sig);
	return rc;
}

/**
 * Copy a package into the pool without overwriting.  An identical file already
 * there is fine; a different one is an error.
 * @return 0 or -errno/-EEXIST.
 */
static int pool_put(const char *repo, struct irec *r, const char *src)
{
	char dir[PKG_PATH_MAX];
	char dst[PKG_PATH_MAX];
	uint8_t have[PKG_SHA256_LEN];
	uint64_t hsize;
	uint8_t *data;
	size_t len;
	int rc;

	snprintf(r->file, sizeof r->file, "pool/%s/%s", r->m.name, ub_basename(src));
	snprintf(dir, sizeof dir, "%s/pool/%s", repo, r->m.name);
	snprintf(dst, sizeof dst, "%s/%s", repo, r->file);
	if (ub_sha256_file(dst, have, &hsize) == 0)
		return (hsize == r->size && memcmp(have, r->sha, sizeof have) == 0) ? 0 : -EEXIST;
	if ((rc = ub_mkdirs(dir)) != 0 || (rc = ub_read_file(src, &data, &len)) != 0)
		return rc;
	rc = ub_write_new(dst, data, len, 0644);
	free(data);
	return rc;
}

/** Render the index text for the sorted records.  @return 0 or -errno. */
static int render_index(struct ub_buf *b,
                        uint64_t serial,
                        uint32_t abi,
                        const char *arch,
                        time_t created,
                        long days,
                        const uint8_t *prev,
                        const struct irec *recs,
                        size_t n)
{
	char ts[32];
	char hex[PKG_SHA256_HEX];
	size_t i;
	int rc;

	fmt_time(created, ts, sizeof ts);
	rc = ub_buf_printf(
	    b, "serial: %llu\nabi: %u\narch: %s\ncreated: %s\n", (unsigned long long)serial, abi, arch, ts);
	if (rc == 0 && days > 0)
	{
		fmt_time(created + (time_t)days * 86400, ts, sizeof ts);
		rc = ub_buf_printf(b, "expires: %s\n", ts);
	}
	if (rc == 0 && prev != NULL)
	{
		pkg_hex_encode(prev, PKG_SHA256_LEN, hex);
		rc = ub_buf_printf(b, "previous: %s\n", hex);
	}
	for (i = 0; rc == 0 && i < n; i++)
	{
		const struct pkg_manifest *m = &recs[i].m;

		pkg_hex_encode(recs[i].sha, PKG_SHA256_LEN, hex);
		rc = ub_buf_printf(b,
		                   "\nname: %s\nversion: %s\nrevision: %u\nfile: %s\nsize: %llu\nsha256: %s\n",
		                   m->name,
		                   m->version,
		                   m->revision,
		                   recs[i].file,
		                   (unsigned long long)recs[i].size,
		                   hex);
		if (rc == 0 && m->ndepends > 0)
			rc = put_deps(b, "depends", m->depends, m->ndepends);
		if (rc == 0 && m->base.op != PKG_OP_ANY)
			rc = ub_buf_printf(b, "base: %s %s\n", op_str(m->base.op), m->base.version);
		if (rc == 0 && m->comment[0] != '\0')
			rc = ub_buf_printf(b, "comment: %s\n", m->comment);
	}
	return rc;
}

/**
 * `ubpkg index -R repo -s serial -a arch -A abi -k key [-k key2] [-e days]
 *  [--no-previous] [--prefix P] pkg.upkg...`
 * @return exit status.
 */
int cmd_index(int argc, char **argv)
{
	static const struct option lo[] = {
	    {"no-previous", no_argument, NULL, 'N'}, {"prefix", required_argument, NULL, 'P'}, {NULL, 0, NULL, 0}};
	const char *repo = NULL;
	const char *arch = NULL;
	const char *prefix = UBPKG_DEFAULT_PREFIX;
	const char *keyf[MAX_KEYS];
	size_t nkeyf = 0;
	uint64_t serial = 0;
	long abi = -1;
	long days = 60;
	int no_prev = 0;
	int c;
	size_t n = 0;
	size_t i;
	struct irec *recs = NULL;
	struct ub_buf text = {0};
	struct ub_buf sig = {0};
	uint8_t prev[PKG_SHA256_LEN];
	int have_prev = 0;
	char path[PKG_PATH_MAX];
	int rc = 1;

	optind = 1;
	while ((c = getopt_long(argc, argv, "R:s:a:A:k:e:", lo, NULL)) != -1)
	{
		switch (c)
		{
			case 'R':
				repo = optarg;
				break;
			case 's':
				serial = strtoull(optarg, NULL, 10);
				break;
			case 'a':
				arch = optarg;
				break;
			case 'A':
				abi = strtol(optarg, NULL, 10);
				break;
			case 'k':
				if (nkeyf == MAX_KEYS)
				{
					ub_err("too many -k");
					return 2;
				}
				keyf[nkeyf++] = optarg;
				break;
			case 'e':
				days = strtol(optarg, NULL, 10);
				break;
			case 'N':
				no_prev = 1;
				break;
			case 'P':
				prefix = optarg;
				break;
			default:
				return 2;
		}
	}
	n = (size_t)(argc - optind);
	if (repo == NULL || arch == NULL || abi < 0 || serial == 0 || nkeyf == 0 || n == 0 || n > MAX_PKGS)
	{
		ub_err("usage: ubpkg index -R repo -s serial -a arch -A abi -k privkey [-k privkey2] [-e days] "
		       "[--no-previous] [--prefix p] pkg.upkg...");
		return 2;
	}
	recs = calloc(n, sizeof *recs);
	if (recs == NULL)
		return 1;
	n = 0;
	for (i = 0; i < (size_t)(argc - optind); i++)
	{
		const char *pkg = argv[optind + i];
		struct irec *r = &recs[n];

		if ((c = ub_pkg_extract(pkg, NULL, prefix, &r->m)) != 0)
		{
			ub_err("%s: invalid package (%s)", pkg, strerror(-c));
			goto out;
		}
		n++;
		if (strcmp(r->m.arch, arch) != 0 || r->m.abi != (uint32_t)abi)
		{
			ub_err(
			    "%s: arch/abi (%s/%u) differ from the index (%s/%ld)", pkg, r->m.arch, r->m.abi, arch, abi);
			goto out;
		}
		if ((c = ub_sha256_file(pkg, r->sha, &r->size)) != 0)
		{
			ub_err("%s: %s", pkg, strerror(-c));
			goto out;
		}
		if ((c = pool_put(repo, r, pkg)) != 0)
		{
			ub_err("%s: pool/%s: %s",
			       pkg,
			       r->m.name,
			       c == -EEXIST ? "a different file with this name is already published" : strerror(-c));
			goto out;
		}
	}
	qsort(recs, n, sizeof *recs, irec_cmp);
	for (i = 1; i < n; i++)
	{
		if (irec_cmp(&recs[i - 1], &recs[i]) == 0)
		{
			ub_err("%s %s-r%u listed twice", recs[i].m.name, recs[i].m.version, recs[i].m.revision);
			goto out;
		}
	}
	if (serial > 1 && !no_prev)
	{
		uint64_t sz;

		snprintf(path, sizeof path, "%s/index/index.%llu", repo, (unsigned long long)(serial - 1));
		if ((c = ub_sha256_file(path, prev, &sz)) != 0)
		{
			ub_err("%s: %s (use --no-previous to start a chain here)", path, strerror(-c));
			goto out;
		}
		have_prev = 1;
	}
	if ((c = render_index(
	         &text, serial, (uint32_t)abi, arch, now_time(), days, have_prev ? prev : NULL, recs, n)) != 0)
	{
		ub_err("cannot render index");
		goto out;
	}
	{
		struct pkg_index chk = {0};
		unsigned line = 0;

		if (pkg_index_parse((const char *)text.p, text.n, &chk, &line) != 0)
		{
			ub_err("internal error: generated index does not parse (line %u)", line);
			goto out;
		}
		pkg_index_free(&chk);
	}
	{
		uint8_t digest[PKG_SHA256_LEN];

		pkg_sha256(text.p, text.n, digest);
		for (i = 0; i < nkeyf; i++)
		{
			br_ec_private_key sk;
			br_ec_public_key pk;
			uint8_t xbuf[32];
			uint8_t pbuf[BR_EC_KBUF_PUB_MAX_SIZE];
			uint8_t der[80];
			char hex[sizeof der * 2 + 1];
			size_t dl;

			if ((c = load_privkey(keyf[i], &sk, xbuf)) != 0)
			{
				ub_err("%s: not a private key file", keyf[i]);
				goto out;
			}
			dl = br_ecdsa_i31_sign_asn1(&br_ec_prime_i31, &br_sha256_vtable, digest, &sk, der);
			if (dl == 0 || br_ec_compute_pub(&br_ec_prime_i31, &pk, pbuf, &sk) == 0)
			{
				ub_err("%s: signing failed", keyf[i]);
				goto out;
			}
			pkg_hex_encode(der, dl, hex);
			if (ub_buf_printf(&sig, "ecdsa-p256-sha256: %s\n", hex) != 0)
				goto out;
			{
				struct pkg_key vk;

				memcpy(vk.point, pk.q, PKG_ECDSA_PUB_LEN);
				if (pkg_ecdsa_verify(&vk, text.p, text.n, der, dl) != 0)
				{
					ub_err("internal error: fresh signature does not verify");
					goto out;
				}
			}
		}
	}
	snprintf(path, sizeof path, "%s/index", repo);
	if ((c = ub_mkdirs(path)) != 0)
	{
		ub_err("%s: %s", path, strerror(-c));
		goto out;
	}
	snprintf(path, sizeof path, "%s/index/index.%llu", repo, (unsigned long long)serial);
	if ((c = ub_write_new(path, text.p, text.n, 0644)) != 0)
	{
		ub_err("%s: %s", path, strerror(-c));
		goto out;
	}
	strncat(path, ".sig", sizeof path - strlen(path) - 1); /* signature last: a reader that sees it sees all */
	if ((c = ub_write_new(path, sig.p, sig.n, 0644)) != 0)
	{
		ub_err("%s: %s", path, strerror(-c));
		goto out;
	}
	printf("index.%llu: %zu packages\n", (unsigned long long)serial, n);
	rc = 0;
out:
	for (i = 0; i < n; i++)
		pkg_manifest_free(&recs[i].m);
	free(recs);
	free(text.p);
	free(sig.p);
	return rc;
}

/**
 * `ubpkg verify -k pub [-k pub2] [--allow-expired] <index-file>`: check an index
 * signature, structure and freshness.
 * @return exit status.
 */
int cmd_verify(int argc, char **argv)
{
	static const struct option lo[] = {{"allow-expired", no_argument, NULL, 'x'}, {NULL, 0, NULL, 0}};
	struct pkg_key keys[MAX_KEYS];
	struct pkg_index ix = {0};
	size_t nkeys = 0;
	int allow_expired = 0;
	int c;
	int rc;

	optind = 1;
	while ((c = getopt_long(argc, argv, "k:", lo, NULL)) != -1)
	{
		if (c == 'k' && nkeys < MAX_KEYS)
		{
			if (ub_load_pubkey(optarg, &keys[nkeys]) != 0)
			{
				ub_err("%s: not a public key file", optarg);
				return 1;
			}
			nkeys++;
		}
		else if (c == 'x')
		{
			allow_expired = 1;
		}
		else
		{
			return 2;
		}
	}
	if (nkeys == 0 || argc - optind != 1)
	{
		ub_err("usage: ubpkg verify -k pubkey [-k pubkey2] [--allow-expired] index-file");
		return 2;
	}
	rc = ub_index_load(argv[optind], keys, nkeys, allow_expired, &ix);
	if (rc != 0)
		return 1;
	printf("index serial %llu ok: %zu packages\n", (unsigned long long)ix.serial, ix.nrecs);
	pkg_index_free(&ix);
	return 0;
}
