/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubpkg host tests: drive the ubpkg binary end to end (key, build, index,
 * verify, --root install) and check the results independently with libpkg;
 * then the failure cases: bad or foreign signature, tampered index, wrong
 * package hash, wrong file hash, path escape, symlink through the install
 * path, expired index, overwrite, setid.  Host-only; run via `bmake host`.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
#include <ubix/pkg.h>

static int g_fail;
static int g_pass;
static const char *g_ubpkg;
static char g_tmp[512];

/** Record one check. */
static void check(int ok, const char *what)
{
	if (ok)
	{
		g_pass++;
		return;
	}
	g_fail++;
	printf("FAIL: %s\n", what);
}

/** Run a shell command built from a format; @return its exit status. */
static int sh(const char *fmt, ...)
{
	char cmd[4096];
	va_list ap;
	int rc;

	va_start(ap, fmt);
	vsnprintf(cmd, sizeof cmd, fmt, ap);
	va_end(ap);
	rc = system(cmd);
	return rc == -1 ? -1 : (rc >> 8) & 0xff;
}

/** Run `ubpkg <args>` quietly, from the test's temp dir. */
static int ub(const char *fmt, ...)
{
	char args[3072];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(args, sizeof args, fmt, ap);
	va_end(ap);
	return sh("cd %s && %s %s >/dev/null%s", g_tmp, g_ubpkg, args, getenv("UBPKGTEST_VERBOSE") ? "" : " 2>&1");
}

/** Write a small file (creating parent dirs). */
static void put(const char *rel, const char *content, unsigned mode)
{
	char path[1024];
	FILE *f;

	snprintf(path, sizeof path, "%s/%s", g_tmp, rel);
	sh("mkdir -p $(dirname %s)", path);
	f = fopen(path, "wb");
	if (f == NULL)
	{
		perror(path);
		exit(2);
	}
	fputs(content, f);
	fclose(f);
	chmod(path, mode);
}

/** @return 1 if g_tmp/rel exists (symlinks not followed). */
static int exists(const char *rel)
{
	char path[1024];
	struct stat st;

	snprintf(path, sizeof path, "%s/%s", g_tmp, rel);
	return lstat(path, &st) == 0;
}

/** Read g_tmp/rel (NUL-terminated heap copy, or NULL). */
static char *slurp(const char *rel, size_t *len)
{
	char path[1024];
	FILE *f;
	char *buf;
	long n;

	snprintf(path, sizeof path, "%s/%s", g_tmp, rel);
	f = fopen(path, "rb");
	if (f == NULL)
		return NULL;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = malloc((size_t)n + 1);
	if (fread(buf, 1, (size_t)n, f) != (size_t)n)
		exit(2);
	buf[n] = '\0';
	fclose(f);
	if (len != NULL)
		*len = (size_t)n;
	return buf;
}

/** Overwrite one byte of g_tmp/rel at offset `off` with XOR 1. */
static void flip(const char *rel, long off)
{
	char path[1024];
	int fd;
	unsigned char b;

	snprintf(path, sizeof path, "%s/%s", g_tmp, rel);
	fd = open(path, O_RDWR);
	if (fd < 0 || pread(fd, &b, 1, off) != 1)
		exit(2);
	b ^= 1;
	if (pwrite(fd, &b, 1, off) != 1)
		exit(2);
	close(fd);
}

/* ---- a tiny ustar+gzip writer for hand-made hostile packages ------------ */

struct member
{
	char type; /* '0' file, '5' dir, '2' symlink */
	const char *name;
	const char *link;
	unsigned mode;
	const char *data;
};

/** Append `n` bytes to a growable buffer. */
static void bput(char **p, size_t *n, const void *d, size_t len)
{
	*p = realloc(*p, *n + len);
	memcpy(*p + *n, d, len);
	*n += len;
}

