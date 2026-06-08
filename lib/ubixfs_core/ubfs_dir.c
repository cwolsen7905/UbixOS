/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS directory helper.  See ubfs_dir.h.  Layout in the directory object:
 * [uint64_t count][ubfs_dirent count].
 */
#include "ubfs_dir.h"
#include <string.h>

#define HDR (int)sizeof(uint64_t) /* count prefix */

int ubfs_dir_lookup(ubfs_dmu_os_t *os, uint64_t dirobj, const char *name, uint64_t *obj, uint8_t *type)
{
	uint64_t count = 0, i;

	if (ubfs_dmu_read(os, dirobj, 0, &count, sizeof(count)) < 0)
		return -1;
	for (i = 0; i < count; i++)
	{
		ubfs_dirent_t e;
		if (ubfs_dmu_read(os, dirobj, HDR + i * UBFS_DIRENT_SIZE, &e, sizeof(e)) < 0)
			return -1;
		if (e.obj != 0 && strcmp(e.name, name) == 0)
		{
			*obj = e.obj;
			if (type)
				*type = e.type;
			return 0;
		}
	}
	return -1;
}

int ubfs_dir_add(ubfs_dmu_os_t *os, uint64_t dirobj, const char *name, uint64_t obj, uint8_t type)
{
	uint64_t      count = 0, i, slot;
	ubfs_dirent_t e;

	if (strlen(name) >= sizeof(e.name))
		return -1;
	if (ubfs_dmu_read(os, dirobj, 0, &count, sizeof(count)) < 0)
		return -1;

	/* Reuse a freed slot if one exists, else append. */
	slot = count;
	for (i = 0; i < count; i++)
	{
		ubfs_dirent_t t;
		if (ubfs_dmu_read(os, dirobj, HDR + i * UBFS_DIRENT_SIZE, &t, sizeof(t)) < 0)
			return -1;
		if (t.obj == 0)
		{
			slot = i;
			break;
		}
	}

	memset(&e, 0, sizeof(e));
	e.obj = obj;
	e.type = type;
	e.namelen = (uint8_t)strlen(name);
	strcpy(e.name, name);
	if (ubfs_dmu_write(os, dirobj, HDR + slot * UBFS_DIRENT_SIZE, &e, sizeof(e)) < 0)
		return -1;
	if (slot == count)
	{
		count++;
		return ubfs_dmu_write(os, dirobj, 0, &count, sizeof(count));
	}
	return 0;
}

int ubfs_dir_remove(ubfs_dmu_os_t *os, uint64_t dirobj, const char *name)
{
	uint64_t count = 0, i;

	if (ubfs_dmu_read(os, dirobj, 0, &count, sizeof(count)) < 0)
		return -1;
	for (i = 0; i < count; i++)
	{
		ubfs_dirent_t e;
		if (ubfs_dmu_read(os, dirobj, HDR + i * UBFS_DIRENT_SIZE, &e, sizeof(e)) < 0)
			return -1;
		if (e.obj != 0 && strcmp(e.name, name) == 0)
		{
			memset(&e, 0, sizeof(e)); /* obj=0 marks the slot free */
			return ubfs_dmu_write(os, dirobj, HDR + i * UBFS_DIRENT_SIZE, &e, sizeof(e));
		}
	}
	return -1;
}

int ubfs_dir_foreach(ubfs_dmu_os_t *os, uint64_t dirobj, ubfs_dir_cb cb, void *arg)
{
	uint64_t count = 0, i;

	if (ubfs_dmu_read(os, dirobj, 0, &count, sizeof(count)) < 0)
		return -1;
	for (i = 0; i < count; i++)
	{
		ubfs_dirent_t e;
		int           r;
		if (ubfs_dmu_read(os, dirobj, HDR + i * UBFS_DIRENT_SIZE, &e, sizeof(e)) < 0)
			return -1;
		if (e.obj == 0)
			continue;
		r = cb(arg, e.name, e.obj, e.type);
		if (r != 0)
			return r;
	}
	return 0;
}

int ubfs_dir_count(ubfs_dmu_os_t *os, uint64_t dirobj, uint64_t *count_out)
{
	uint64_t count = 0, i, live = 0;

	if (ubfs_dmu_read(os, dirobj, 0, &count, sizeof(count)) < 0)
		return -1;
	for (i = 0; i < count; i++)
	{
		ubfs_dirent_t e;
		if (ubfs_dmu_read(os, dirobj, HDR + i * UBFS_DIRENT_SIZE, &e, sizeof(e)) < 0)
			return -1;
		if (e.obj != 0)
			live++;
	}
	*count_out = live;
	return 0;
}
