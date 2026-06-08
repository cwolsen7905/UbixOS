/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * dsl_test — host milestone for the UbixFS DSL (dataset layer):
 *   pool -> MOS -> a "root" filesystem dataset + a "swap" volume -> write into
 *   each -> sync datasets + MOS + commit -> reopen -> look up by name -> read
 *   both back.  Exercises the full uberblock->MOS->dataset->objset indirection.
 */
#include <ubfs_dsl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct fdev
{
	int fd;
};
static int fdr(void *c, uint64_t b, void *buf)
{
	return pread(((struct fdev *)c)->fd, buf, UBFS_BLOCK_SIZE, (off_t)b * UBFS_BLOCK_SIZE) == UBFS_BLOCK_SIZE ? 0
	                                                                                                          : -1;
}
static int fdw(void *c, uint64_t b, const void *buf)
{
	return pwrite(((struct fdev *)c)->fd, buf, UBFS_BLOCK_SIZE, (off_t)b * UBFS_BLOCK_SIZE) == UBFS_BLOCK_SIZE ? 0
	                                                                                                           : -1;
}

static int fails;
#define CHECK(c, m)                                                                                                  \
	do {                                                                                                           \
		int _ok = (c); /* evaluate once — the condition may have side effects */                                \
		printf("  %s : %s\n", _ok ? "ok  " : "FAIL", m);                                                        \
		if (!_ok)                                                                                                \
			fails++;                                                                                       \
	} while (0)

int main(void)
{
	const char    *path = "/tmp/ubfs_dsl.img";
	struct fdev    d;
	ubfs_vdev_io_t io;
	ubfs_pool_t   *p;
	ubfs_dsl_t     dsl;
	uint64_t       fs_ds, zv_ds, t;
	const char    *msg = "hello from the root filesystem dataset!";
	uint8_t        volblk[4096], rd[4096];

	d.fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (d.fd < 0 || ftruncate(d.fd, 64ll * 1024 * 1024) < 0) {
		perror("img");
		return 2;
	}
	io.ctx = &d;
	io.read = fdr;
	io.write = fdw;
	io.size_blocks = 64ull * 1024 * 1024 / UBFS_BLOCK_SIZE;

	CHECK(ubfs_pool_format(&io, "tank", 1, &p) == 0, "format pool");
	CHECK(ubfs_dsl_create(p, &dsl) == 0, "create MOS + dataset directory");
	CHECK(ubfs_dsl_create_fs(&dsl, "root", &fs_ds) == 0, "create fs dataset 'root'");
	CHECK(ubfs_dsl_create_volume(&dsl, "swap", 16ull * 1024 * 1024, &zv_ds) == 0, "create volume 'swap' (16 MB)");

	printf("== write into the fs dataset ==\n");
	{
		ubfs_dataset_phys_t dp;
		ubfs_dmu_os_t       fs;
		uint64_t            fobj;
		CHECK(ubfs_dsl_open_dataset(&dsl, fs_ds, &dp, &fs) == 0, "open fs dataset");
		fobj = ubfs_dmu_object_alloc(&fs, UBFS_OT_PLAIN_FILE, UBFS_BT_NONE);
		CHECK(fobj == 1, "fs file object is #1");
		CHECK(ubfs_dmu_write(&fs, fobj, 0, msg, strlen(msg) + 1) == 0, "write file into fs");
		CHECK(ubfs_dsl_sync_dataset(&dsl, fs_ds, &fs) == 0, "sync fs dataset into MOS");
	}

	printf("== write into the volume ==\n");
	{
		ubfs_dataset_phys_t dp;
		ubfs_dmu_os_t       zv;
		CHECK(ubfs_dsl_open_dataset(&dsl, zv_ds, &dp, &zv) == 0, "open volume dataset");
		CHECK(dp.volsize == 16ull * 1024 * 1024, "volume volsize is 16 MB");
		memset(volblk, 0xC3, sizeof(volblk));
		CHECK(ubfs_dmu_write(&zv, UBFS_VOL_DATA_OBJ, 1ull * 1024 * 1024, volblk, sizeof(volblk)) == 0,
		      "write a block to the volume at 1 MB");
		CHECK(ubfs_dsl_sync_dataset(&dsl, zv_ds, &zv) == 0, "sync volume dataset into MOS");
	}

	CHECK(ubfs_dsl_sync(&dsl) == 0, "sync MOS + commit pool");
	ubfs_pool_close(p);

	printf("== reopen and read both datasets back by name ==\n");
	CHECK(ubfs_pool_open(&io, &p) == 0, "reopen pool");
	CHECK(ubfs_dsl_open(p, &dsl) == 0, "open MOS");

	CHECK(ubfs_dsl_lookup(&dsl, "root", &t) == 0 && t == fs_ds, "lookup 'root'");
	CHECK(ubfs_dsl_lookup(&dsl, "swap", &t) == 0 && t == zv_ds, "lookup 'swap'");
	CHECK(ubfs_dsl_lookup(&dsl, "nope", &t) != 0, "lookup of missing dataset fails");

	{
		ubfs_dataset_phys_t dp;
		ubfs_dmu_os_t       fs, zv;
		char                buf[128];
		CHECK(ubfs_dsl_open_dataset(&dsl, fs_ds, &dp, &fs) == 0, "reopen fs dataset");
		memset(buf, 0, sizeof(buf));
		ubfs_dmu_read(&fs, 1, 0, buf, strlen(msg) + 1);
		CHECK(strcmp(buf, msg) == 0, "fs file content intact across reopen");

		CHECK(ubfs_dsl_open_dataset(&dsl, zv_ds, &dp, &zv) == 0, "reopen volume dataset");
		CHECK(dp.volsize == 16ull * 1024 * 1024, "volume volsize persisted");
		memset(rd, 0, sizeof(rd));
		ubfs_dmu_read(&zv, UBFS_VOL_DATA_OBJ, 1ull * 1024 * 1024, rd, sizeof(rd));
		CHECK(rd[0] == 0xC3 && rd[4095] == 0xC3, "volume data intact across reopen");
	}
	ubfs_pool_close(p);

	close(d.fd);
	printf("\n%s\n", fails ? "DSL TEST: FAIL" : "DSL TEST: PASS");
	return fails ? 1 : 0;
}
