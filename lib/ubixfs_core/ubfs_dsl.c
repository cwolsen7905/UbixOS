/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS DSL implementation — datasets over the DMU.  See ubfs_dsl.h.
 */
#include "ubfs_dsl.h"
#include "ubfs_dir.h"
#include <string.h>

/* ── lifecycle ──────────────────────────────────────────────────────────────*/

int ubfs_dsl_create(ubfs_pool_t *pool, ubfs_dsl_t *dsl)
{
	uint64_t dirobj;

	dsl->pool = pool;
	ubfs_dmu_objset_create(pool, UBFS_OST_MOS, &dsl->mos);
	dirobj = ubfs_dmu_object_alloc(&dsl->mos, UBFS_OT_DIRECTORY, UBFS_BT_NONE);
	if (dirobj != UBFS_MOS_DIR_OBJ)
		return -1; /* layout invariant: the dataset directory is object 1 */
	return 0;
}

int ubfs_dsl_open(ubfs_pool_t *pool, ubfs_dsl_t *dsl)
{
	dsl->pool = pool;
	return ubfs_dmu_objset_open(pool, &ubfs_pool_uberblock(pool)->rootbp, &dsl->mos);
}

int ubfs_dsl_sync(ubfs_dsl_t *dsl)
{
	ubfs_blkptr_t mosbp;

	if (ubfs_dmu_objset_sync(&dsl->mos, &mosbp) < 0)
		return -1;
	return ubfs_pool_commit(dsl->pool, &mosbp);
}

/* ── datasets ───────────────────────────────────────────────────────────────*/

/* Write a dataset object's bonus (ubfs_dataset_phys) into the MOS. */
static int dataset_put(ubfs_dsl_t *dsl, uint64_t ds_obj, const ubfs_dataset_phys_t *dp)
{
	ubfs_dnode_t dn;

	if (ubfs_dmu_dnode_get(&dsl->mos, ds_obj, &dn) < 0)
		return -1;
	memset(dn.bonus, 0, sizeof(dn.bonus));
	memcpy(dn.bonus, dp, sizeof(*dp));
	dn.bonuslen = sizeof(*dp);
	dn.bonustype = UBFS_BT_DATASET;
	return ubfs_dmu_dnode_put(&dsl->mos, ds_obj, &dn);
}

static int dataset_get(ubfs_dsl_t *dsl, uint64_t ds_obj, ubfs_dataset_phys_t *dp)
{
	ubfs_dnode_t dn;

	if (ubfs_dmu_dnode_get(&dsl->mos, ds_obj, &dn) < 0)
		return -1;
	if (dn.bonustype != UBFS_BT_DATASET)
		return -1;
	memcpy(dp, dn.bonus, sizeof(*dp));
	return 0;
}

int ubfs_dsl_create_fs(ubfs_dsl_t *dsl, const char *name, uint64_t *ds_obj)
{
	ubfs_dmu_os_t       fs_os;
	ubfs_blkptr_t       fs_bp;
	ubfs_dataset_phys_t dp;
	uint64_t            obj;

	ubfs_dmu_objset_create(dsl->pool, UBFS_OST_FS, &fs_os);
	if (ubfs_dmu_objset_sync(&fs_os, &fs_bp) < 0)
		return -1;

	memset(&dp, 0, sizeof(dp));
	dp.bp = fs_bp;
	dp.type = UBFS_DS_FS;
	dp.recordsize = UBFS_BLOCK_SIZE;
	dp.compression = UBFS_CO_OFF;
	dp.atime = 1;
	strncpy(dp.name, name, sizeof(dp.name) - 1);

	obj = ubfs_dmu_object_alloc(&dsl->mos, UBFS_OT_DATASET, UBFS_BT_DATASET);
	if (obj == 0 || dataset_put(dsl, obj, &dp) < 0 || ubfs_dir_add(&dsl->mos, UBFS_MOS_DIR_OBJ, name, obj, 0) < 0)
		return -1;
	*ds_obj = obj;
	return 0;
}

int ubfs_dsl_create_volume(ubfs_dsl_t *dsl, const char *name, uint64_t volsize, uint64_t *ds_obj)
{
	ubfs_dmu_os_t       zv_os;
	ubfs_blkptr_t       zv_bp;
	ubfs_dataset_phys_t dp;
	uint64_t            obj, dataobj;

	ubfs_dmu_objset_create(dsl->pool, UBFS_OST_VOL, &zv_os);
	dataobj = ubfs_dmu_object_alloc(&zv_os, UBFS_OT_VOL, UBFS_BT_NONE); /* volume data object */
	if (dataobj != UBFS_VOL_DATA_OBJ)
		return -1;
	if (ubfs_dmu_objset_sync(&zv_os, &zv_bp) < 0)
		return -1;

	memset(&dp, 0, sizeof(dp));
	dp.bp = zv_bp;
	dp.type = UBFS_DS_VOL;
	dp.recordsize = UBFS_BLOCK_SIZE;
	dp.compression = UBFS_CO_OFF;
	dp.volsize = volsize;
	strncpy(dp.name, name, sizeof(dp.name) - 1);

	obj = ubfs_dmu_object_alloc(&dsl->mos, UBFS_OT_DATASET, UBFS_BT_DATASET);
	if (obj == 0 || dataset_put(dsl, obj, &dp) < 0 || ubfs_dir_add(&dsl->mos, UBFS_MOS_DIR_OBJ, name, obj, 0) < 0)
		return -1;
	*ds_obj = obj;
	return 0;
}

int ubfs_dsl_lookup(ubfs_dsl_t *dsl, const char *name, uint64_t *ds_obj)
{
	return ubfs_dir_lookup(&dsl->mos, UBFS_MOS_DIR_OBJ, name, ds_obj, NULL);
}

int ubfs_dsl_open_dataset(ubfs_dsl_t *dsl, uint64_t ds_obj, ubfs_dataset_phys_t *dp, ubfs_dmu_os_t *os)
{
	if (dataset_get(dsl, ds_obj, dp) < 0)
		return -1;
	return ubfs_dmu_objset_open(dsl->pool, &dp->bp, os);
}

int ubfs_dsl_sync_dataset(ubfs_dsl_t *dsl, uint64_t ds_obj, ubfs_dmu_os_t *os)
{
	ubfs_dataset_phys_t dp;
	ubfs_blkptr_t       bp;

	if (ubfs_dmu_objset_sync(os, &bp) < 0)
		return -1;
	if (dataset_get(dsl, ds_obj, &dp) < 0)
		return -1;
	dp.bp = bp;
	return dataset_put(dsl, ds_obj, &dp);
}
