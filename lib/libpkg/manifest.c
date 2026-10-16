/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * libpkg — +MANIFEST parser.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "pkg_internal.h"

#define MANIFEST_MAX_ENTRIES (1U << 20)

/** qsort/bsearch order: by path. */
static int entry_cmp(const void *a, const void *b)
{
	return strcmp(((const struct pkg_mentry *)a)->path, ((const struct pkg_mentry *)b)->path);
}

/** Release one entry's strings. */
static void entry_free(struct pkg_mentry *e)
{
	free(e->path);
	free(e->target);
}

/** Free everything parsed so far. */
void pkg_manifest_free(struct pkg_manifest *m)
{
	size_t i;

	for (i = 0; i < m->nentries; i++)
		entry_free(&m->entries[i]);
	free(m->entries);
	free(m->depends);
	free(m->conflicts);
	memset(m, 0, sizeof *m);
}

/** Duplicate n bytes as a checked, safe relative path.  @return NULL on failure. */
static char *dup_path(const char *s, size_t n)
{
	char *p;

	if (n == 0 || n >= PKG_PATH_MAX)
		return NULL;
	p = malloc(n + 1);
	if (p == NULL)
		return NULL;
	memcpy(p, s, n);
	p[n] = '\0';
	if (!pkg_path_is_safe(p))
	{
		free(p);
		return NULL;
	}
	return p;
}

/** Take the next space-delimited token from *s.  @return its length. */
static size_t token(const char **s, const char *end, const char **tok)
{
	const char *p = *s;

	while (p < end && *p == ' ')
		p++;
	*tok = p;
	while (p < end && *p != ' ' && *p != '\t')
		p++;
	*s = p;
	return (size_t)(p - *tok);
}

/** Parse one `file:`, `dir:` or `link:` value into e.  @return 0, -EINVAL, -ENOMEM. */
static int parse_entry(const struct pkg_kv *it, struct pkg_mentry *e)
{
	const char *s = it->val;
	const char *end = it->val + it->vlen;
	const char *tok;
	size_t n;
	uint64_t v;

	memset(e, 0, sizeof *e);
	if (strcmp(it->key, "link") == 0)
	{
		const char *arrow = NULL;
		size_t i;

		for (i = 0; i + 4 <= it->vlen; i++)
		{
			if (memcmp(s + i, " -> ", 4) == 0)
			{
				arrow = s + i;
				break;
			}
		}
		if (arrow == NULL)
			return -EINVAL;
		e->type = PKG_M_LINK;
		e->path = dup_path(s, (size_t)(arrow - s));
		if (e->path == NULL)
			return -EINVAL;
		arrow += 4;
		n = (size_t)(end - arrow);
		if (n == 0 || n >= PKG_PATH_MAX)
			return -EINVAL;
		e->target = malloc(n + 1);
		if (e->target == NULL)
			return -ENOMEM;
		memcpy(e->target, arrow, n);
		e->target[n] = '\0';
		return 0;
	}

	n = token(&s, end, &tok);
	if (pkg_kv_u64(tok, n, 8, &v) != 0 || v > 07777)
		return -EINVAL;
	e->mode = (uint32_t)v;
	if (strcmp(it->key, "dir") == 0)
	{
		e->type = PKG_M_DIR;
	}
	else
	{
		e->type = PKG_M_FILE;
		n = token(&s, end, &tok);
		if (pkg_kv_u64(tok, n, 10, &e->size) != 0)
			return -EINVAL;
		n = token(&s, end, &tok);
		if (pkg_hex_decode(tok, n, e->sha256, PKG_SHA256_LEN) != 0)
			return -EINVAL;
	}
	while (s < end && *s == ' ')
		s++;
	e->path = dup_path(s, (size_t)(end - s));
	return e->path != NULL ? 0 : -EINVAL;
}

