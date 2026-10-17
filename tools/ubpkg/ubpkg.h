/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubpkg — host-side package tool: key generation, package build, signed index
 * build, verification and `--root` install.  Built on lib/libpkg.  See
 * docs/design/packages-plan.md.
 *
 * Convention: functions return 0 (or a count) on success and -errno on
 * failure; commands print a one-line diagnostic to stderr and return a process
 * exit status.
 */
#ifndef UBPKG_H
#define UBPKG_H

#include <stddef.h>
#include <stdint.h>
#include <ubix/pkg.h>

#define UBPKG_DEFAULT_PREFIX "usr/local/"
#define UBPKG_PRIV_KEY "ecdsa-p256-private"
#define UBPKG_MANIFEST_MAX (64u << 20)

/** Growable byte buffer. */
struct ub_buf
{
	uint8_t *p;
	size_t n;
	size_t cap;
};

/* util.c */
void ub_err(const char *fmt, ...);
int ub_buf_add(struct ub_buf *b, const void *data, size_t len);
int ub_buf_printf(struct ub_buf *b, const char *fmt, ...);
int ub_read_file(const char *path, uint8_t **buf, size_t *len);
int ub_write_new(const char *path, const void *data, size_t len, unsigned mode);
int ub_sha256_file(const char *path, uint8_t out[PKG_SHA256_LEN], uint64_t *size);
int ub_load_pubkey(const char *path, struct pkg_key *key);
int ub_mkdirs(const char *path);
const char *ub_basename(const char *path);

/* pkgfile.c: verified read / extract of a .upkg */
int ub_pkg_extract(const char *pkg, const char *root, const char *prefix, struct pkg_manifest *out);

/* index.c: index loading shared with install */
int ub_index_load(
    const char *indexpath, const struct pkg_key *keys, size_t nkeys, int allow_expired, struct pkg_index *ix);
int ub_index_latest(const char *repo, char *path, size_t pathsz);

/* commands */
int cmd_keygen(int argc, char **argv);
int cmd_build(int argc, char **argv);
int cmd_index(int argc, char **argv);
int cmd_verify(int argc, char **argv);
int cmd_install(int argc, char **argv);

#endif /* UBPKG_H */
