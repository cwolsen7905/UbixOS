/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * mkpkg — a small host-side package builder and signer for testing the
 * on-device pkg client without the full ubpkg tool.  Builds a reproducible
 * .upkg (ustar + gzip, +MANIFEST first, pax `path` for long names) from a
 * staging directory, generates P-256 keys and signs a package file the way
 * `pkg add --local` expects (ECDSA over the lowercase hex SHA-256 + newline).
 *
 *   mkpkg keygen <base>                       writes <base>.key and <base>.pub
 *   mkpkg build <out.upkg> <name> <version> <arch> <stagedir> [manifest-line ...]
 *   mkpkg sign <base>.key <file>              writes <file>.sig
 *
 * Environment: MKPKG_BADHASH=1 records a wrong hash for the first file.
 */

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <bearssl.h>
#include <zlib.h>
#include <ubix/pkg.h>

struct ent
{
	char *path;
	char *target;
	mode_t mode;
	int type; /* 'f' 'd' 'l' */
	off_t size;
};

static struct ent *g_ents;
static size_t g_nents;
static unsigned char *g_tar;
static size_t g_tarlen, g_tarcap;

/** Print an error and exit. */
static void die(const char *what)
{
	fprintf(stderr, "mkpkg: %s\n", what);
	exit(1);
}

/** Append bytes to the tar buffer. */
static void tar_put(const void *p, size_t n)
{
	if (g_tarlen + n > g_tarcap)
	{
		g_tarcap = (g_tarlen + n) * 2 + 4096;
		if ((g_tar = realloc(g_tar, g_tarcap)) == NULL)
			die("out of memory");
	}
	memcpy(g_tar + g_tarlen, p, n);
	g_tarlen += n;
}

/** Emit one 512-byte ustar header. */
static void tar_header(const char *name, char type, unsigned mode, uint64_t size, const char *link)
{
	unsigned char h[512];
	unsigned sum = 0;
	size_t i;

	memset(h, 0, sizeof h);
	snprintf((char *)h, 100, "%s", name);
	snprintf((char *)h + 100, 8, "%07o", mode);
	snprintf((char *)h + 108, 8, "%07o", 0);
	snprintf((char *)h + 116, 8, "%07o", 0);
	snprintf((char *)h + 124, 12, "%011llo", (unsigned long long)size);
	snprintf((char *)h + 136, 12, "%011o", 0);
	memset(h + 148, ' ', 8);
	h[156] = (unsigned char)type;
	if (link != NULL)
		snprintf((char *)h + 157, 100, "%s", link);
	memcpy(h + 257,
	       "ustar\0"
	       "00",
	       8);
	for (i = 0; i < sizeof h; i++)
		sum += h[i];
	snprintf((char *)h + 148, 8, "%06o", sum);
	h[154] = '\0';
	h[155] = ' ';
	tar_put(h, sizeof h);
}

/** Emit a member: header (with a pax `path` record if the name is long), data, padding. */
static void tar_member(const char *name, char type, unsigned mode, const void *data, uint64_t size, const char *link)
{
	static const unsigned char zero[512];

	if (strlen(name) > 99)
	{
		char rec[PKG_PATH_MAX + 32];
		size_t body = strlen(name) + 7, len = body; /* " path=" + name + "\n" */
		int digits;

		do
		{
			digits = snprintf(NULL, 0, "%zu", len);
			len = body + (size_t)digits;
		} while (snprintf(NULL, 0, "%zu", len) != digits);
		snprintf(rec, sizeof rec, "%zu path=%s\n", len, name);
		tar_header("PaxHeader", 'x', 0644, strlen(rec), NULL);
		tar_put(rec, strlen(rec));
		tar_put(zero, (512 - strlen(rec) % 512) % 512);
	}
	tar_header(name, type, mode, size, link);
	if (size > 0)
	{
		tar_put(data, (size_t)size);
		tar_put(zero, (size_t)((512 - size % 512) % 512));
	}
}

