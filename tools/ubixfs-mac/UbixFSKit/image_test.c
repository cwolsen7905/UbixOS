/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * image_test — host harness exercising the GUI read path end to end against a
 * pool image (raw or MBR-wrapped): open via the vdev shim, enumerate datasets
 * (ubfs_dsl_foreach), list a directory, and read a file out.  This is exactly
 * the sequence the Obj-C facade will perform, so a green run here validates the
 * whole C foundation before any Swift exists.
 *
 *   image_test <image> [dataset] [path] [readfile]
 *
 * Defaults: dataset = first FS dataset, path = "/", readfile = none.
 */
#include "ubfs_vdev_image.h"

#include <ubfs_dsl.h>
#include <ubfs_fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int print_dataset(void *arg, const char *name, uint64_t ds_obj)
{
	(void)arg;
	printf("  dataset: %-16s (mos obj %llu)\n", name, (unsigned long long)ds_obj);
	return 0;
}

/* Remember the first dataset name so we can default to it. */
struct first_ds
{
	char found[64];
};
static int grab_first(void *arg, const char *name, uint64_t ds_obj)
{
	struct first_ds *f = (struct first_ds *)arg;

	(void)ds_obj;
	if (f->found[0] == '\0')
		strncpy(f->found, name, sizeof(f->found) - 1);
	return 0;
}

static int ls_cb(void *arg, const char *name, uint64_t obj, uint8_t type)
{
	ubfs_fs_t *fs = (ubfs_fs_t *)arg;
	ubfs_inode_t in;
	char t = (type == UBFS_DT_DIR) ? 'd' : (type == UBFS_DT_LNK) ? 'l' : '-';

	if (ubfs_fs_getattr(fs, obj, &in) == 0)
		printf("  %c %04llo %8llu  %s\n",
		       t,
		       (unsigned long long)(in.mode & 07777),
		       (unsigned long long)in.size,
		       name);
	else
		printf("  ? ????      %s\n", name);
	return 0;
}

int main(int argc, char **argv)
{
	ubfs_image_t img;
	ubfs_pool_t *pool;
	ubfs_dsl_t dsl;
	ubfs_dataset_phys_t dp;
	ubfs_dmu_os_t os;
	ubfs_fs_t fs;
	uint64_t ds_obj, obj;
	const char *path;
	const char *dsname;
	struct first_ds first = {{0}};

	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <image> [dataset] [path] [readfile]\n", argv[0]);
		return 2;
	}

	if (ubfs_image_open(&img, argv[1], 0) < 0)
	{
		fprintf(stderr, "%s: no UbixFS pool found\n", argv[1]);
		return 1;
	}
	printf("opened %s: pool base=%llu span=%llu bytes\n",
	       argv[1],
	       (unsigned long long)img.base,
	       (unsigned long long)img.span_bytes);

	if (ubfs_pool_open(&img.io, &pool) < 0 || ubfs_dsl_open(pool, &dsl) < 0)
	{
		fprintf(stderr, "pool open failed\n");
		return 1;
	}

	printf("datasets:\n");
	ubfs_dsl_foreach(&dsl, print_dataset, NULL);

	/* Pick the dataset: arg, else the first one enumerated. */
	if (argc >= 3)
		dsname = argv[2];
	else
	{
		ubfs_dsl_foreach(&dsl, grab_first, &first);
		dsname = first.found;
	}
	if (dsname[0] == '\0' || ubfs_dsl_lookup(&dsl, dsname, &ds_obj) < 0 ||
	    ubfs_dsl_open_dataset(&dsl, ds_obj, &dp, &os) < 0)
	{
		fprintf(stderr, "dataset '%s' not found\n", dsname);
		return 1;
	}
	ubfs_fs_init(&fs, &os, (uint64_t)time(NULL));

	path = (argc >= 4) ? argv[3] : "/";
	if (ubfs_fs_lookup(&fs, path, &obj) < 0)
	{
		fprintf(stderr, "%s: no such path in dataset '%s'\n", path, dsname);
		return 1;
	}
	printf("listing %s in dataset '%s':\n", path, dsname);
	ubfs_fs_readdir(&fs, obj, ls_cb, &fs);

	/* Optional: read a file out to stdout's sibling (prove copy-out works). */
	if (argc >= 5)
	{
		uint64_t fobj;
		ubfs_inode_t in;
		char buf[4096];
		uint64_t off = 0;

		if (ubfs_fs_lookup(&fs, argv[4], &fobj) < 0 || ubfs_fs_getattr(&fs, fobj, &in) < 0)
		{
			fprintf(stderr, "%s: cannot read\n", argv[4]);
			return 1;
		}
		printf("---- %s (%llu bytes) ----\n", argv[4], (unsigned long long)in.size);
		while (off < in.size)
		{
			uint64_t want = in.size - off;
			int got;

			if (want > sizeof(buf))
				want = sizeof(buf);
			got = ubfs_fs_read(&fs, fobj, off, buf, want);
			if (got <= 0)
				break;
			fwrite(buf, 1, (size_t)got, stdout);
			off += (uint64_t)got;
		}
		printf("\n---- end (%llu bytes read) ----\n", (unsigned long long)off);
	}

	ubfs_pool_close(pool);
	ubfs_image_close(&img);
	return 0;
}
