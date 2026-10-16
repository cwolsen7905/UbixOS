/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * libpkg — `key: value` line iterator and shared field parsers.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "pkg_internal.h"

/** Start iterating a text buffer. */
void pkg_kv_init(struct pkg_kv *it, const char *text, size_t len)
{
	it->p = text;
	it->end = text + len;
	it->line = 0;
	it->key[0] = '\0';
	it->val = text;
	it->vlen = 0;
}

/** @return nonzero for a blank character other than newline. */
static int is_blank(char c)
{
	return c == ' ' || c == '\t' || c == '\r';
}

/** Advance one line. */
int pkg_kv_next(struct pkg_kv *it)
{
	const char *s;
	const char *e;
	const char *colon;
	size_t klen;

	for (;;)
	{
		if (it->p >= it->end)
			return PKG_KV_EOF;
		s = it->p;
		e = memchr(s, '\n', (size_t)(it->end - s));
		it->p = e != NULL ? e + 1 : it->end;
		if (e == NULL)
			e = it->end;
		it->line++;
		while (s < e && is_blank(*s))
			s++;
		while (e > s && is_blank(e[-1]))
			e--;
		if (s == e)
			return PKG_KV_BLANK;
		if (*s == '#')
			continue;
		break;
	}
	colon = memchr(s, ':', (size_t)(e - s));
	if (colon == NULL)
		return -EINVAL;
	klen = (size_t)(colon - s);
	while (klen > 0 && is_blank(s[klen - 1]))
		klen--;
	if (klen == 0 || klen >= PKG_KV_KEY_MAX)
		return -EINVAL;
	memcpy(it->key, s, klen);
	it->key[klen] = '\0';
	s = colon + 1;
	while (s < e && is_blank(*s))
		s++;
	it->val = s;
	it->vlen = (size_t)(e - s);
	return PKG_KV_PAIR;
}

/** Copy a value into a NUL-terminated buffer. */
int pkg_kv_copy(char *dst, size_t dstsz, const char *val, size_t vlen)
{
	size_t i;

	if (vlen >= dstsz)
		return -EINVAL;
	for (i = 0; i < vlen; i++)
	{
		if ((unsigned char)val[i] < 0x20 || val[i] == 0x7f)
			return -EINVAL;
	}
	memcpy(dst, val, vlen);
	dst[vlen] = '\0';
	return 0;
}

/** Parse an unsigned number. */
int pkg_kv_u64(const char *val, size_t vlen, int base, uint64_t *out)
{
	uint64_t v = 0;
	size_t i;

	if (vlen == 0)
		return -EINVAL;
	for (i = 0; i < vlen; i++)
	{
		unsigned d = (unsigned)(val[i] - '0');

		if (d >= (unsigned)base)
			return -EINVAL;
		if (v > (UINT64_MAX - d) / (uint64_t)base)
			return -EINVAL;
		v = v * (uint64_t)base + d;
	}
	*out = v;
	return 0;
}

/** Split the next whitespace-delimited token.  @return its length (0 = none). */
static size_t next_token(const char **s, const char *end, const char **tok)
{
	const char *p = *s;
	size_t n;

	while (p < end && is_blank(*p))
		p++;
	*tok = p;
	while (p < end && !is_blank(*p))
		p++;
	n = (size_t)(p - *tok);
	*s = p;
	return n;
}

/** Parse `[name] [op version]`. */
int pkg_dep_parse(const char *val, size_t vlen, int with_name, struct pkg_dep *dep)
{
	const char *s = val;
	const char *end = val + vlen;
	const char *tok;
	size_t n;
	size_t oplen = 0;

	memset(dep, 0, sizeof *dep);
	if (with_name)
	{
		n = next_token(&s, end, &tok);
		if (n == 0 || pkg_kv_copy(dep->name, sizeof dep->name, tok, n) != 0)
			return -EINVAL;
	}
	n = next_token(&s, end, &tok);
	if (n == 0)
		return with_name ? 0 : -EINVAL;
	while (oplen < n && (tok[oplen] == '<' || tok[oplen] == '>' || tok[oplen] == '='))
		oplen++;
	if (oplen == 2 && tok[0] == '>' && tok[1] == '=')
		dep->op = PKG_OP_GE;
	else if (oplen == 2 && tok[0] == '<' && tok[1] == '=')
		dep->op = PKG_OP_LE;
	else if (oplen == 1 && tok[0] == '>')
		dep->op = PKG_OP_GT;
	else if (oplen == 1 && tok[0] == '<')
		dep->op = PKG_OP_LT;
	else if (oplen == 1 && tok[0] == '=')
		dep->op = PKG_OP_EQ;
	else
		return -EINVAL;
	tok += oplen;
	n -= oplen;
	if (n == 0)
		n = next_token(&s, end, &tok);
	if (n == 0 || pkg_kv_copy(dep->version, sizeof dep->version, tok, n) != 0)
		return -EINVAL;
	if (next_token(&s, end, &tok) != 0)
		return -EINVAL;
	return 0;
}

/** Grow a heap array to hold one more element. */
int pkg_grow(void **arr, size_t n, size_t *cap, size_t elem)
{
	void *p;
	size_t ncap;

	if (n < *cap)
		return 0;
	ncap = *cap == 0 ? 16 : *cap * 2;
	if (ncap > SIZE_MAX / elem)
		return -ENOMEM;
	p = realloc(*arr, ncap * elem);
	if (p == NULL)
		return -ENOMEM;
	*arr = p;
	*cap = ncap;
	return 0;
}

/** Parse a comma-separated dependency list. */
int pkg_deplist_parse(const char *val, size_t vlen, struct pkg_dep **list, size_t *n)
{
	const char *p = val;
	const char *end = val + vlen;
	size_t cap = *n;

	while (p < end)
	{
		const char *comma = memchr(p, ',', (size_t)(end - p));
		const char *item_end = comma != NULL ? comma : end;
		struct pkg_dep d;
		int rc;

		rc = pkg_dep_parse(p, (size_t)(item_end - p), 1, &d);
		if (rc != 0)
			return rc;
		rc = pkg_grow((void **)(void *)list, *n, &cap, sizeof **list);
		if (rc != 0)
			return rc;
		(*list)[(*n)++] = d;
		p = comma != NULL ? comma + 1 : end;
	}
	return 0;
}
