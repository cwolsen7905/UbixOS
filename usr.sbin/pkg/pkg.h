/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * pkg — the on-device package client (internal header).  A thin front end over
 * lib/libpkg; see docs/design/packages-plan.md section 4.
 *
 * Failure convention: 0 (or a count) on success, -errno on failure.
 */
#ifndef PKG_CLIENT_H
#define PKG_CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <ubix/pkg.h>

/** The OS series and version packages are matched against (`abi:`, `base:`). */
#ifndef PKG_OS_ABI
#define PKG_OS_ABI 3
#endif
#ifndef PKG_OS_VERSION
#define PKG_OS_VERSION "3.1.0"
#endif

#if defined(__aarch64__)
#define PKG_OS_ARCH "aarch64"
#elif defined(__x86_64__)
#define PKG_OS_ARCH "x86_64"
#else
#define PKG_OS_ARCH "unknown"
#endif

#define PKG_ABS_MAX (PKG_PATH_MAX + PKG_PATH_MAX)
#define PKG_MANIFEST_MAX (32u * 1024u * 1024u)

/** Per-invocation options and state. */
struct ctx
{
	char root[PKG_PATH_MAX]; /**< "" for the live root */
	int force;
	int noverify;
	int lockfd;
};

/** One line of a FILES list: an installed (or base) path. */
struct frec
{
	char type; /**< 'F' file, 'D' directory, 'L' symlink */
	uint32_t mode;
	char hex[PKG_SHA256_HEX]; /**< files only */
	char *path;
	char *target; /**< links only */
};

struct flist
{
	struct frec *v;
	size_t n;
};

/** A path claimed by an installed package (or "base"). */
struct owner
{
	char *path;
	char name[PKG_NAME_MAX];
};

struct owners
{
	struct owner *v;
	size_t n;
};

/* util.c */
int ctx_path(const struct ctx *c, char *out, size_t n, const char *rel);
int read_whole(const char *path, char **buf, size_t *len, size_t max);
int write_whole(const char *path, const void *data, size_t len, unsigned mode);
int file_sha256(const char *path, uint8_t out[PKG_SHA256_LEN], uint64_t *size);
int path_through_symlink(const struct ctx *c, const char *rel);
int lock_take(struct ctx *c);
void lock_drop(struct ctx *c);
int make_dirs(const struct ctx *c, const char *rel);
int name_is_valid(const char *name);

/* db.c */
int db_path(const struct ctx *c, char *out, size_t n, const char *name, const char *leaf);
int db_installed(const struct ctx *c, const char *name);
int db_list(const struct ctx *c, char ***names, size_t *n);
void db_names_free(char **names, size_t n);
int db_read_manifest(const struct ctx *c, const char *name, struct pkg_manifest *m, char **text);
int db_read_files(const struct ctx *c, const char *name, struct flist *fl);
int db_write_files(const char *path, const struct pkg_manifest *m);
void flist_free(struct flist *fl);
int db_remove(const struct ctx *c, const char *name);
int owners_load(const struct ctx *c, struct owners *o);
const struct owner *owners_find(const struct owners *o, const char *path);
void owners_free(struct owners *o);
int journal_begin(const struct ctx *c, const char *verb, const char *name);
int journal_note(const struct ctx *c, char tag, const char *rel);
void journal_end(const struct ctx *c);
int journal_recover(struct ctx *c);

/* add.c, del.c, query.c */
int cmd_add(struct ctx *c, const char *file);
int cmd_del(struct ctx *c, const char *name);
int del_files(const struct ctx *c, const char *name, int force);
int cmd_info(struct ctx *c, const char *name);
int cmd_verify(struct ctx *c, int argc, char **argv);

#endif /* PKG_CLIENT_H */
