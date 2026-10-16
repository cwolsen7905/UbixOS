/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * libpkg internals: the `key: value` line iterator and field parsers shared by
 * the manifest and index parsers.
 */
#ifndef UBIXOS_PKG_INTERNAL_H
#define UBIXOS_PKG_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <ubix/pkg.h>

#define PKG_KV_KEY_MAX 32

#define PKG_KV_EOF 0
#define PKG_KV_PAIR 1
#define PKG_KV_BLANK 2

/** Iterator over a text buffer of `key: value` lines (`#` comments skipped). */
struct pkg_kv
{
	const char *p;
	const char *end;
	unsigned line; /**< 1-based number of the line last returned */
	char key[PKG_KV_KEY_MAX];
	const char *val; /**< trimmed value, not NUL-terminated */
	size_t vlen;
};

void pkg_kv_init(struct pkg_kv *it, const char *text, size_t len);

/**
 * Advance one line.
 * @return PKG_KV_PAIR, PKG_KV_BLANK, PKG_KV_EOF, or -EINVAL for a line with no
 *         `:` or an over-long or empty key.
 */
int pkg_kv_next(struct pkg_kv *it);

/** Copy a value into a NUL-terminated buffer.  @return 0 or -EINVAL if it does not fit. */
int pkg_kv_copy(char *dst, size_t dstsz, const char *val, size_t vlen);

/** Parse an unsigned number in the given base (8, 10).  @return 0 or -EINVAL. */
int pkg_kv_u64(const char *val, size_t vlen, int base, uint64_t *out);

/** Parse `[name] [op version]` (with_name selects the leading name).  @return 0 or -EINVAL. */
int pkg_dep_parse(const char *val, size_t vlen, int with_name, struct pkg_dep *dep);

/** Parse a comma-separated dependency list, appending to *list.  @return 0, -EINVAL, -ENOMEM. */
int pkg_deplist_parse(const char *val, size_t vlen, struct pkg_dep **list, size_t *n);

/** Grow a heap array of `elem`-byte items to hold n+1.  @return 0 or -ENOMEM. */
int pkg_grow(void **arr, size_t n, size_t *cap, size_t elem);

#endif /* UBIXOS_PKG_INTERNAL_H */