/** Append one ustar member. */
static void tar_add(char **p, size_t *n, char type, const char *name, const char *link, unsigned mode, const char *data)
{
	unsigned char h[512];
	unsigned sum = 0;
	size_t len = data != NULL ? strlen(data) : 0;
	static const char zero[512];
	size_t i;

	memset(h, 0, sizeof h);
	memcpy(h, name, strlen(name));
	snprintf((char *)h + 100, 8, "%07o", mode);
	snprintf((char *)h + 108, 8, "%07o", 0);
	snprintf((char *)h + 116, 8, "%07o", 0);
	snprintf((char *)h + 124, 12, "%011o", (unsigned)len);
	snprintf((char *)h + 136, 12, "%011o", 0);
	memset(h + 148, ' ', 8);
	h[156] = (unsigned char)type;
	if (link != NULL)
		memcpy(h + 157, link, strlen(link));
	memcpy(h + 257, "ustar", 6);
	memcpy(h + 263, "00", 2);
	for (i = 0; i < sizeof h; i++)
		sum += h[i];
	snprintf((char *)h + 148, 7, "%06o", sum);
	h[155] = ' ';
	bput(p, n, h, sizeof h);
	if (len > 0)
	{
		bput(p, n, data, len);
		bput(p, n, zero, (512 - len % 512) % 512);
	}
}