/** Recursively collect the staging tree. */
static void walk(const char *root, const char *rel)
{
	char abs[PKG_PATH_MAX * 2], sub[PKG_PATH_MAX];
	struct dirent *de;
	DIR *d;

	snprintf(abs, sizeof abs, "%s/%s", root, rel);
	if ((d = opendir(rel[0] ? abs : root)) == NULL)
		die("cannot read stage dir");
	while ((de = readdir(d)) != NULL)
	{
		struct stat st;
		struct ent *e;
		char full[PKG_PATH_MAX * 2];

		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		snprintf(sub, sizeof sub, "%s%s%s", rel, rel[0] ? "/" : "", de->d_name);
		snprintf(full, sizeof full, "%s/%s", root, sub);
		if (lstat(full, &st) < 0)
			die("lstat");
		if (strcmp(sub, "usr") == 0 || strcmp(sub, "usr/local") == 0)
		{
			walk(root, sub); /* the prefix itself belongs to the base layout */
			continue;
		}
		g_ents = realloc(g_ents, (g_nents + 1) * sizeof *g_ents);
		e = &g_ents[g_nents++];
		memset(e, 0, sizeof *e);
		e->path = strdup(sub);
		e->mode = st.st_mode & 0777;
		e->size = st.st_size;
		if (S_ISDIR(st.st_mode))
		{
			e->type = 'd';
			walk(root, sub);
		}
		else if (S_ISLNK(st.st_mode))
		{
			char t[PKG_PATH_MAX];
			ssize_t n = readlink(full, t, sizeof t - 1);

			if (n < 0)
				die("readlink");
			t[n] = '\0';
			e->type = 'l';
			e->target = strdup(t);
		}
		else
			e->type = 'f';
	}
	closedir(d);
}

/** qsort order: by path. */
static int cmp_ent(const void *a, const void *b)
{
	return strcmp(((const struct ent *)a)->path, ((const struct ent *)b)->path);
}

/** Read a whole file. */
static unsigned char *slurp(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	unsigned char *b;
	long n;

	if (f == NULL)
		die("cannot open file");
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	b = malloc((size_t)n + 1);
	if (b == NULL || fread(b, 1, (size_t)n, f) != (size_t)n)
		die("read failed");
	fclose(f);
	*len = (size_t)n;
	return b;
}

