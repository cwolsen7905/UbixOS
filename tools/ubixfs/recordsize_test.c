/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * recordsize_test — host milestone for variable recordsize (large blocks):
 *   set a file object's datablksz to 64 KiB / 1 MiB, write multi-record data,
 *   read back, CoW-overwrite within a record, persist across reopen, and prove a
 *   small file in a 1 MiB-recordsize object does NOT waste a whole record (tail
 *   allocation is ceil(size/4K), not the full record).
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
#define CHECK(c, m)                                                                                                    \
	do                                                                                                             \
	{                                                                                                              \
		printf("  %s : %s\n", (c) ? "ok  " : "FAIL", m);                                                       \
		if (!(c))                                                                                              \
			fails++;                                                                                       \
	} while (0)

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

/* Allocate a file object whose data block size (recordsize) is `rsz`. */
static uint64_t alloc_file(ubfs_dmu_os_t *os, uint32_t rsz)
{
	uint64_t obj = ubfs_dmu_object_alloc(os, UBFS_OT_PLAIN_FILE, UBFS_BT_NONE);
	ubfs_dnode_t dn;

	if (obj == 0 || ubfs_dmu_dnode_get(os, obj, &dn) < 0)
		return 0;
	dn.datablksz = rsz;
	if (ubfs_dmu_dnode_put(os, obj, &dn) < 0)
		return 0;
	return obj;
}

#define FILESZ 200000 /* ~3 x 64 KiB records + a partial */

int main(void)
{
	const char *path = "/tmp/ubfs_recordsize.img";
	struct fdev d;
	ubfs_vdev_io_t io;
	ubfs_pool_t *p;
	ubfs_dmu_os_t os;
	ubfs_blkptr_t osbp;
	uint64_t obj, big, small;
	uint8_t *w = malloc(FILESZ), *r = malloc(FILESZ);

	d.fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (d.fd < 0 || ftruncate(d.fd, 64ll * 1024 * 1024) < 0)
	{
		perror("img");
		return 2;
	}
	io.ctx = &d;
	io.read = fdev_read;
	io.write = fdev_write;
	io.size_blocks = 64ull * 1024 * 1024 / UBFS_BLOCK_SIZE;

	CHECK(ubfs_pool_format(&io, "tank", 1, &p) == 0, "format pool");
	ubfs_dmu_objset_create(p, UBFS_OST_FS, &os);

	printf("== 64 KiB recordsize: multi-record write/read ==\n");
	obj = alloc_file(&os, 64u * 1024);
	CHECK(obj >= 1, "allocate 64K-recordsize file");
	fill(w, FILESZ, 0x11);
	CHECK(ubfs_dmu_write(&os, obj, 0, w, FILESZ) == 0, "write 200000 bytes");
	memset(r, 0, FILESZ);
	CHECK(ubfs_dmu_read(&os, obj, 0, r, FILESZ) == 0, "read back");
	CHECK(patcmp(r, FILESZ, 0x11), "data matches");

	printf("== CoW overwrite 10 KB straddling a record boundary ==\n");
	fill(w + 60000, 10240, 0x99);
	CHECK(ubfs_dmu_write(&os, obj, 60000, w + 60000, 10240) == 0, "overwrite straddling 64K boundary");
	memset(r, 0, FILESZ);
	ubfs_dmu_read(&os, obj, 0, r, FILESZ);
	CHECK(memcmp(r, w, FILESZ) == 0, "whole file matches after CoW overwrite");

	printf("== 1 MiB recordsize: large contiguous run (256 blocks) ==\n");
	big = alloc_file(&os, 1024u * 1024);
	CHECK(big >= 1, "allocate 1M-recordsize file");
	{
		uint8_t *bw = malloc(1536u * 1024), *br = malloc(1536u * 1024);
		fill(bw, 1536u * 1024, 0x42);
		CHECK(ubfs_dmu_write(&os, big, 0, bw, 1536u * 1024) == 0, "write 1.5 MiB (1 full + 1 partial record)");
		memset(br, 0, 1536u * 1024);
		CHECK(ubfs_dmu_read(&os, big, 0, br, 1536u * 1024) == 0, "read 1.5 MiB back");
		CHECK(memcmp(bw, br, 1536u * 1024) == 0, "1.5 MiB matches");
		free(bw);
		free(br);
	}

	printf("== small file in a 1 MiB-recordsize object wastes no tail ==\n");
	{
		uint64_t before, after, delta;
		uint8_t s[5000], sr[5000];

		small = alloc_file(&os, 1024u * 1024);
		CHECK(small >= 1, "allocate 1M-recordsize file (small payload)");
		before = ubfs_pool_free_blocks(p);
		fill(s, sizeof(s), 0x33);
		CHECK(ubfs_dmu_write(&os, small, 0, s, sizeof(s)) == 0, "write 5000 bytes");
		after = ubfs_pool_free_blocks(p);
		delta = before - after; /* blocks consumed: data + any metadata */
		printf("     consumed %llu blocks for a 5000-byte file (record=1 MiB)\n", (unsigned long long)delta);
		CHECK(delta < 16, "no 1 MiB tail waste (a few blocks, not 256)");
		memset(sr, 0, sizeof(sr));
		ubfs_dmu_read(&os, small, 0, sr, sizeof(sr));
		CHECK(patcmp(sr, sizeof(sr), 0x33), "small file reads back");
	}

	printf("== sync + commit + reopen, data intact ==\n");
	CHECK(ubfs_dmu_objset_sync(&os, &osbp) == 0, "sync objset");
	CHECK(ubfs_pool_commit(p, &osbp) == 0, "commit pool");
	ubfs_pool_close(p);
	CHECK(ubfs_pool_open(&io, &p) == 0, "reopen pool");
	CHECK(ubfs_dmu_objset_open(p, &ubfs_pool_uberblock(p)->rootbp, &os) == 0, "reopen objset");
	memset(r, 0, FILESZ);
	CHECK(ubfs_dmu_read(&os, obj, 0, r, FILESZ) == 0, "re-read 64K-recordsize file");
	CHECK(memcmp(r, w, FILESZ) == 0, "64K file intact across reopen");

	printf("== feature_incompat gate ==\n");
	CHECK((ubfs_pool_config(p)->feature_incompat & UBFS_FEAT_INCOMPAT_LARGE_BLOCKS) != 0,
	      "large-blocks feature bit persisted (we created >4K files)");
	CHECK(ubfs_pool_set_incompat(p, 0x8000ULL) == 0, "mark an unknown incompat bit");
	ubfs_pool_close(p);
	{
		ubfs_pool_t *p2 = NULL;
		CHECK(ubfs_pool_open(&io, &p2) != 0, "reopen refused while an unknown incompat bit is set");
	}

	free(w);
	free(r);
	close(d.fd);
	printf("\n%s\n", fails ? "RECORDSIZE TEST: FAIL" : "RECORDSIZE TEST: PASS");
	return fails ? 1 : 0;
}
