/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * pkgtest — unit tests for libpkg: SHA-256, ECDSA P-256 verification, version
 * and path policy, the manifest and index parsers, and the ustar+gzip reader
 * (including its rejection cases).  Self-contained: archives and keys are
 * generated in memory, so it runs identically in-OS and on a host
 * (-DPKGTEST_HOST, see the `host` target of the Makefile).
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bearssl.h>
#include <zlib.h>
#include <ubix/pkg.h>

static int g_fail;

static const char g_manifest[] =
    "name: llvm\n"
    "version: 18.1.8\n"
    "revision: 1\n"
    "arch: aarch64\n"
    "abi: 3\n"
    "base: >= 3.1.0\n"
    "comment: Clang, lld\n"
    "license: Apache-2.0 WITH LLVM-exception\n"
    "depends: zlib >= 1.3, libfoo\n"
    "conflicts:\n"
    "installed-size: 612000000\n"
    "future-key: ignored\n"
    "file: 0755 11 ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad usr/local/bin/clang-18\n"
    "link: usr/local/bin/clang -> clang-18\n"
    "dir: 0755 usr/local/lib/clang/18/include\n"
    "file: 0644 4 e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 usr/local/etc/llvm.conf\n"
    "conf: usr/local/etc/llvm.conf\n";

static const char g_index[] = "serial: 42\n"
                              "abi: 3\n"
                              "arch: aarch64\n"
                              "created: 2026-10-15T12:00:00Z\n"
                              "expires: 2026-12-15T12:00:00Z\n"
                              "previous: ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n"
                              "\n"
                              "name: llvm\n"
                              "version: 18.1.8\n"
                              "revision: 1\n"
                              "file: pool/llvm/llvm-18.1.8-r1.upkg\n"
                              "size: 98304000\n"
                              "sha256: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n"
                              "depends: zlib >= 1.3\n"
                              "base: >= 3.1.0\n"
                              "comment: compiler\n"
                              "\n"
                              "name: llvm\n"
                              "version: 18.1.8\n"
                              "revision: 2\n"
                              "file: pool/llvm/llvm-18.1.8-r2.upkg\n"
                              "size: 98304001\n"
                              "sha256: ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n"
                              "\n"
                              "name: zlib\n"
                              "version: 1.3.1\n"
                              "file: pool/zlib/zlib-1.3.1-r0.upkg\n"
                              "size: 100\n"
                              "sha256: ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n";

/** Record one check. */
static void check(int ok, const char *what)
{
	printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok)
		g_fail = 1;
}

/* ---- SHA-256 / hex ----------------------------------------------------- */

/** SHA-256 known answers, streaming and hex. */
static void test_sha256(void)
{
	uint8_t d[PKG_SHA256_LEN];
	uint8_t d2[PKG_SHA256_LEN];
	char hex[PKG_SHA256_HEX];
	struct pkg_sha256 s;
	uint8_t back[PKG_SHA256_LEN];

	pkg_sha256("abc", 3, d);
	pkg_hex_encode(d, sizeof d, hex);
	check(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0, "sha256(abc)");
	pkg_sha256("", 0, d);
	pkg_hex_encode(d, sizeof d, hex);
	check(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0, "sha256(empty)");
	pkg_sha256_init(&s);
	pkg_sha256_update(&s, "a", 1);
	pkg_sha256_update(&s, "bc", 2);
	pkg_sha256_final(&s, d2);
	pkg_sha256("abc", 3, d);
	check(memcmp(d, d2, sizeof d) == 0, "sha256 streaming equals one-shot");
	pkg_hex_encode(d, sizeof d, hex);
	check(pkg_hex_decode(hex, 64, back, 32) == 0 && memcmp(back, d, 32) == 0, "hex round trip");
	check(pkg_hex_decode("zz", 2, back, 1) == -EINVAL, "hex rejects non-hex");
	check(pkg_hex_decode("ab", 2, back, 2) == -EINVAL, "hex rejects wrong length");
}

/* ---- ECDSA ------------------------------------------------------------- */