/** gzip g_tar into out.  */
static void gzip_to(const char *out)
{
	z_stream z;
	unsigned char *buf;
	size_t cap = g_tarlen + g_tarlen / 100 + 1024;
	FILE *f;

	memset(&z, 0, sizeof z);
	if (deflateInit2(&z, 9, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		die("deflateInit2");
	buf = malloc(cap);
	z.next_in = g_tar;
	z.avail_in = (uInt)g_tarlen;
	z.next_out = buf;
	z.avail_out = (uInt)cap;
	if (deflate(&z, Z_FINISH) != Z_STREAM_END)
		die("deflate");
	if ((f = fopen(out, "wb")) == NULL || fwrite(buf, 1, z.total_out, f) != z.total_out || fclose(f) != 0)
		die("write failed");
	deflateEnd(&z);
	free(buf);
}

/** build: stage dir to .upkg. */
static int build(int argc, char **argv)
{
	const char *out = argv[0], *name = argv[1], *ver = argv[2], *arch = argv[3], *stage = argv[4];
	char *man = malloc(1 << 22);
	size_t ml = 0, i, nfile = 0;
	int badhash = getenv("MKPKG_BADHASH") != NULL;

	if (argc < 5)
		die("usage: build out name version arch stagedir [line...]");
	walk(stage, "");
	qsort(g_ents, g_nents, sizeof *g_ents, cmp_ent);
	ml += (size_t)snprintf(man + ml,
	                       4096,
	                       "name: %s\nversion: %s\nrevision: 1\narch: %s\nabi: 3\ncomment: test package %s\n",
	                       name,
	                       ver,
	                       arch,
	                       name);
	for (i = 5; i < (size_t)argc; i++)
		ml += (size_t)snprintf(man + ml, 4096, "%s\n", argv[i]);
	for (i = 0; i < g_nents; i++)
	{
		struct ent *e = &g_ents[i];
		char full[PKG_PATH_MAX * 2], hex[PKG_SHA256_HEX];

		if (e->type == 'd')
			ml += (size_t)snprintf(man + ml, 4096, "dir: %04o %s\n", (unsigned)e->mode, e->path);
		else if (e->type == 'l')
			ml += (size_t)snprintf(man + ml, 4096, "link: %s -> %s\n", e->path, e->target);
		else
		{
			unsigned char d[PKG_SHA256_LEN], *data;
			size_t n;

			snprintf(full, sizeof full, "%s/%s", stage, e->path);
			data = slurp(full, &n);
			pkg_sha256(data, n, d);
			free(data);
			pkg_hex_encode(d, sizeof d, hex);
			if (badhash && nfile++ == 0)
				memset(hex, '0', 64);
			ml += (size_t)snprintf(man + ml,
			                       4096,
			                       "file: %04o %lld %s %s\n",
			                       (unsigned)e->mode,
			                       (long long)e->size,
			                       hex,
			                       e->path);
		}
	}
	tar_member(PKG_MANIFEST_NAME, '0', 0644, man, ml, NULL);
	for (i = 0; i < g_nents; i++)
	{
		struct ent *e = &g_ents[i];

		if (e->type == 'd')
			tar_member(e->path, '5', e->mode, NULL, 0, NULL);
		else if (e->type == 'l')
			tar_member(e->path, '2', 0777, NULL, 0, e->target);
		else
		{
			char full[PKG_PATH_MAX * 2];
			size_t n;
			unsigned char *data;

			snprintf(full, sizeof full, "%s/%s", stage, e->path);
			data = slurp(full, &n);
			tar_member(e->path, '0', e->mode, data, n, NULL);
			free(data);
		}
	}
	tar_put(calloc(1, 1024), 1024);
	gzip_to(out);
	return 0;
}

/** keygen: write <base>.key (private scalar, hex) and <base>.pub. */
static int keygen(const char *base)
{
	br_hmac_drbg_context rng;
	br_ec_private_key sk;
	br_ec_public_key pk;
	unsigned char seed[48], kbuf[BR_EC_KBUF_PRIV_MAX_SIZE], pbuf[BR_EC_KBUF_PUB_MAX_SIZE];
	char hex[300], path[1100];
	const br_prng_class *rc = &br_hmac_drbg_vtable;
	FILE *f;
	int fd = open("/dev/urandom", O_RDONLY);

	if (fd < 0 || read(fd, seed, sizeof seed) != (ssize_t)sizeof seed)
		die("no entropy");
	close(fd);
	br_hmac_drbg_init(&rng, &br_sha256_vtable, seed, sizeof seed);
	(void)rc;
	if (br_ec_keygen(&rng.vtable, &br_ec_p256_m31, &sk, kbuf, BR_EC_secp256r1) == 0)
		die("keygen failed");
	if (br_ec_compute_pub(&br_ec_p256_m31, &pk, pbuf, &sk) != PKG_ECDSA_PUB_LEN)
		die("compute_pub failed");
	pkg_hex_encode(sk.x, sk.xlen, hex);
	snprintf(path, sizeof path, "%s.key", base);
	if ((f = fopen(path, "w")) == NULL)
		die("cannot write key");
	fprintf(f, "%s\n", hex);
	fclose(f);
	pkg_hex_encode(pk.q, pk.qlen, hex);
	snprintf(path, sizeof path, "%s.pub", base);
	if ((f = fopen(path, "w")) == NULL)
		die("cannot write pub");
	fprintf(f, "# test key\necdsa-p256: %s\n", hex);
	fclose(f);
	return 0;
}

/** sign: sign the hex SHA-256 + newline of a file. */
static int sign(const char *keyfile, const char *file)
{
	size_t kl, fl;
	unsigned char *kt = slurp(keyfile, &kl), *data = slurp(file, &fl), d[PKG_SHA256_LEN], x[32], h[32];
	unsigned char sig[160];
	char msg[PKG_SHA256_HEX + 1], hex[400], path[1100];
	br_ec_private_key sk;
	br_sha256_context sc;
	size_t n;
	FILE *f;

	kt[kl] = '\0';
	if (pkg_hex_decode((char *)kt, 64, x, 32) != 0)
		die("bad key file");
	sk.curve = BR_EC_secp256r1;
	sk.x = x;
	sk.xlen = 32;
	pkg_sha256(data, fl, d);
	pkg_hex_encode(d, sizeof d, msg);
	msg[64] = '\n';
	br_sha256_init(&sc);
	br_sha256_update(&sc, msg, 65);
	br_sha256_out(&sc, h);
	if ((n = br_ecdsa_i31_sign_asn1(&br_ec_p256_m31, &br_sha256_vtable, h, &sk, sig)) == 0)
		die("sign failed");
	pkg_hex_encode(sig, n, hex);
	snprintf(path, sizeof path, "%s.sig", file);
	if ((f = fopen(path, "w")) == NULL)
		die("cannot write sig");
	fprintf(f, "ecdsa-p256-sha256: %s\n", hex);
	fclose(f);
	return 0;
}

/** Entry point. */
int main(int argc, char **argv)
{
	if (argc >= 3 && strcmp(argv[1], "keygen") == 0)
		return keygen(argv[2]);
	if (argc >= 7 && strcmp(argv[1], "build") == 0)
		return build(argc - 2, argv + 2);
	if (argc == 4 && strcmp(argv[1], "sign") == 0)
		return sign(argv[2], argv[3]);
	fprintf(stderr, "usage: mkpkg keygen base | build out name ver arch stage [line...] | sign key file\n");
	return 2;
}