/** Parse a +MANIFEST. */
int pkg_manifest_parse(const char *text, size_t len, struct pkg_manifest *m, unsigned *line)
{
	struct pkg_kv it;
	size_t cap = 0;
	size_t i;
	int pass;
	int rc = 0;
	unsigned have = 0;

	memset(m, 0, sizeof *m);
	for (pass = 0; pass < 2 && rc == 0; pass++)
	{
		int r;

		pkg_kv_init(&it, text, len);
		while ((r = pkg_kv_next(&it)) > PKG_KV_EOF && rc == 0)
		{
			uint64_t v;

			if (r != PKG_KV_PAIR)
				continue;
			if (pass == 1)
			{
				if (strcmp(it.key, "conf") == 0)
				{
					char path[PKG_PATH_MAX];
					const struct pkg_mentry *f;

					if (pkg_kv_copy(path, sizeof path, it.val, it.vlen) != 0)
						rc = -EINVAL;
					else if ((f = pkg_manifest_find(m, path)) == NULL || f->type != PKG_M_FILE)
						rc = -EINVAL;
					else
						((struct pkg_mentry *)(uintptr_t)f)->conf = 1;
				}
				continue;
			}
			if (strcmp(it.key, "name") == 0)
			{
				rc = pkg_kv_copy(m->name, sizeof m->name, it.val, it.vlen);
				have |= it.vlen ? 1U : 0U;
			}
			else if (strcmp(it.key, "version") == 0)
			{
				rc = pkg_kv_copy(m->version, sizeof m->version, it.val, it.vlen);
				have |= it.vlen ? 2U : 0U;
			}
			else if (strcmp(it.key, "revision") == 0)
			{
				rc = pkg_kv_u64(it.val, it.vlen, 10, &v);
				if (rc == 0 && v > UINT32_MAX)
					rc = -EINVAL;
				m->revision = (uint32_t)v;
			}
			else if (strcmp(it.key, "arch") == 0)
			{
				rc = pkg_kv_copy(m->arch, sizeof m->arch, it.val, it.vlen);
				have |= it.vlen ? 4U : 0U;
			}
			else if (strcmp(it.key, "abi") == 0)
			{
				rc = pkg_kv_u64(it.val, it.vlen, 10, &v);
				if (rc == 0 && v > UINT32_MAX)
					rc = -EINVAL;
				m->abi = (uint32_t)v;
				have |= rc == 0 ? 8U : 0U;
			}
			else if (strcmp(it.key, "base") == 0)
				rc = pkg_dep_parse(it.val, it.vlen, 0, &m->base);
			else if (strcmp(it.key, "comment") == 0)
				rc = pkg_kv_copy(m->comment, sizeof m->comment, it.val, it.vlen);
			else if (strcmp(it.key, "license") == 0)
				rc = pkg_kv_copy(m->license, sizeof m->license, it.val, it.vlen);
			else if (strcmp(it.key, "compression") == 0)
				rc = pkg_kv_copy(m->compression, sizeof m->compression, it.val, it.vlen);
			else if (strcmp(it.key, "installed-size") == 0)
				rc = pkg_kv_u64(it.val, it.vlen, 10, &m->installed_size);
			else if (strcmp(it.key, "recipe-hash") == 0)
				rc = pkg_kv_copy(m->recipe_hash, sizeof m->recipe_hash, it.val, it.vlen);
			else if (strcmp(it.key, "depends") == 0)
				rc = pkg_deplist_parse(it.val, it.vlen, &m->depends, &m->ndepends);
			else if (strcmp(it.key, "conflicts") == 0)
				rc = pkg_deplist_parse(it.val, it.vlen, &m->conflicts, &m->nconflicts);
			else if (strcmp(it.key, "file") == 0 || strcmp(it.key, "dir") == 0 ||
			         strcmp(it.key, "link") == 0)
			{
				if (m->nentries >= MANIFEST_MAX_ENTRIES)
					rc = -EINVAL;
				else
					rc = pkg_grow(
					    (void **)(void *)&m->entries, m->nentries, &cap, sizeof *m->entries);
				if (rc == 0)
				{
					rc = parse_entry(&it, &m->entries[m->nentries]);
					if (rc == 0)
						m->nentries++;
					else
						entry_free(&m->entries[m->nentries]);
				}
			}
		}
		if (rc == 0 && r < 0)
			rc = r;
		if (pass == 0 && rc == 0 && m->nentries > 1)
		{
			qsort(m->entries, m->nentries, sizeof *m->entries, entry_cmp);
			for (i = 1; i < m->nentries; i++)
			{
				if (strcmp(m->entries[i - 1].path, m->entries[i].path) == 0)
					rc = -EINVAL;
			}
		}
	}
	if (rc == 0 && have != 15U)
		rc = -EINVAL;
	if (rc != 0)
	{
		if (line != NULL)
			*line = it.line;
		pkg_manifest_free(m);
	}
	return rc;
}

/** Look an entry up by path. */
const struct pkg_mentry *pkg_manifest_find(const struct pkg_manifest *m, const char *path)
{
	struct pkg_mentry key;

	if (m->nentries == 0)
		return NULL;
	key.path = (char *)(uintptr_t)path;
	return bsearch(&key, m->entries, m->nentries, sizeof *m->entries, entry_cmp);
}
