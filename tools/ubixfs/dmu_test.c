/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * dmu_test — host milestone for the UbixFS DMU (CoW object engine):
 *   create objset -> alloc object -> write a multi-block file (forces indirect-
 *   tree growth) -> read back -> CoW-overwrite the middle -> sparse hole ->
 *   sync objset + commit pool -> reopen -> data intact at the committed state.
 */
#include <ubfs_dmu.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct fdev
{
	int fd;
};
static int fdev_read(void *ctx, uint64_t blk, void *buf)
{
	return pread(((struct fdev *)ctx)->fd, buf, UBFS_BLOCK_SIZE, (off_t)blk * UBFS_BLOCK_SIZE) == UBFS_BLOCK_SIZE
	           ? 0
	           : -1;
}
static int fdev_write(void *ctx, uint64_t blk, const void *buf)
{
	return pwrite(((struct fdev *)ctx)->fd, buf, UBFS_BLOCK_SIZE, (off_t)blk * UBFS_BLOCK_SIZE) == UBFS_BLOCK_SIZE
	           ? 0
	           : -1;
}

static int fails;
#define CHECK(c, m)                                                                                                  \
	do {                                                                                                           \
		printf("  %s : %s\n", (c) ? "ok  " : "FAIL", m);                                                        \
		if (!(c))                                                                                                \
			fails++;                                                                                       \
	} while (0)

/* deterministic byte pattern so we can verify large buffers */
static void fill(uint8_t *b, size_t n, uint8_t seed)
{
	for (size_t i = 0; i < n; i++)
		b[i] = (uint8_t)(seed + i * 31u);
}
static int patcmp(const uint8_t *b, size_t n, uint8_t seed)
{
	for (size_t i = 0; i < n; i++)
		if (b[i] != (uint8_t)(seed + i * 31u))
			return 0;
	return 1;
}

#define FILESZ 200000 /* ~49 blocks -> forces tree growth to 3 levels */

int main(void)
{
	const char    *path = "/tmp/ubfs_dmu.img";
	struct fdev    d;
	ubfs_vdev_io_t io;
	ubfs_pool_t   *p;
	ubfs_dmu_os_t  os;
	ubfs_blkptr_t  osbp;
	uint64_t       obj;
	uint8_t       *w = malloc(FILESZ), *r = malloc(FILESZ);

	d.fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (d.fd < 0 || ftruncate(d.fd, 64ll * 1024 * 1024) < 0) {
		perror("img");
		return 2;
	}
	io.ctx = &d;
	io.read = fdev_read;
	io.write = fdev_write;
	io.size_blocks = 64ull * 1024 * 1024 / UBFS_BLOCK_SIZE;

	CHECK(ubfs_pool_format(&io, "tank", 1, &p) == 0, "format pool");
	ubfs_dmu_objset_create(p, UBFS_OST_FS, &os);
	obj = ubfs_dmu_object_alloc(&os, UBFS_OT_PLAIN_FILE, UBFS_BT_NONE);
	CHECK(obj >= 1, "allocate file object");

	printf("== write %d bytes (multi-block, grows indirect tree) ==\n", FILESZ);
	fill(w, FILESZ, 0x11);
	CHECK(ubfs_dmu_write(&os, obj, 0, w, FILESZ) == 0, "write file");
	memset(r, 0, FILESZ);
	CHECK(ubfs_dmu_read(&os, obj, 0, r, FILESZ) == 0, "read file back");
	CHECK(patcmp(r, FILESZ, 0x11), "data matches");

	printf("== CoW overwrite 8 KB in the middle ==\n");
	fill(w + 40960, 8192, 0x77); /* keep our reference buffer in sync */
	CHECK(ubfs_dmu_write(&os, obj, 40960, w + 40960, 8192) == 0, "overwrite middle");
	memset(r, 0, FILESZ);
	ubfs_dmu_read(&os, obj, 0, r, FILESZ);
	CHECK(memcmp(r, w, FILESZ) == 0, "whole file matches after overwrite (CoW)");

	printf("== sparse: write at 4 MB, hole in between reads as zeros ==\n");
	{
		uint8_t hi[16], hole[4096];
		fill(hi, sizeof(hi), 0x55);
		CHECK(ubfs_dmu_write(&os, obj, 4ull * 1024 * 1024, hi, sizeof(hi)) == 0, "write at 4 MB");
		CHECK(ubfs_dmu_read(&os, obj, 2ull * 1024 * 1024, hole, sizeof(hole)) == 0, "read the hole");
		int zero = 1;
		for (size_t i = 0; i < sizeof(hole); i++)
			if (hole[i]) {
				zero = 0;
				break;
			}
		CHECK(zero, "hole reads as zeros (sparse)");
	}

	printf("== sync objset + commit pool, then reopen ==\n");
	CHECK(ubfs_dmu_objset_sync(&os, &osbp) == 0, "sync objset");
	CHECK(ubfs_pool_commit(p, &osbp) == 0, "commit pool with objset root");
	ubfs_pool_close(p);

	CHECK(ubfs_pool_open(&io, &p) == 0, "reopen pool");
	CHECK(ubfs_dmu_objset_open(p, &ubfs_pool_uberblock(p)->rootbp, &os) == 0, "reopen objset from uberblock");
	memset(r, 0, FILESZ);
	CHECK(ubfs_dmu_read(&os, obj, 0, r, FILESZ) == 0, "re-read file after reopen");
	CHECK(memcmp(r, w, FILESZ) == 0, "data intact across reopen (CoW persisted)");
	ubfs_pool_close(p);

	free(w);
	free(r);
	close(d.fd);
	printf("\n%s\n", fails ? "DMU TEST: FAIL" : "DMU TEST: PASS");
	return fails ? 1 : 0;
}
