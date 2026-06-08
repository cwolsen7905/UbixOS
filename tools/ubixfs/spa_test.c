/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * spa_test — host milestone test for the UbixFS SPA (pool) layer:
 *   format -> commit txgs -> reopen at last txg -> alloc/free persistence ->
 *   corrupt the newest uberblock and confirm the pool falls back to the previous
 *   committed transaction (the CoW crash-recovery spine).
 */
#include <ubfs_spa.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* file-backed vdev */
struct fdev
{
	int fd;
};

static int fdev_read(void *ctx, uint64_t blk, void *buf)
{
	struct fdev *d = (struct fdev *)ctx;
	return pread(d->fd, buf, UBFS_BLOCK_SIZE, (off_t)blk * UBFS_BLOCK_SIZE) == UBFS_BLOCK_SIZE ? 0 : -1;
}

static int fdev_write(void *ctx, uint64_t blk, const void *buf)
{
	struct fdev *d = (struct fdev *)ctx;
	return pwrite(d->fd, buf, UBFS_BLOCK_SIZE, (off_t)blk * UBFS_BLOCK_SIZE) == UBFS_BLOCK_SIZE ? 0 : -1;
}

static int fails;
#define CHECK(cond, msg)                                                                                               \
	do {                                                                                                           \
		if (cond) {                                                                                            \
			printf("  ok   : %s\n", msg);                                                                   \
		} else {                                                                                               \
			printf("  FAIL : %s\n", msg);                                                                   \
			fails++;                                                                                       \
		}                                                                                                      \
	} while (0)

int main(void)
{
	const char     *path = "/tmp/ubfs_pool.img";
	const uint64_t  nblocks = 16384; /* 64 MB image */
	struct fdev     d;
	ubfs_vdev_io_t   io;
	ubfs_pool_t    *p;
	uint64_t        b1, b2, b3, free_after_alloc;

	d.fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (d.fd < 0 || ftruncate(d.fd, (off_t)nblocks * UBFS_BLOCK_SIZE) < 0) {
		perror("image");
		return 2;
	}
	io.ctx = &d;
	io.read = fdev_read;
	io.write = fdev_write;
	io.size_blocks = nblocks;

	printf("== format ==\n");
	CHECK(ubfs_pool_format(&io, "tank", 0xABCDEF, &p) == 0, "format pool");
	CHECK(ubfs_pool_txg(p) == 1, "fresh pool is at txg 1");

	printf("== commit a few empty txgs ==\n");
	CHECK(ubfs_pool_commit(p, NULL) == 0, "commit -> txg 2");
	CHECK(ubfs_pool_commit(p, NULL) == 0, "commit -> txg 3");
	CHECK(ubfs_pool_txg(p) == 3, "pool is at txg 3");

	printf("== allocate + free, then commit ==\n");
	b1 = ubfs_alloc_block(p);
	b2 = ubfs_alloc_block(p);
	b3 = ubfs_alloc_block(p);
	CHECK(b1 && b2 && b3 && b1 != b2 && b2 != b3, "three distinct blocks allocated");
	ubfs_free_block(p, b2);
	free_after_alloc = ubfs_pool_free_blocks(p);
	CHECK(ubfs_pool_commit(p, NULL) == 0, "commit -> txg 4 (persist allocations)");
	ubfs_pool_close(p);

	printf("== reopen: lands on last committed txg + persisted allocations ==\n");
	CHECK(ubfs_pool_open(&io, &p) == 0, "reopen pool");
	CHECK(ubfs_pool_txg(p) == 4, "reopened at txg 4");
	CHECK(ubfs_pool_free_blocks(p) == free_after_alloc, "free-block count persisted");
	/* b2 was freed, so it should be the next allocation (hint walks back to it). */
	CHECK(ubfs_alloc_block(p) == b2, "freed block is reused");
	ubfs_pool_close(p);

	printf("== corrupt the NEWEST uberblock, confirm fallback to previous txg ==\n");
	/* Newest committed txg here is 4 -> ring slot 4 -> byte BLOCK_SIZE + 4*1024. */
	{
		uint64_t byte = (uint64_t)UBFS_BLOCK_SIZE + 4ull * UBFS_UBERBLOCK_SIZE;
		char     garbage[UBFS_UBERBLOCK_SIZE];
		memset(garbage, 0xA5, sizeof(garbage));
		if (pwrite(d.fd, garbage, sizeof(garbage), (off_t)byte) != sizeof(garbage)) {
			perror("corrupt");
			return 2;
		}
	}
	CHECK(ubfs_pool_open(&io, &p) == 0, "reopen after corruption");
	CHECK(ubfs_pool_txg(p) == 3, "fell back to txg 3 (previous valid uberblock)");
	ubfs_pool_close(p);

	close(d.fd);
	printf("\n%s\n", fails ? "SPA TEST: FAIL" : "SPA TEST: PASS");
	return fails ? 1 : 0;
}
