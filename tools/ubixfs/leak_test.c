/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * leak_test — host regression for UbixFS space reclamation.  Two leaks used to
 * bleed a pool dry over normal use:
 *   1. ubfs_fs_unlink dropped the directory entry but never freed the object's
 *      dnode + data blocks (no dmu_object_free).  Every delete — and every
 *      overwrite, which the tools do as unlink+create — leaked the whole file.
 *   2. apply_deferred_frees cleared freed bits only in the in-memory bitmap;
 *      they reached disk on the NEXT commit's flush, so the last txg before
 *      ubfs_pool_close (i.e. every host-tool invocation, every unmount) leaked
 *      its frees.
 *
 * The test runs many create+write+unlink cycles, each in its own open/close, on
 * a deliberately small pool.  Free space is read on each fresh reopen (recomputed
 * from the on-disk bitmap), so both leaks show up as a monotonic decline — and,
 * because the pool is far smaller than the total bytes written, as an outright
 * allocation failure.  With the fixes, free space returns to baseline every
 * cycle and all iterations succeed.
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
	uint64_t baseline = 0;

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
	CHECK(ubfs_dsl_sync_dataset(&dsl, fs_ds, &os) == 0, "sync dataset");
	CHECK(ubfs_dsl_sync(&dsl) == 0, "commit");
	ubfs_pool_close(p);

	printf("== %d create+write(%dK)+unlink cycles on an %lluMB pool ==\n",
	       ITERS,
	       FILEBYTES / 1024,
	       POOLBYTES / (1024 * 1024));

	for (int it = 0; it < ITERS; it++)
	{
		uint64_t freeblk;

		/* Fresh open — free-space count is recomputed from the on-disk bitmap,
		 * so any block that leaked to disk on the previous close shows here. */
		CHECK(ubfs_pool_open(&io, &p) == 0, "reopen pool");
		CHECK(ubfs_dsl_open(p, &dsl) == 0, "open MOS");
		CHECK(ubfs_dsl_open_dataset(&dsl, fs_ds, &dp, &os) == 0, "open dataset");
		ubfs_fs_init(&fs, &os, (uint64_t)(it + 2));
		freeblk = ubfs_pool_free_blocks(p);
		/* Baseline at iteration 2 — after the one-time warm-up (the root directory
		 * growing its first data block to its high-water mark, a bounded cost, not
		 * a leak).  From here free space must be perfectly flat. */
		if (it == 2)
			baseline = freeblk;

		CHECK(ubfs_fs_create(&fs, "/f", 0644, 0, 0, &o) == 0, "create /f");
		CHECK(ubfs_fs_write(&fs, o, 0, buf, FILEBYTES) == 0, "write /f");
		uint64_t after_write = ubfs_pool_free_blocks(p);
		CHECK(ubfs_fs_unlink(&fs, "/f") == 0, "unlink /f");
		uint64_t after_unlink = ubfs_pool_free_blocks(p);
		CHECK(ubfs_dsl_sync_dataset(&dsl, fs_ds, &os) == 0, "sync dataset");
		CHECK(ubfs_dsl_sync(&dsl) == 0, "commit");
		uint64_t after_commit = ubfs_pool_free_blocks(p);
		ubfs_pool_close(p);

		if (it < 4 || it == ITERS - 1)
			printf(
			    "  iter %2d: open=%llu  afterWrite=%llu  afterUnlink=%llu  afterCommit=%llu (base %llu)\n",
			    it,
			    freeblk,
			    after_write,
			    after_unlink,
			    after_commit,
			    baseline);
		/* The whole point: once warmed up, free space must not drift down. */
		if (it >= 2)
			CHECK(freeblk >= baseline, "free space did not leak vs steady-state baseline");
	}

	printf(fails ? "\nLEAK TEST: %d FAILURE(S)\n" : "\nLEAK TEST: all clear (no space leaked)\n", fails);
	free(buf);
	close(d.fd);
	return fails ? 1 : 0;
}