/** Generate a P-256 key pair deterministically and return the public point. */
static void make_key(const char *seed, br_ec_private_key *sk, uint8_t *kbuf, struct pkg_key *pub)
{
	br_hmac_drbg_context rng;
	br_ec_public_key pk;
	uint8_t pbuf[BR_EC_KBUF_PUB_MAX_SIZE];

	br_hmac_drbg_init(&rng, &br_sha256_vtable, seed, strlen(seed));
	br_ec_keygen(&rng.vtable, &br_ec_prime_i31, sk, kbuf, BR_EC_secp256r1);
	br_ec_compute_pub(&br_ec_prime_i31, &pk, pbuf, sk);
	memcpy(pub->point, pk.q, PKG_ECDSA_PUB_LEN);
}

/** Sign data, returning the DER length. */
static size_t sign(const br_ec_private_key *sk, const void *data, size_t len, uint8_t *sig)
{
	uint8_t h[PKG_SHA256_LEN];

	pkg_sha256(data, len, h);
	return br_ecdsa_i31_sign_asn1(&br_ec_prime_i31, &br_sha256_vtable, h, sk, sig);
}

/** ECDSA verify, signature files, key files and tamper cases. */
static void test_ecdsa(void)
{
	br_ec_private_key sk1;
	br_ec_private_key sk2;
	uint8_t kb1[BR_EC_KBUF_PRIV_MAX_SIZE];
	uint8_t kb2[BR_EC_KBUF_PRIV_MAX_SIZE];
	struct pkg_key k1;
	struct pkg_key k2;
	struct pkg_key trust[2];
	uint8_t sig[80];
	uint8_t bad[80];
	char text[512];
	char hexs[200];
	char keytext[300];
	struct pkg_key parsed;
	const char *msg = "serial: 42\nname: llvm\n";
	size_t n;
	size_t kl;

	make_key("pkgtest key one", &sk1, kb1, &k1);
	make_key("pkgtest key two", &sk2, kb2, &k2);
	n = sign(&sk1, msg, strlen(msg), sig);
	check(n > 0, "ecdsa sign");
	check(pkg_ecdsa_verify(&k1, msg, strlen(msg), sig, n) == 0, "ecdsa verify good");
	check(pkg_ecdsa_verify(&k2, msg, strlen(msg), sig, n) == -EBADMSG, "ecdsa verify wrong key");
	check(pkg_ecdsa_verify(&k1, "serial: 43\n", 11, sig, n) == -EBADMSG, "ecdsa verify tampered data");
	memcpy(bad, sig, n);
	bad[n - 1] ^= 1;
	check(pkg_ecdsa_verify(&k1, msg, strlen(msg), bad, n) == -EBADMSG, "ecdsa verify tampered signature");
	check(pkg_ecdsa_verify(&k1, msg, strlen(msg), sig, n - 3) == -EBADMSG, "ecdsa verify truncated signature");

	/* key file */
	memcpy(keytext, "# repo key\necdsa-p256: ", 23);
	pkg_hex_encode(k1.point, PKG_ECDSA_PUB_LEN, keytext + 23);
	kl = strlen(keytext);
	keytext[kl++] = '\n';
	keytext[kl] = '\0';
	check(pkg_key_parse(keytext, strlen(keytext), &parsed) == 0 && memcmp(parsed.point, k1.point, 65) == 0,
	      "key file parse");
	check(pkg_key_parse("nothing here\n", 13, &parsed) == -EINVAL, "key file garbage");
	check(pkg_key_parse("ecdsa-p256: 04ab\n", 17, &parsed) == -EINVAL, "key file short");

	/* signature file: rotation signs with both keys; trust list holds one */
	pkg_hex_encode(sig, n, hexs);
	snprintf(text, sizeof text, "ecdsa-p256-sha256: %s\n", hexs);
	trust[0] = k2;
	trust[1] = k1;
	check(pkg_sigfile_verify(text, strlen(text), msg, strlen(msg), trust, 2) == 0, "sigfile verifies (2nd key)");
	check(pkg_sigfile_verify(text, strlen(text), msg, strlen(msg), &k2, 1) == -EBADMSG,
	      "sigfile untrusted key rejected");
	check(pkg_sigfile_verify("junk\n", 5, msg, strlen(msg), trust, 2) == -EINVAL, "sigfile with no signature");
	check(pkg_sigfile_verify(text, strlen(text), msg, strlen(msg), trust, 0) == -EINVAL, "empty trust list");
	n = sign(&sk2, msg, strlen(msg), bad);
	pkg_hex_encode(bad, n, hexs);
	{
		char two[1024];

		snprintf(two, sizeof two, "%secdsa-p256-sha256: %s\n", text, hexs);
		check(pkg_sigfile_verify(two, strlen(two), msg, strlen(msg), &k2, 1) == 0,
		      "sigfile with two signatures, one trusted");
	}
}

