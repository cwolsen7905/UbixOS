/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS DSL (Dataset/Snapshot Layer) — datasets on top of the DMU.  The
 * uberblock points at the MOS (a meta object set); the MOS holds a name→object
 * dataset directory and one dataset object per dataset (bonus = ubfs_dataset_phys
 * pointing at that dataset's own object set).  This is the
 * uberblock→MOS→dataset→objset indirection that keeps snapshots addable later
 * (snapshot hook #2).  Two dataset kinds: a POSIX filesystem and a raw volume.
 * See docs/design/ubixfs-pool-plan.md.
 */
#ifndef _UBIXFS_DSL_H
#define _UBIXFS_DSL_H

#include "ubfs_dmu.h"

#define UBFS_MOS_DIR_OBJ 1  /* the dataset directory object within the MOS */
#define UBFS_VOL_DATA_OBJ 1 /* the volume-data object within a volume's objset */

typedef struct ubfs_dsl
{
	ubfs_pool_t *pool;
	ubfs_dmu_os_t mos; /* the meta object set (in memory) */
} ubfs_dsl_t;

/* ── lifecycle ──────────────────────────────────────────────────────────────*/

/** Create a fresh MOS with an empty dataset directory (in memory). */
int ubfs_dsl_create(ubfs_pool_t *pool, ubfs_dsl_t *dsl);

/** Open the MOS from the pool's current uberblock. */
int ubfs_dsl_open(ubfs_pool_t *pool, ubfs_dsl_t *dsl);

/** Sync the MOS object set and commit the pool (the atomic publish). */
int ubfs_dsl_sync(ubfs_dsl_t *dsl);

/* ── datasets ───────────────────────────────────────────────────────────────*/

/** Create a filesystem dataset `name`; returns its MOS object number. */
int ubfs_dsl_create_fs(ubfs_dsl_t *dsl, const char *name, uint64_t *ds_obj);

/** Create a volume dataset `name` of `volsize` bytes; returns its MOS object #. */
int ubfs_dsl_create_volume(ubfs_dsl_t *dsl, const char *name, uint64_t volsize, uint64_t *ds_obj);

/** Look up a dataset by name → its MOS object number. */
int ubfs_dsl_lookup(ubfs_dsl_t *dsl, const char *name, uint64_t *ds_obj);

/** Iterate every dataset in the pool (the MOS dataset directory).  `cb` receives
 *  each dataset's name and MOS object number; it returns non-zero to stop early
 *  (that value is returned).  @return 0 on full iteration, the cb's stop value,
 *  or negative on error. */
typedef int (*ubfs_dsl_cb)(void *arg, const char *name, uint64_t ds_obj);
int ubfs_dsl_foreach(ubfs_dsl_t *dsl, ubfs_dsl_cb cb, void *arg);

/** Open a dataset's object set (and read its properties). */
int ubfs_dsl_open_dataset(ubfs_dsl_t *dsl, uint64_t ds_obj, ubfs_dataset_phys_t *dp, ubfs_dmu_os_t *os);

/** True if `rsz` is a legal recordsize: a power of two from one block (4 KiB) up
 *  to UBFS_RECORDSIZE_MAX (1 MiB). */
int ubfs_recordsize_valid(uint64_t rsz);

/** Change a dataset's `recordsize` property (affects only files created after).
 *  @return 0 on success, negative if the value is invalid or the dataset is bad.
 *  Caller must ubfs_dsl_sync afterwards to persist. */
int ubfs_dsl_set_recordsize(ubfs_dsl_t *dsl, uint64_t ds_obj, uint64_t rsz);

/** After modifying a dataset's object set, push its new root back into the MOS
 *  (updates the dataset's ubfs_dataset_phys.bp).  Call before ubfs_dsl_sync. */
int ubfs_dsl_sync_dataset(ubfs_dsl_t *dsl, uint64_t ds_obj, ubfs_dmu_os_t *os);

#endif /* _UBIXFS_DSL_H */
