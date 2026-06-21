/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * ubfs_vdev_image — host file-backed vdev with MBR-partition awareness.
 * See ubfs_vdev_image.h.
 */
#include "ubfs_vdev_image.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Classic MBR layout (mirrors sys/dev/partition.c): 4 × 16-byte records at
 * 0x1BE, then the 0x55 0xAA signature.  Per record: type at +4, LBA start
 * (LE u32) at +8, sector count at +12.  Sectors are 512 bytes. */
#define MBR_SECTOR_SIZE 512
#define MBR_PART_TABLE_OFF 0x1BE
#define MBR_RECORD_SZ 16
#define MBR_NRECORDS 4
#define MBR_SIG_OFF 0x1FE
#define MBR_TYPE_UBPOOL 0x9C /* UbixFS pool partition (see sys/include/dev/partition.h) */

/* ── block I/O over the image, offset to the pool's base ────────────────────*/
static int img_read(void *ctx, uint64_t blk, void *buf)
{
	ubfs_image_t *im = (ubfs_image_t *)ctx;
	off_t off = (off_t)(im->base + blk * UBFS_BLOCK_SIZE);

	return pread(im->fd, buf, UBFS_BLOCK_SIZE, off) == (ssize_t)UBFS_BLOCK_SIZE ? 0 : -1;
}

static int img_write(void *ctx, uint64_t blk, const void *buf)
{
	ubfs_image_t *im = (ubfs_image_t *)ctx;
	off_t off = (off_t)(im->base + blk * UBFS_BLOCK_SIZE);

	return pwrite(im->fd, buf, UBFS_BLOCK_SIZE, off) == (ssize_t)UBFS_BLOCK_SIZE ? 0 : -1;
}

/* ── detection helpers ──────────────────────────────────────────────────────*/

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/** True if a valid UbixFS pool label (its config magic) sits at `byte_off`. */
static int has_pool_label(int fd, uint64_t byte_off)
{
	uint64_t magic = 0;

	if (pread(fd, &magic, sizeof(magic), (off_t)byte_off) != (ssize_t)sizeof(magic))
		return 0;
	/* On-disk magic is little-endian; the only hosts here (arm64/x86_64) are LE. */
	return magic == UBFS_MAGIC;
}

/**
 * Find the pool base byte offset within the image:
 *   1. raw pool — label at byte 0;
 *   2. MBR disk image — the type-0x9C partition whose label validates;
 *   3. fallback — any MBR partition whose label validates.
 * On success sets *base and *span (usable bytes from base) and returns 0;
 * returns -1 if no pool is found.
 */
static int locate_pool(int fd, uint64_t file_size, uint64_t *base, uint64_t *span)
{
	uint8_t mbr[MBR_SECTOR_SIZE];

	/* 1. raw pool image. */
	if (has_pool_label(fd, 0))
	{
		*base = 0;
		*span = file_size;
		return 0;
	}

	/* Need an MBR to go further. */
	if (pread(fd, mbr, sizeof(mbr), 0) != (ssize_t)sizeof(mbr))
		return -1;
	if (mbr[MBR_SIG_OFF] != 0x55 || mbr[MBR_SIG_OFF + 1] != 0xAA)
		return -1;

	/* 2. preferred: the UbixFS-pool-typed partition. */
	for (int i = 0; i < MBR_NRECORDS; i++)
	{
		const uint8_t *rec = mbr + MBR_PART_TABLE_OFF + (i * MBR_RECORD_SZ);
		uint32_t lba = le32(rec + 8);
		uint64_t off;

		if (lba == 0 || rec[4] != MBR_TYPE_UBPOOL)
			continue;
		off = (uint64_t)lba * MBR_SECTOR_SIZE;
		if (has_pool_label(fd, off))
		{
			*base = off;
			*span = (uint64_t)le32(rec + 12) * MBR_SECTOR_SIZE;
			return 0;
		}
	}

	/* 3. fallback: any partition that actually carries a pool label. */
	for (int i = 0; i < MBR_NRECORDS; i++)
	{
		const uint8_t *rec = mbr + MBR_PART_TABLE_OFF + (i * MBR_RECORD_SZ);
		uint32_t lba = le32(rec + 8);
		uint64_t off;

		if (lba == 0)
			continue;
		off = (uint64_t)lba * MBR_SECTOR_SIZE;
		if (has_pool_label(fd, off))
		{
			*base = off;
			*span = (uint64_t)le32(rec + 12) * MBR_SECTOR_SIZE;
			return 0;
		}
	}

	return -1;
}

/* ── public API ─────────────────────────────────────────────────────────────*/

int ubfs_image_open(ubfs_image_t *img, const char *path, int rw)
{
	struct stat st;
	uint64_t base = 0, span = 0;

	memset(img, 0, sizeof(*img));
	img->fd = open(path, rw ? O_RDWR : O_RDONLY);
	if (img->fd < 0)
		return -1;
	if (fstat(img->fd, &st) < 0)
	{
		close(img->fd);
		img->fd = -1;
		return -1;
	}

	if (locate_pool(img->fd, (uint64_t)st.st_size, &base, &span) < 0)
	{
		close(img->fd);
		img->fd = -1;
		return -2;
	}

	img->base = base;
	img->span_bytes = span;
	img->rw = rw;
	img->io.ctx = img;
	img->io.read = img_read;
	img->io.write = img_write;
	img->io.size_blocks = span / UBFS_BLOCK_SIZE;
	return 0;
}

void ubfs_image_close(ubfs_image_t *img)
{
	if (img->fd >= 0)
		close(img->fd);
	img->fd = -1;
}