/* ---- paths, versions, time --------------------------------------------- */

/** Path policy, version ordering, dependency constraints, timestamps. */
static void test_policy(void)
{
	struct pkg_dep d;
	time_t t;

	check(pkg_path_is_safe("usr/local/bin/clang"), "path ok");
	check(pkg_path_is_safe("+MANIFEST"), "path +MANIFEST ok");
	check(!pkg_path_is_safe("/etc/passwd"), "path absolute rejected");
	check(!pkg_path_is_safe("usr/../etc"), "path .. rejected");
	check(!pkg_path_is_safe(".."), "path bare .. rejected");
	check(!pkg_path_is_safe("a//b"), "path empty component rejected");
	check(!pkg_path_is_safe("a/./b"), "path . component rejected");
	check(!pkg_path_is_safe("a/b/"), "path trailing slash rejected");
	check(!pkg_path_is_safe(""), "path empty rejected");
	check(!pkg_path_is_safe("a\nb"), "path control char rejected");
	check(pkg_path_is_safe("a/..b/c..d"), "path dots inside names ok");

	check(pkg_version_cmp("1.10", "1.9") > 0, "version 1.10 > 1.9");
	check(pkg_version_cmp("1.0", "1.0.0") < 0, "version 1.0 < 1.0.0");
	check(pkg_version_cmp("2.0", "2.0") == 0, "version equal");
	check(pkg_version_cmp("1.0a", "1.0b") < 0, "version 1.0a < 1.0b");
	check(pkg_version_cmp("1.0", "1.0a") < 0, "version 1.0 < 1.0a");
	check(pkg_version_cmp("18.1.8", "3.9") > 0, "version 18 > 3");
	check(pkg_version_cmp("1.007", "1.7") == 0, "version leading zeros");

	memset(&d, 0, sizeof d);
	d.op = PKG_OP_GE;
	memcpy(d.version, "3.1.0", 6);
	check(pkg_dep_satisfied(&d, "3.1.0") && pkg_dep_satisfied(&d, "3.2") && !pkg_dep_satisfied(&d, "3.0.9"),
	      "dep >= semantics");

	check(pkg_time_parse("2026-10-15T12:00:00Z", &t) == 0 && t == 1792065600, "time parse");
	check(pkg_time_parse("1970-01-01T00:00:00Z", &t) == 0 && t == 0, "time epoch");
	check(pkg_time_parse("2026-10-15 12:00:00", &t) == -EINVAL, "time bad format");
	check(pkg_time_parse("2026-13-15T12:00:00Z", &t) == -EINVAL, "time bad month");
}

/* ---- manifest ---------------------------------------------------------- */