/** Write a gzip'ed tar of `manifest` + members to g_tmp/rel. */
static void make_pkg(const char *rel, const char *manifest, const struct member *m, size_t nm)
{
	char *tar = NULL;
	size_t tn = 0;
	char path[1024];
	static const char zero[1024];
	z_stream z;
	unsigned char *out;
	size_t i;
	FILE *f;

	tar_add(&tar, &tn, '0', PKG_MANIFEST_NAME, NULL, 0644, manifest);
	for (i = 0; i < nm; i++)
		tar_add(&tar, &tn, m[i].type, m[i].name, m[i].link, m[i].mode, m[i].data);
	bput(&tar, &tn, zero, sizeof zero);
	memset(&z, 0, sizeof z);
	if (deflateInit2(&z, 6, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		exit(2);
	out = malloc(deflateBound(&z, (uLong)tn) + 64);
	z.next_in = (Bytef *)tar;
	z.avail_in = (uInt)tn;
	z.next_out = out;
	z.avail_out = (uInt)(deflateBound(&z, (uLong)tn) + 64);
	if (deflate(&z, Z_FINISH) != Z_STREAM_END)
		exit(2);
	snprintf(path, sizeof path, "%s/%s", g_tmp, rel);
	f = fopen(path, "wb");
	if (f == NULL || fwrite(out, 1, z.total_out, f) != z.total_out || fclose(f) != 0)
		exit(2);
	deflateEnd(&z);
	free(out);
	free(tar);
}

/** Build a manifest line for a file member with the correct hash. */
static void file_line(char *dst, size_t sz, const char *path, const char *data, unsigned mode)
{
	uint8_t d[PKG_SHA256_LEN];
	char hex[PKG_SHA256_HEX];

	pkg_sha256(data, strlen(data), d);
	pkg_hex_encode(d, sizeof d, hex);
	snprintf(dst, sz, "file: %04o %zu %s %s\n", mode, strlen(data), hex, path);
}

/** A directory name that pushes the member path past the 100-byte ustar field. */
#define LONGDIR "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

#define MANIFEST_HEAD "name: evil\nversion: 1\nrevision: 0\narch: aarch64\nabi: 3\n"

/* ---- tests -------------------------------------------------------------- */

/** Stage a small tree. */
static void stage_tree(void)
{
	put("stage/usr/local/bin/hello", "#!/bin/sh\necho hello\n", 0755);
	put("stage/usr/local/etc/hello.conf", "greeting=hi\n", 0644);
	put("stage/usr/local/share/hello/long/" LONGDIR "/data.txt", "long path payload\n", 0644);
	sh("cd %s/stage/usr/local/bin && ln -sf hello hi", g_tmp);
}

/** Independent verification of the published repo with libpkg itself. */
static void verify_with_libpkg(void)
{
	size_t il;
	size_t sl;
	size_t kl;
	char *itext = slurp("repo/index/index.1", &il);
	char *sig = slurp("repo/index/index.1.sig", &sl);
	char *pub = slurp("k.pub", &kl);
	struct pkg_key key;
	struct pkg_index ix = {0};
	const struct pkg_irec *rec;

	check(itext && sig && pub, "repo files exist");
	check(pkg_key_parse(pub, kl, &key) == 0, "libpkg parses the generated public key");
	check(pkg_sigfile_verify(sig, sl, itext, il, &key, 1) == 0, "libpkg verifies the index signature");
	check(pkg_index_parse(itext, il, &ix, NULL) == 0, "libpkg parses the index");
	check(ix.serial == 1 && ix.abi == 3 && strcmp(ix.arch, "aarch64") == 0, "index header");
	rec = pkg_index_find(&ix, "hello");
	check(rec != NULL, "index lists hello");
	if (rec != NULL)
	{
		char path[1024];
		size_t pl;
		char *pk;
		uint8_t d[PKG_SHA256_LEN];
		struct pkg_manifest m = {0};

		snprintf(path, sizeof path, "repo/%s", rec->file);
		pk = slurp(path, &pl);
		check(pk != NULL && pl == rec->size, "package size matches the index");
		if (pk != NULL)
		{
			pkg_sha256(pk, pl, d);
			check(memcmp(d, rec->sha256, sizeof d) == 0, "package hash matches the index");
		}
		free(pk);
		check(strcmp(rec->version, "1.2.3") == 0 && rec->revision == 2, "record version/revision");
		check(rec->ndepends == 1 && strcmp(rec->depends[0].name, "zlib") == 0 &&
		          rec->depends[0].op == PKG_OP_GE,
		      "record depends");
		check(rec->base.op == PKG_OP_GE && strcmp(rec->base.version, "3.1.0") == 0, "record base constraint");
		(void)m;
	}
	pkg_index_free(&ix);
	free(itext);
	free(sig);
	free(pub);
}

/** Happy path: key, build, index, verify, install. */
static void test_roundtrip(void)
{
	char *a;
	char *b;

	stage_tree();
	check(ub("keygen k") == 0, "keygen");
	check(exists("k.key") && exists("k.pub"), "key files written");
	check(ub("keygen k") != 0, "keygen refuses to overwrite");
	{
		struct stat st;
		char p[1024];

		snprintf(p, sizeof p, "%s/k.key", g_tmp);
		check(stat(p, &st) == 0 && (st.st_mode & 077) == 0, "private key is mode 0600");
	}
	check(ub("build -n hello -v 1.2.3 -r 2 -a aarch64 -A 3 -b '>= 3.1.0' -d 'zlib >= 1.3' -c 'Hello' "
	         "-l MIT --conf usr/local/etc/hello.conf -o out stage") == 0,
	      "build");
	check(exists("out/hello-1.2.3-r2.upkg"), "package named name-version-rrev.upkg");
	check(ub("build -n hello -v 1.2.3 -r 2 -a aarch64 -A 3 -b '>= 3.1.0' -d 'zlib >= 1.3' -c 'Hello' "
	         "-l MIT --conf usr/local/etc/hello.conf -o out stage") != 0,
	      "build refuses to overwrite an existing package");
	check(ub("build -n hello -v 1.2.3 -r 2 -a aarch64 -A 3 -b '>= 3.1.0' -d 'zlib >= 1.3' -c 'Hello' "
	         "-l MIT --conf usr/local/etc/hello.conf -o out2 stage") == 0,
	      "rebuild into another dir");
	a = slurp("out/hello-1.2.3-r2.upkg", NULL);
	b = slurp("out2/hello-1.2.3-r2.upkg", NULL);
	check(a && b && strcmp(a, b) == 0, "build is reproducible");
	free(a);
	free(b);

	check(ub("index -R repo -s 1 -a aarch64 -A 3 -k k.key out/hello-1.2.3-r2.upkg") == 0, "index");
	check(ub("index -R repo -s 1 -a aarch64 -A 3 -k k.key out/hello-1.2.3-r2.upkg") != 0,
	      "index refuses to overwrite a serial");
	check(ub("verify -k k.pub repo/index/index.1") == 0, "verify index");
	verify_with_libpkg();

	check(ub("install --root root --repo repo -k k.pub hello") == 0, "install from the signed repo");
	a = slurp("root/usr/local/bin/hello", NULL);
	check(a && strcmp(a, "#!/bin/sh\necho hello\n") == 0, "file content installed");
	free(a);
	{
		struct stat st;
		char p[1024];
		char l[64] = "";

		snprintf(p, sizeof p, "%s/root/usr/local/bin/hello", g_tmp);
		check(stat(p, &st) == 0 && (st.st_mode & 0777) == 0755, "file mode installed");
		snprintf(p, sizeof p, "%s/root/usr/local/bin/hi", g_tmp);
		check(readlink(p, l, sizeof l - 1) > 0 && strcmp(l, "hello") == 0, "symlink installed");
	}
	check(exists("root/usr/local/share/hello/long/" LONGDIR "/data.txt"), "long (pax) path installed");

	/* config file handling: a modified one is kept, the new one lands beside it */
	put("root/usr/local/etc/hello.conf", "greeting=changed\n", 0644);
	check(ub("install --root root --repo repo -k k.pub hello") == 0, "reinstall over an existing tree");
	a = slurp("root/usr/local/etc/hello.conf", NULL);
	check(a && strcmp(a, "greeting=changed\n") == 0, "modified config file kept");
	free(a);
	a = slurp("root/usr/local/etc/hello.conf.upkg-new", NULL);
	check(a && strcmp(a, "greeting=hi\n") == 0, "new config written beside it as .upkg-new");
	free(a);

	check(ub("install --root root2 --no-verify out/hello-1.2.3-r2.upkg") == 0,
	      "install --no-verify of a local file");
}

/** Signature, index and package tampering. */
static void test_tamper(void)
{
	size_t il;
	char *itext;

	check(ub("keygen other") == 0, "second key");
	check(ub("verify -k other.pub repo/index/index.1") != 0, "wrong key does not verify");
	check(ub("install --root rootx --repo repo -k other.pub hello") != 0 && !exists("rootx/usr"),
	      "install with a foreign key refused, nothing written");

	/* a trust list with several keys: either may have signed */
	check(ub("verify -k other.pub -k k.pub repo/index/index.1") == 0, "multi-key trust list accepts the signer");

	/* bad signature: flip a nibble of the DER */
	sh("cp -R %s/repo %s/repo-badsig", g_tmp, g_tmp);
	flip("repo-badsig/index/index.1.sig", 30);
	check(ub("verify -k k.pub repo-badsig/index/index.1") != 0, "corrupted signature rejected");
	check(ub("install --root rooty --repo repo-badsig -k k.pub hello") != 0 && !exists("rooty/usr"),
	      "install refused on a bad signature, nothing written");

	/* tampered index text */
	sh("cp -R %s/repo %s/repo-badidx", g_tmp, g_tmp);
	itext = slurp("repo-badidx/index/index.1", &il);
	(void)itext;
	flip("repo-badidx/index/index.1", 10);
	check(ub("verify -k k.pub repo-badidx/index/index.1") != 0, "tampered index rejected");
	free(itext);

	/* missing signature */
	sh("cp -R %s/repo %s/repo-nosig && rm %s/repo-nosig/index/index.1.sig", g_tmp, g_tmp, g_tmp);
	check(ub("install --root rootz --repo repo-nosig -k k.pub hello") != 0 && !exists("rootz/usr"),
	      "unsigned index refused");

	/* wrong package hash: the signed index no longer matches the pool file */
	sh("cp -R %s/repo %s/repo-badpkg", g_tmp, g_tmp);
	flip("repo-badpkg/pool/hello/hello-1.2.3-r2.upkg", 20);
	check(ub("install --root rootw --repo repo-badpkg -k k.pub hello") != 0 && !exists("rootw/usr"),
	      "package differing from the signed hash refused, nothing written");

	/* unknown package */
	check(ub("install --root rootq --repo repo -k k.pub nosuch") != 0, "package not in the index refused");

	/* expiry */
	check(sh("cd %s && SOURCE_DATE_EPOCH=946684800 %s index -R repo-old -s 1 -a aarch64 -A 3 -k k.key -e 30 "
	         "out/hello-1.2.3-r2.upkg >/dev/null 2>&1",
	         g_tmp,
	         g_ubpkg) == 0,
	      "index created in the past");
	check(ub("verify -k k.pub repo-old/index/index.1") != 0, "expired index rejected");
	check(ub("verify -k k.pub --allow-expired repo-old/index/index.1") == 0,
	      "expired index accepted with --allow-expired");
	check(ub("install --root rootv --repo repo-old -k k.pub hello") != 0 && !exists("rootv/usr"),
	      "install from an expired index refused");

	/* serial chain: previous = hash of the prior index */
	check(ub("index -R repo -s 2 -a aarch64 -A 3 -k k.key out/hello-1.2.3-r2.upkg") == 0, "serial 2");
	itext = slurp("repo/index/index.2", &il);
	check(itext && strstr(itext, "previous: ") != NULL, "serial 2 chains to serial 1");
	free(itext);
	check(ub("index -R repo -s 4 -a aarch64 -A 3 -k k.key out/hello-1.2.3-r2.upkg") != 0,
	      "index with a missing previous serial refused");
	check(ub("verify -k k.pub repo/index/index.2") == 0, "serial 2 verifies");

	/* arch / abi mismatch with the index */
	check(ub("index -R repo-x -s 1 -a x86_64 -A 3 -k k.key out/hello-1.2.3-r2.upkg") != 0,
	      "package arch differing from the index refused");
	check(ub("index -R repo-x -s 1 -a aarch64 -A 4 -k k.key out/hello-1.2.3-r2.upkg") != 0,
	      "package abi differing from the index refused");

	/* a different file under an existing pool name must not replace it */
	put("stage2/usr/local/bin/hello", "different\n", 0755);
	check(ub("build -n hello -v 1.2.3 -r 2 -a aarch64 -A 3 -b '>= 3.1.0' -d 'zlib >= 1.3' -o out3 stage2") == 0,
	      "build a conflicting package of the same file name");
	check(ub("index -R repo -s 5 -a aarch64 -A 3 -k k.key --no-previous out3/hello-1.2.3-r2.upkg") != 0,
	      "pool never overwritten by different content");
}

/** Hostile packages. */
static void test_hostile(void)
{
	char man[2048];
	char line[512];
	struct member m[4];

	/* file hash in the manifest differs from the payload */
	snprintf(man,
	         sizeof man,
	         MANIFEST_HEAD "file: 0644 4 %s usr/local/f\n",
	         "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	m[0] = (struct member){'0', "usr/local/f", NULL, 0644, "BBBB"};
	make_pkg("bad-filehash.upkg", man, m, 1);
	check(ub("install --root h1 --no-verify bad-filehash.upkg") != 0 && !exists("h1/usr/local/f"),
	      "wrong file hash refused, nothing written");

	/* a member the manifest does not list */
	file_line(line, sizeof line, "usr/local/f", "AAAA", 0644);
	snprintf(man, sizeof man, MANIFEST_HEAD "%s", line);
	m[0] = (struct member){'0', "usr/local/f", NULL, 0644, "AAAA"};
	m[1] = (struct member){'0', "usr/local/extra", NULL, 0644, "x"};
	make_pkg("unlisted.upkg", man, m, 2);
	check(ub("install --root h2 --no-verify unlisted.upkg") != 0 && !exists("h2/usr/local/f"),
	      "member missing from the manifest refused");

	/* manifest lists something the archive lacks */
	snprintf(man, sizeof man, MANIFEST_HEAD "%s", line);
	snprintf(man + strlen(man), sizeof man - strlen(man), "dir: 0755 usr/local/never\n");
	m[0] = (struct member){'0', "usr/local/f", NULL, 0644, "AAAA"};
	make_pkg("missing.upkg", man, m, 1);
	check(ub("install --root h3 --no-verify missing.upkg") != 0 && !exists("h3/usr/local/f"),
	      "manifest entry absent from the archive refused");

	/* sanity: the same hand-made shape, done right, installs (the harness is sound) */
	snprintf(man, sizeof man, MANIFEST_HEAD "%s", line);
	m[0] = (struct member){'0', "usr/local/f", NULL, 0644, "AAAA"};
	make_pkg("good.upkg", man, m, 1);
	check(ub("install --root h4 --no-verify good.upkg") == 0 && exists("h4/usr/local/f"),
	      "hand-made control package installs");

	/* path escape: .. and absolute */
	file_line(line, sizeof line, "usr/local/../../../escape", "AAAA", 0644);
	snprintf(man, sizeof man, MANIFEST_HEAD "%s", line);
	m[0] = (struct member){'0', "usr/local/../../../escape", NULL, 0644, "AAAA"};
	make_pkg("dotdot.upkg", man, m, 1);
	check(ub("install --root h5/inner --prefix '' --no-verify dotdot.upkg") != 0 && !exists("h5/escape"),
	      "path with .. refused, nothing escapes the root");

	file_line(line, sizeof line, "/tmp/ubpkg-abs", "AAAA", 0644);
	snprintf(man, sizeof man, MANIFEST_HEAD "%s", line);
	m[0] = (struct member){'0', "/tmp/ubpkg-abs", NULL, 0644, "AAAA"};
	make_pkg("absolute.upkg", man, m, 1);
	check(ub("install --root h6 --prefix '' --no-verify absolute.upkg") != 0 && !exists("h6/tmp"),
	      "absolute path refused");
	check(access("/tmp/ubpkg-abs", F_OK) != 0, "absolute path did not write to the host");

	/* the archive itself (not just the manifest) names an escaping path */
	file_line(line, sizeof line, "usr/local/f", "AAAA", 0644);
	snprintf(man, sizeof man, MANIFEST_HEAD "%s", line);
	m[0] = (struct member){'0', "usr/local/../../../escape2", NULL, 0644, "AAAA"};
	make_pkg("arc-dotdot.upkg", man, m, 1);
	check(ub("install --root h12/inner --prefix '' --no-verify arc-dotdot.upkg") != 0 && !exists("h12/escape2"),
	      "archive member with .. refused");
	m[0] = (struct member){'0', "/tmp/ubpkg-abs2", NULL, 0644, "AAAA"};
	make_pkg("arc-abs.upkg", man, m, 1);
	check(ub("install --root h13 --prefix '' --no-verify arc-abs.upkg") != 0 &&
	          access("/tmp/ubpkg-abs2", F_OK) != 0,
	      "archive member with an absolute path refused");

	/* outside the prefix */
	file_line(line, sizeof line, "etc/passwd", "AAAA", 0644);
	snprintf(man, sizeof man, MANIFEST_HEAD "%s", line);
	m[0] = (struct member){'0', "etc/passwd", NULL, 0644, "AAAA"};
	make_pkg("outside.upkg", man, m, 1);
	check(ub("install --root h7 --no-verify outside.upkg") != 0 && !exists("h7/etc"),
	      "member outside /usr/local refused by the default prefix");

	/* setuid */
	file_line(line, sizeof line, "usr/local/su", "AAAA", 04755);
	snprintf(man, sizeof man, MANIFEST_HEAD "%s", line);
	m[0] = (struct member){'0', "usr/local/su", NULL, 04755, "AAAA"};
	make_pkg("setuid.upkg", man, m, 1);
	check(ub("install --root h8 --no-verify setuid.upkg") != 0 && !exists("h8/usr/local/su"),
	      "setuid member refused");

	/* symlink through the install path, inside one package */
	sh("mkdir -p %s/outside", g_tmp);
	file_line(line, sizeof line, "usr/local/l/pwned", "AAAA", 0644);
	snprintf(man, sizeof man, MANIFEST_HEAD "dir: 0755 usr/local\nlink: usr/local/l -> ../../../outside\n%s", line);
	m[0] = (struct member){'5', "usr/local", NULL, 0755, NULL};
	m[1] = (struct member){'2', "usr/local/l", "../../../outside", 0777, NULL};
	m[2] = (struct member){'0', "usr/local/l/pwned", NULL, 0644, "AAAA"};
	make_pkg("symthrough.upkg", man, m, 3);
	check(ub("install --root h9/root --no-verify symthrough.upkg") != 0,
	      "write through a package-made symlink refused");
	check(!exists("outside/pwned"), "nothing written through the symlink");

	/* symlink already present in the root */
	sh("mkdir -p %s/h10/root %s/outside2 && ln -s ../../outside2 %s/h10/root/usr", g_tmp, g_tmp, g_tmp);
	check(ub("install --root h10/root --no-verify good.upkg") != 0 && !exists("outside2/local"),
	      "pre-existing symlink in the root not followed");
	sh("mkdir -p %s/h11/root/usr/local %s/outside3 && ln -s ../../../outside3 %s/h11/root/usr/local/f",
	   g_tmp,
	   g_tmp,
	   g_tmp);
	check(ub("install --root h11/root --no-verify good.upkg") == 0 && !exists("outside3/f"),
	      "a symlink at the leaf is replaced, not followed");

	/* build side: tree outside the prefix, setuid, device */
	put("bad1/etc/passwd", "x\n", 0644);
	check(ub("build -n b -v 1 -a aarch64 -A 3 -o outb bad1") != 0, "build refuses files outside the prefix");
	check(ub("build -n b -v 1 -a aarch64 -A 3 --prefix '' -o outb bad1") == 0,
	      "build with an empty prefix allows them");
	put("bad2/usr/local/su", "x\n", 04755);
	chmod("bad2/usr/local/su", 04755);
	{
		char p[1024];

		snprintf(p, sizeof p, "%s/bad2/usr/local/su", g_tmp);
		chmod(p, 04755);
	}
	check(ub("build -n b -v 1 -a aarch64 -A 3 -o outb2 bad2") != 0, "build refuses setuid files");
	put("bad3/usr/local/has space", "x\n", 0644);
	check(ub("build -n b -v 1 -a aarch64 -A 3 -o outb3 bad3") != 0, "build refuses names with whitespace");
	check(ub("build -n b -v 1 -a aarch64 -A 3 -o outb4 nonexistent") != 0, "build of a missing stage dir fails");
}

/** Test entry point. */
int main(int argc, char **argv)
{
	const char *base = getenv("TMPDIR");

	if (argc != 2)
	{
		fprintf(stderr, "usage: ubpkgtest <path-to-ubpkg>\n");
		return 2;
	}
	g_ubpkg = argv[1];
	snprintf(g_tmp, sizeof g_tmp, "%s/ubpkgtest.XXXXXX", base != NULL ? base : "/tmp");
	if (mkdtemp(g_tmp) == NULL)
	{
		perror("mkdtemp");
		return 2;
	}
	test_roundtrip();
	test_tamper();
	test_hostile();
	printf("%d passed, %d failed\n", g_pass, g_fail);
	if (g_fail == 0)
		sh("rm -rf %s", g_tmp);
	else
		printf("kept %s\n", g_tmp);
	return g_fail != 0;
}
