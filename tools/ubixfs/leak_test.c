/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * leak_test — host regression for UbixFS space reclamation.  Four leaks used to
 * bleed a pool dry over normal use:
 *   1. ubfs_fs_unlink dropped the directory entry but never freed the object's
 *      dnode + data blocks (no dmu_object_free).  Every delete — and every
 *      overwrite, which the tools do as unlink+create — leaked the whole file.
 *   2. ubfs_dmu_objset_sync re-serialised each objset into a fresh block every
 *      commit without freeing the previous header — two blocks per commit.
 *   3. Object numbers were never reused, so create/delete churn grew the
 *      metadnode without bound (now recycled via an in-memory free list).
 *   4. apply_deferred_frees cleared freed bits only in the in-memory bitmap;
 *      they reached disk on the NEXT commit's flush, so the last txg before
 *      ubfs_pool_close (every unmount) leaked its frees (now settled on close).
 *
 * Phase 1 runs many create+write+unlink cycles in ONE mount session (the model
 * for the kernel, which mounts once and reuses freed slots in-memory): after a
 * short warm-up, free space must be perfectly flat — catching #1, #2 and #3.
 * Phase 2 closes and reopens, recomputing free space from the on-disk bitmap, to
 * confirm the last txg's frees became durable rather than leaking — catching #4.
 */
#include <ubfs_fs.h>
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
#define CHECK(c, m)                                                                                                    \
	do                                                                                                             \
	{                                                                                                              \
		int _ok = (c);                                                                                         \
		if (!_ok)                                                                                              \
		{                                                                                                      \
			printf("  FAIL : %s\n", m);                                                                    \
			fails++;                                                                                       \
		}                                                                                                      \
	} while (0)

#define FILEBYTES (256 * 1024) /* 64 blocks of data per file */
#define ITERS 40               /* far more than an 8 MB pool could hold if leaking */
#define POOLBYTES (8ull * 1024 * 1024)

int main(void)
{
	const char *path = "/tmp/ubfs_leak.img";
	struct fdev d;
	ubfs_vdev_io_t io;
	ubfs_pool_t *p;
	ubfs_dsl_t dsl;
	ubfs_dmu_os_t os;
	ubfs_dataset_phys_t dp;
	ubfs_fs_t fs;
	uint64_t fs_ds, o;
	uint8_t *buf = malloc(FILEBYTES);
	uint64_t baseline = 0, steady = 0;

	if (!buf)
		return 2;
	for (int i = 0; i < FILEBYTES; i++)
		buf[i] = (uint8_t)(i * 5 + 1);

	d.fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (d.fd < 0 || ftruncate(d.fd, (off_t)POOLBYTES) < 0)
	{
		perror("img");
		return 2;
	}
	io.ctx = &d;
	io.read = fdr;
	io.write = fdw;
	io.size_blocks = POOLBYTES / UBFS_BLOCK_SIZE;

	CHECK(ubfs_pool_format(&io, "tank", 1, &p) == 0, "format pool");
	CHECK(ubfs_dsl_create(p, &dsl) == 0, "create MOS");
	CHECK(ubfs_dsl_create_fs(&dsl, "root", &fs_ds) == 0, "create fs dataset");
	CHECK(ubfs_dsl_open_dataset(&dsl, fs_ds, &dp, &os) == 0, "open fs dataset");
	ubfs_fs_init(&fs, &os, 1);
	CHECK(ubfs_fs_mkroot(&fs, 0, 0) == 0, "mkroot");

	printf("== Phase 1: %d create+write(%dK)+unlink cycles in one mount session ==\n", ITERS, FILEBYTES / 1024);
	for (int it = 0; it < ITERS; it++)
	{
		uint64_t freeblk;

		CHECK(ubfs_fs_create(&fs, "/f", 0644, 0, 0, &o) == 0, "create /f");
		CHECK(ubfs_fs_write(&fs, o, 0, buf, FILEBYTES) == 0, "write /f");
		CHECK(ubfs_fs_unlink(&fs, "/f") == 0, "unlink /f");
		CHECK(ubfs_dsl_sync_dataset(&dsl, fs_ds, &os) == 0, "sync dataset");
		CHECK(ubfs_dsl_sync(&dsl) == 0, "commit");
		freeblk = ubfs_pool_free_blocks(p);

		/* Baseline at iteration 2 — after the one-time warm-up (the root directory
		 * growing its first data block to its high-water mark, a bounded cost, not
		 * a leak).  From here free space must be perfectly flat. */
		if (it == 2)
			baseline = freeblk;
		if (it < 3 || it == ITERS - 1)
			printf("  iter %2d: free after commit = %llu blocks (baseline %llu)\n", it, freeblk, baseline);
		if (it >= 2)
			CHECK(freeblk >= baseline, "free space did not leak vs steady-state baseline");
	}
	steady = ubfs_pool_free_blocks(p);
	ubfs_pool_close(p);

	printf("== Phase 2: reopen — the last txg's frees must be durable, not leaked ==\n");
	CHECK(ubfs_pool_open(&io, &p) == 0, "reopen pool");
	printf("  reopened free = %llu blocks (in-session steady %llu)\n", ubfs_pool_free_blocks(p), steady);
	CHECK(ubfs_pool_free_blocks(p) >= steady, "close settled the last txg's frees (no leak on unmount)");
	ubfs_pool_close(p);

	printf(fails ? "\nLEAK TEST: %d FAILURE(S)\n" : "\nLEAK TEST: all clear (no space leaked)\n", fails);
	free(buf);
	close(d.fd);
	return fails ? 1 : 0;
}