/** Parse a manifest and probe failure cases. */
static void test_manifest(void)
{
	struct pkg_manifest m;
	const struct pkg_mentry *e;
	unsigned line = 0;
	char *mut;
	char buf[1024];

	check(pkg_manifest_parse(g_manifest, sizeof g_manifest - 1, &m, &line) == 0, "manifest parses");
	check(strcmp(m.name, "llvm") == 0 && strcmp(m.version, "18.1.8") == 0 && m.revision == 1 && m.abi == 3,
	      "manifest header fields");
	check(m.base.op == PKG_OP_GE && strcmp(m.base.version, "3.1.0") == 0, "manifest base constraint");
	check(m.ndepends == 2 && strcmp(m.depends[0].name, "zlib") == 0 && m.depends[0].op == PKG_OP_GE &&
	          strcmp(m.depends[1].name, "libfoo") == 0 && m.depends[1].op == PKG_OP_ANY,
	      "manifest depends");
	check(m.nconflicts == 0 && m.installed_size == 612000000ULL, "manifest conflicts/installed-size");
	check(m.nentries == 4, "manifest entry count");
	e = pkg_manifest_find(&m, "usr/local/bin/clang-18");
	check(e != NULL && e->type == PKG_M_FILE && e->mode == 0755 && e->size == 11 && e->sha256[0] == 0xba &&
	          !e->conf,
	      "manifest file entry");
	e = pkg_manifest_find(&m, "usr/local/bin/clang");
	check(e != NULL && e->type == PKG_M_LINK && strcmp(e->target, "clang-18") == 0, "manifest link entry");
	e = pkg_manifest_find(&m, "usr/local/etc/llvm.conf");
	check(e != NULL && e->conf, "manifest conf flag");
	check(pkg_manifest_find(&m, "usr/local/none") == NULL, "manifest find miss");
	pkg_manifest_free(&m);

	check(pkg_manifest_parse("name: x\n", 8, &m, &line) == -EINVAL, "manifest missing fields");
	snprintf(buf, sizeof buf, "%s%s", g_manifest, "dir: 0755 ../evil\n");
	check(pkg_manifest_parse(buf, strlen(buf), &m, &line) == -EINVAL && line == 18, "manifest traversal path");
	snprintf(buf, sizeof buf, "%s%s", g_manifest, "dir: 0755 usr/local/bin/clang\n");
	check(pkg_manifest_parse(buf, strlen(buf), &m, &line) == -EINVAL, "manifest duplicate path");
	snprintf(buf, sizeof buf, "%s%s", g_manifest, "conf: usr/local/nothere\n");
	check(pkg_manifest_parse(buf, strlen(buf), &m, &line) == -EINVAL, "manifest conf of unlisted file");
	mut = strdup(g_manifest);
	memcpy(strstr(mut, "ba7816bf"), "zz", 2);
	check(pkg_manifest_parse(mut, strlen(mut), &m, &line) == -EINVAL, "manifest bad sha256");
	free(mut);
	check(pkg_manifest_parse("garbage line\n", 13, &m, &line) == -EINVAL, "manifest line without colon");
}

/* ---- index ------------------------------------------------------------- */

/** Parse an index, apply freshness policy, find by name. */
static void test_index(void)
{
	struct pkg_index ix;
	const struct pkg_irec *r;
	unsigned line = 0;
	time_t now;

	check(pkg_index_parse(g_index, sizeof g_index - 1, &ix, &line) == 0, "index parses");
	check(ix.serial == 42 && ix.abi == 3 && strcmp(ix.arch, "aarch64") == 0 && ix.has_previous && ix.nrecs == 3,
	      "index header and record count");
	r = pkg_index_find(&ix, "llvm");
	check(r != NULL && r->revision == 2 && r->size == 98304001ULL, "index find highest revision");
	check(pkg_index_find(&ix, "nothing") == NULL, "index find miss");
	check(ix.recs[0].ndepends == 1 && ix.recs[0].base.op == PKG_OP_GE, "index record depends/base");
	(void)pkg_time_parse("2026-11-01T00:00:00Z", &now);
	check(pkg_index_check(&ix, now, 42) == 0, "index fresh");
	check(pkg_index_check(&ix, now, 43) == -ERANGE, "index rollback refused");
	(void)pkg_time_parse("2027-01-01T00:00:00Z", &now);
	check(pkg_index_check(&ix, now, 0) == -ESTALE, "index expired refused");
	pkg_index_free(&ix);

	check(pkg_index_parse("serial: 1\n", 10, &ix, &line) == -EINVAL, "index missing header fields");
	check(pkg_index_parse("serial: 1\nabi: 3\narch: x86_64\n\nname: a\nversion: 1\n", 47, &ix, &line) == -EINVAL,
	      "index incomplete record");
	check(pkg_index_parse("serial: 1\nabi: 3\narch: x86_64\n\nname: a\nversion: 1\nfile: ../x\nsize: 1\n"
	                      "sha256: ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n",
	                      160,
	                      &ix,
	                      &line) == -EINVAL,
	      "index traversal file path");
}

/* ---- archive ----------------------------------------------------------- */

struct buf
{
	uint8_t *p;
	size_t n;
};

/** Append bytes to a growable buffer. */
static void buf_add(struct buf *b, const void *d, size_t n)
{
	b->p = realloc(b->p, b->n + n);
	memcpy(b->p + b->n, d, n);
	b->n += n;
}

