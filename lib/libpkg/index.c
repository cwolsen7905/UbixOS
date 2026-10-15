/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * libpkg — repository index parser and freshness policy.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "pkg_internal.h"

#define INDEX_MAX_RECS (1U << 18)

#define REC_NAME 1U
#define REC_VERSION 2U
#define REC_FILE 4U
#define REC_SIZE 8U
#define REC_SHA 16U
#define REC_ALL 31U

#define HDR_SERIAL 1U
#define HDR_ABI 2U
#define HDR_ARCH 4U
#define HDR_ALL 7U

/** Release everything parsed. */
void pkg_index_free(struct pkg_index *ix)
{
	size_t i;

	for (i = 0; i < ix->nrecs; i++)
		free(ix->recs[i].depends);
	free(ix->recs);
	memset(ix, 0, sizeof *ix);
}

/** Apply one header key.  @return 0 or -EINVAL. */
static int header_key(struct pkg_index *ix, const struct pkg_kv *it, unsigned *have)
{
	uint64_t v;
	int rc = 0;

	if (strcmp(it->key, "serial") == 0)
	{
		rc = pkg_kv_u64(it->val, it->vlen, 10, &ix->serial);
		*have |= rc == 0 ? HDR_SERIAL : 0;
	}
	else if (strcmp(it->key, "abi") == 0)
	{
		rc = pkg_kv_u64(it->val, it->vlen, 10, &v);
		if (rc == 0 && v > UINT32_MAX)
			rc = -EINVAL;
		ix->abi = (uint32_t)v;
		*have |= rc == 0 ? HDR_ABI : 0;
	}
	else if (strcmp(it->key, "arch") == 0)
	{
		rc = pkg_kv_copy(ix->arch, sizeof ix->arch, it->val, it->vlen);
		*have |= (rc == 0 && it->vlen) ? HDR_ARCH : 0;
	}
	else if (strcmp(it->key, "created") == 0 || strcmp(it->key, "expires") == 0)
	{
		char buf[32];
		time_t t;

		rc = pkg_kv_copy(buf, sizeof buf, it->val, it->vlen);
		if (rc == 0)
			rc = pkg_time_parse(buf, &t);
		if (rc == 0)
		{
			if (it->key[0] == 'c')
				ix->created = t;
			else
				ix->expires = t;
		}
	}
	else if (strcmp(it->key, "previous") == 0)
	{
		rc = pkg_hex_decode(it->val, it->vlen, ix->previous, PKG_SHA256_LEN);
		ix->has_previous = rc == 0;
	}
	return rc;
}

/** Apply one record key.  @return 0 or -EINVAL/-ENOMEM. */
static int record_key(struct pkg_irec *r, const struct pkg_kv *it, unsigned *have)
{
	uint64_t v;
	int rc = 0;

	if (strcmp(it->key, "name") == 0)
	{
		rc = pkg_kv_copy(r->name, sizeof r->name, it->val, it->vlen);
		*have |= (rc == 0 && it->vlen) ? REC_NAME : 0;
	}
	else if (strcmp(it->key, "version") == 0)
	{
		rc = pkg_kv_copy(r->version, sizeof r->version, it->val, it->vlen);
		*have |= (rc == 0 && it->vlen) ? REC_VERSION : 0;
	}
	else if (strcmp(it->key, "revision") == 0)
	{
		rc = pkg_kv_u64(it->val, it->vlen, 10, &v);
		if (rc == 0 && v > UINT32_MAX)
			rc = -EINVAL;
		r->revision = (uint32_t)v;
	}
	else if (strcmp(it->key, "file") == 0)
	{
		rc = pkg_kv_copy(r->file, sizeof r->file, it->val, it->vlen);
		if (rc == 0 && !pkg_path_is_safe(r->file))
			rc = -EINVAL;
		*have |= rc == 0 ? REC_FILE : 0;
	}
	else if (strcmp(it->key, "size") == 0)
	{
		rc = pkg_kv_u64(it->val, it->vlen, 10, &r->size);
		*have |= rc == 0 ? REC_SIZE : 0;
	}
	else if (strcmp(it->key, "sha256") == 0)
	{
		rc = pkg_hex_decode(it->val, it->vlen, r->sha256, PKG_SHA256_LEN);
		*have |= rc == 0 ? REC_SHA : 0;
	}
	else if (strcmp(it->key, "base") == 0)
		rc = pkg_dep_parse(it->val, it->vlen, 0, &r->base);
	else if (strcmp(it->key, "depends") == 0)
		rc = pkg_deplist_parse(it->val, it->vlen, &r->depends, &r->ndepends);
	else if (strcmp(it->key, "comment") == 0)
		rc = pkg_kv_copy(r->comment, sizeof r->comment, it->val, it->vlen);
	return rc;
}

/** Parse an index. */
int pkg_index_parse(const char *text, size_t len, struct pkg_index *ix, unsigned *line)
{
	struct pkg_kv it;
	size_t cap = 0;
	unsigned hdr = 0;
	unsigned rec = 0;
	int in_rec = 0;
	int rc = 0;
	int r;

	memset(ix, 0, sizeof *ix);
	pkg_kv_init(&it, text, len);
	while (rc == 0 && (r = pkg_kv_next(&it)) > PKG_KV_EOF)
	{
		if (r == PKG_KV_BLANK)
		{
			if (in_rec && rec != REC_ALL)
				rc = -EINVAL;
			in_rec = 0;
			continue;
		}
		if (strcmp(it.key, "name") == 0)
		{
			if (in_rec && rec != REC_ALL)
			{
				rc = -EINVAL;
				break;
			}
			if (ix->nrecs >= INDEX_MAX_RECS)
			{
				rc = -EINVAL;
				break;
			}
			rc = pkg_grow((void **)(void *)&ix->recs, ix->nrecs, &cap, sizeof *ix->recs);
			if (rc != 0)
				break;
			memset(&ix->recs[ix->nrecs], 0, sizeof ix->recs[0]);
			ix->nrecs++;
			in_rec = 1;
			rec = 0;
		}
		if (in_rec)
			rc = record_key(&ix->recs[ix->nrecs - 1], &it, &rec);
		else
			rc = header_key(ix, &it, &hdr);
	}
	if (rc == 0 && r < 0)
		rc = r;
	if (rc == 0 && (hdr != HDR_ALL || (in_rec && rec != REC_ALL)))
		rc = -EINVAL;
	if (rc != 0)
	{
		if (line != NULL)
			*line = it.line;
		pkg_index_free(ix);
	}
	return rc;
}

/** Rollback and freeze policy. */
int pkg_index_check(const struct pkg_index *ix, time_t now, uint64_t min_serial)
{
	if (ix->serial < min_serial)
		return -ERANGE;
	if (ix->expires != 0 && now > ix->expires)
		return -ESTALE;
	return 0;
}

/** Highest-versioned record by name. */
const struct pkg_irec *pkg_index_find(const struct pkg_index *ix, const char *name)
{
	const struct pkg_irec *best = NULL;
	size_t i;

	for (i = 0; i < ix->nrecs; i++)
	{
		const struct pkg_irec *c = &ix->recs[i];
		int cmp;

		if (strcmp(c->name, name) != 0)
			continue;
		if (best == NULL)
		{
			best = c;
			continue;
		}
		cmp = pkg_version_cmp(c->version, best->version);
		if (cmp > 0 || (cmp == 0 && c->revision > best->revision))
			best = c;
	}
	return best;
}
