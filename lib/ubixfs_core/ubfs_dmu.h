/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS DMU (Data Management Unit) — the copy-on-write object engine.  Objects
 * (numbered dnodes) live in an object set; an object's data is addressed by a
 * CoW block-pointer tree (checksummed, hole-aware/sparse).  Built on the SPA.
 * See docs/design/ubixfs-pool-plan.md.
 *
 * v1 scope: fixed 4 KB data blocks (variable recordsize later), 1 inline blkptr
 * per dnode with a radix indirect tree (fan-out 32) that grows with the object,
 * no compression yet (field wired, pass-through).
 */
#ifndef _UBIXFS_DMU_H
#define _UBIXFS_DMU_H

#include "ubfs_spa.h"

/* In-memory recycle list of freed object numbers, so create/delete churn reuses
 * dnode slots instead of growing the metadnode without bound.  O(1) push/pop (no
 * scanning); in-memory only (a fresh mount starts empty and resumes bumping —
 * freed slots from a prior mount are simply not reclaimed, a bounded loss). */
#define UBFS_OBJ_FREELIST 256

/* In-memory object-set handle (distinct from the on-disk ubfs_objset_t). */
typedef struct ubfs_dmu_os
{
	ubfs_pool_t *pool;
	ubfs_dnode_t metadnode;                /* describes the dnode-array object */
	uint64_t type;                         /* enum ubfs_ostype */
	uint64_t next_object;                  /* bump allocator for object numbers */
	ubfs_blkptr_t rootbp;                  /* current on-disk objset-header block (freed on next sync) */
	uint64_t free_objs[UBFS_OBJ_FREELIST]; /* recycle list of freed object numbers */
	uint32_t free_obj_n;                   /* entries in free_objs[] */
} ubfs_dmu_os_t;

/* ── object set ─────────────────────────────────────────────────────────────*/

/** Initialise a fresh, empty object set in memory (nothing on disk until sync). */
void ubfs_dmu_objset_create(ubfs_pool_t *pool, uint64_t ostype, ubfs_dmu_os_t *os);

/** Load an object set from its on-disk root block pointer. */
int ubfs_dmu_objset_open(ubfs_pool_t *pool, const ubfs_blkptr_t *bp, ubfs_dmu_os_t *os);

/** Write the object set (its metadnode tree is already CoW'd in memory) into a
 *  fresh block; returns the block pointer to it in *bp_out.  The caller stores
 *  that bp in the uberblock (v1/test) or a dataset (DSL, Phase 4). */
int ubfs_dmu_objset_sync(ubfs_dmu_os_t *os, ubfs_blkptr_t *bp_out);

/* ── objects ────────────────────────────────────────────────────────────────*/

/** Allocate a new object; returns its number (>=1) or 0 on error.  `otype` is
 *  an enum ubfs_otype, `bonustype` an enum ubfs_btype. */
uint64_t ubfs_dmu_object_alloc(ubfs_dmu_os_t *os, uint8_t otype, uint8_t bonustype);

/** Read / write an object's dnode (the bonus buffer travels with it). */
int ubfs_dmu_dnode_get(ubfs_dmu_os_t *os, uint64_t obj, ubfs_dnode_t *dn);
int ubfs_dmu_dnode_put(ubfs_dmu_os_t *os, uint64_t obj, const ubfs_dnode_t *dn);

/** Free an object: release its entire data + indirect block tree (through the
 *  SPA's deferred-free chokepoint) and mark its dnode unused.  Idempotent.
 *  @return 0 on success, negative on error. */
int ubfs_dmu_object_free(ubfs_dmu_os_t *os, uint64_t obj);

/* ── object data (byte-granular; partial blocks are read-modify-write) ───────*/
int ubfs_dmu_write(ubfs_dmu_os_t *os, uint64_t obj, uint64_t off, const void *buf, uint64_t len);
int ubfs_dmu_read(ubfs_dmu_os_t *os, uint64_t obj, uint64_t off, void *buf, uint64_t len);

#endif /* _UBIXFS_DMU_H */