/** Append one ustar member (header + padded data).  pax_path adds an `x` record. */
static void tar_member(struct buf *b,
                       char type,
                       const char *name,
                       const char *link,
                       unsigned mode,
                       const void *data,
                       size_t len,
                       int fix_checksum)
{
	uint8_t h[512];
	unsigned sum = 0;
	size_t i;
	static const uint8_t zero[512];

	memset(h, 0, sizeof h);
	strncpy((char *)h, name, 100);
	snprintf((char *)h + 100, 8, "%07o", mode);
	snprintf((char *)h + 108, 8, "%07o", 0);
	snprintf((char *)h + 116, 8, "%07o", 0);
	snprintf((char *)h + 124, 12, "%011o", (unsigned)len);
	snprintf((char *)h + 136, 12, "%011o", 1700000000U);
	memset(h + 148, ' ', 8);
	h[156] = (uint8_t)type;
	if (link != NULL)
		strncpy((char *)h + 157, link, 100);
	memcpy(h + 257, "ustar", 6);
	memcpy(h + 263, "00", 2);
	for (i = 0; i < 512; i++)
		sum += h[i];
	snprintf((char *)h + 148, 7, "%06o", sum + (fix_checksum ? 0U : 1U));
	h[155] = ' ';
	buf_add(b, h, 512);
	if (len > 0)
	{
		buf_add(b, data, len);
		buf_add(b, zero, (512 - len % 512) % 512);
	}
}

/** Append a pax `path` record followed by the real member. */
static void tar_pax_path(struct buf *b, const char *longpath, char type, unsigned mode, const void *data, size_t len)
{
	char rec[PKG_PATH_MAX + 64];
	int body = (int)strlen(longpath) + 1 + 5 + 1; /* " path=" + "\n" */
	int total = body + 3;                         /* 3-digit length prefix */

	snprintf(rec, sizeof rec, "%d path=%s\n", total, longpath);
	tar_member(b, 'x', "PaxHeader", NULL, 0644, rec, strlen(rec), 1);
	tar_member(b, type, "truncated-name", NULL, mode, data, len, 1);
}

/** Append the two zero end blocks. */
static void tar_end(struct buf *b)
{
	static const uint8_t zero[1024];

	buf_add(b, zero, sizeof zero);
}

/** gzip a buffer. */
static void gz(const struct buf *in, struct buf *out)
{
	z_stream z;

	memset(&z, 0, sizeof z);
	deflateInit2(&z, 6, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
	out->n = deflateBound(&z, (uLong)in->n);
	out->p = malloc(out->n);
	z.next_in = in->p;
	z.avail_in = (uInt)in->n;
	z.next_out = out->p;
	z.avail_out = (uInt)out->n;
	deflate(&z, Z_FINISH);
	out->n = z.total_out;
	deflateEnd(&z);
}

struct memsrc
{
	const uint8_t *p;
	size_t n;
	size_t off;
	size_t chunk;
};

/** pkg_read_fn over memory, in small chunks to exercise refills. */
static long mem_read(void *ctx, void *buf, size_t len)
{
	struct memsrc *s = ctx;
	size_t k = s->n - s->off;

	if (k > len)
		k = len;
	if (k > s->chunk)
		k = s->chunk;
	memcpy(buf, s->p + s->off, k);
	s->off += k;
	return (long)k;
}

/** Walk a whole archive; @return the first non-1 result of pkg_reader_next. */
static int walk(const struct buf *gzbuf, unsigned flags, const char *prefix, int *count)
{
	struct memsrc src = {gzbuf->p, gzbuf->n, 0, 777};
	struct pkg_reader *r = calloc(1, sizeof *r);
	struct pkg_entry *e = calloc(1, sizeof *e);
	int rc;
	uint8_t tmp[300];

	*count = 0;
	if (pkg_reader_open(r, mem_read, &src, flags, prefix) != 0)
		return -ENOMEM;
	while ((rc = pkg_reader_next(r, e)) == 1)
	{
		long k;

		(*count)++;
		while ((k = pkg_reader_read(r, tmp, sizeof tmp)) > 0)
			;
		if (k < 0)
		{
			rc = (int)k;
			break;
		}
	}
	pkg_reader_close(r);
	free(r);
	free(e);
	return rc;
}

/** Build, gzip and walk one archive made by `build`; @return walk result. */
static int run_case(void (*build)(struct buf *), unsigned flags, const char *prefix)
{
	struct buf t = {0, 0};
	struct buf g = {0, 0};
	int count;
	int rc;

	build(&t);
	gz(&t, &g);
	rc = walk(&g, flags, prefix, &count);
	free(t.p);
	free(g.p);
	return rc;
}

/** A well-formed archive. */
static void build_good(struct buf *t)
{
	char big[3000];
	char longpath[300];

	memset(big, 'x', sizeof big);
	memset(longpath, 'd', 250);
	memcpy(longpath, "usr/local/", 10);
	longpath[250] = '\0';
	tar_member(t, '0', "+MANIFEST", NULL, 0644, g_manifest, sizeof g_manifest - 1, 1);
	tar_member(t, '5', "usr/local/bin/", NULL, 0755, NULL, 0, 1);
	tar_member(t, '0', "usr/local/bin/clang-18", NULL, 0755, "hello world", 11, 1);
	tar_member(t, '2', "usr/local/bin/clang", "clang-18", 0777, NULL, 0, 1);
	tar_member(t, '0', "usr/local/share/big", NULL, 0644, big, sizeof big, 1);
	tar_pax_path(t, longpath, '0', 0644, "long", 4);
	tar_end(t);
}

/** Walk the good archive and check entry contents. */
static void test_archive_good(void)
{
	struct buf t = {0, 0};
	struct buf g = {0, 0};
	struct memsrc src;
	struct pkg_reader *r = calloc(1, sizeof *r);
	struct pkg_entry *e = calloc(1, sizeof *e);
	char data[16];
	long k;
	int n = 0;
	int ok = 1;

	build_good(&t);
	gz(&t, &g);
	src = (struct memsrc){g.p, g.n, 0, 100};
	check(pkg_reader_open(r, mem_read, &src, 0, "usr/local/") == 0, "reader opens");
	while (pkg_reader_next(r, e) == 1)
	{
		n++;
		if (n == 1)
			ok &= strcmp(e->path, "+MANIFEST") == 0 && e->type == PKG_FILE &&
			      e->size == sizeof g_manifest - 1;
		if (n == 2)
			ok &= strcmp(e->path, "usr/local/bin") == 0 && e->type == PKG_DIR;
		if (n == 3)
		{
			k = pkg_reader_read(r, data, sizeof data);
			ok &= strcmp(e->path, "usr/local/bin/clang-18") == 0 && e->mode == 0755 && k == 11 &&
			      memcmp(data, "hello world", 11) == 0;
		}
		if (n == 4)
			ok &= e->type == PKG_SYMLINK && strcmp(e->link, "clang-18") == 0;
		if (n == 6)
		{
			k = pkg_reader_read(r, data, sizeof data);
			ok &= strlen(e->path) == 250 && k == 4 && memcmp(data, "long", 4) == 0;
		}
	}
	check(n == 6 && ok, "archive members, content, symlink, pax long path");
	check(pkg_reader_next(r, e) == 0, "archive clean end");
	pkg_reader_close(r);
	free(r);
	free(e);
	free(t.p);
	free(g.p);
}

/** Archive with +MANIFEST not first. */
static void build_manifest_second(struct buf *t)
{
	tar_member(t, '0', "usr/local/x", NULL, 0644, "x", 1, 1);
	tar_member(t, '0', "+MANIFEST", NULL, 0644, "x", 1, 1);
	tar_end(t);
}

/** Archive with a member with a `..` path. */
static void build_traversal(struct buf *t)
{
	tar_member(t, '0', "+MANIFEST", NULL, 0644, "x", 1, 1);
	tar_member(t, '0', "usr/local/../../etc/passwd", NULL, 0644, "x", 1, 1);
	tar_end(t);
}

/** Archive with a member with an absolute path. */
static void build_absolute(struct buf *t)
{
	tar_member(t, '0', "+MANIFEST", NULL, 0644, "x", 1, 1);
	tar_member(t, '0', "/etc/passwd", NULL, 0644, "x", 1, 1);
	tar_end(t);
}

/** Archive with a hard-link member. */
static void build_hardlink(struct buf *t)
{
	tar_member(t, '0', "+MANIFEST", NULL, 0644, "x", 1, 1);
	tar_member(t, '1', "usr/local/h", "usr/local/x", 0644, NULL, 0, 1);
	tar_end(t);
}

/** Archive with a device-node member. */
static void build_device(struct buf *t)
{
	tar_member(t, '0', "+MANIFEST", NULL, 0644, "x", 1, 1);
	tar_member(t, '3', "usr/local/null", NULL, 0644, NULL, 0, 1);
	tar_end(t);
}

/** Archive with a setuid member. */
static void build_setuid(struct buf *t)
{
	tar_member(t, '0', "+MANIFEST", NULL, 0644, "x", 1, 1);
	tar_member(t, '0', "usr/local/bin/su", NULL, 04755, "x", 1, 1);
	tar_end(t);
}

/** Archive with a member outside the package prefix. */
static void build_outside_prefix(struct buf *t)
{
	tar_member(t, '0', "+MANIFEST", NULL, 0644, "x", 1, 1);
	tar_member(t, '0', "bin/sh", NULL, 0755, "x", 1, 1);
	tar_end(t);
}

/** Archive with a header with a wrong checksum. */
static void build_bad_checksum(struct buf *t)
{
	tar_member(t, '0', "+MANIFEST", NULL, 0644, "x", 1, 0);
	tar_end(t);
}

/** Archive with an archive with no end-of-archive blocks. */
static void build_no_trailer(struct buf *t)
{
	tar_member(t, '0', "+MANIFEST", NULL, 0644, "x", 1, 1);
}

/** Archive with a pax header carrying a record other than `path`. */
static void build_pax_other_key(struct buf *t)
{
	const char *rec = "30 mtime=1700000000.123456\n";

	tar_member(t, '0', "+MANIFEST", NULL, 0644, "x", 1, 1);
	tar_member(t, 'x', "PaxHeader", NULL, 0644, rec, strlen(rec), 1);
	tar_member(t, '0', "usr/local/a", NULL, 0644, "x", 1, 1);
	tar_end(t);
}

/** Rejection cases for the archive reader. */
static void test_archive_bad(void)
{
	struct buf t = {0, 0};
	struct buf g = {0, 0};
	int count;

	check(run_case(build_manifest_second, 0, NULL) == -EPERM, "first member must be +MANIFEST");
	check(run_case(build_traversal, 0, NULL) == -EPERM, "`..` member rejected");
	check(run_case(build_absolute, 0, NULL) == -EPERM, "absolute member rejected");
	check(run_case(build_hardlink, 0, NULL) == -EPERM, "hard link rejected");
	check(run_case(build_device, 0, NULL) == -EPERM, "device node rejected");
	check(run_case(build_setuid, 0, NULL) == -EPERM, "setuid rejected by default");
	check(run_case(build_setuid, PKG_READ_ALLOW_SETID, NULL) == 0, "setuid accepted with flag");
	check(run_case(build_outside_prefix, 0, "usr/local/") == -EPERM, "member outside prefix rejected");
	check(run_case(build_outside_prefix, 0, NULL) == 0, "no prefix means unrestricted");
	check(run_case(build_bad_checksum, 0, NULL) == -EINVAL, "bad header checksum rejected");
	check(run_case(build_no_trailer, 0, NULL) == -EINVAL, "missing end-of-archive rejected");
	check(run_case(build_pax_other_key, 0, NULL) == -EINVAL, "pax record other than path rejected");

	/* truncated gzip and corrupted deflate data */
	build_good(&t);
	gz(&t, &g);
	g.n -= g.n / 3;
	check(walk(&g, 0, NULL, &count) == -EINVAL, "truncated gzip rejected");
	g.n += g.n / 2;
	g.p = realloc(g.p, g.n);
	memset(g.p + 20, 0xA5, 64);
	check(walk(&g, 0, NULL, &count) < 0, "corrupt deflate data rejected");
	free(t.p);
	free(g.p);
}

/** Entry point. */
int main(void)
{
	printf("pkgtest: libpkg unit tests\n");
	test_sha256();
	test_ecdsa();
	test_policy();
	test_manifest();
	test_index();
	test_archive_good();
	test_archive_bad();
	printf("%s\n", g_fail ? "pkgtest: FAIL" : "pkgtest: PASS");
	return g_fail ? 1 : 0;
}
