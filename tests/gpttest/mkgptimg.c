/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * mkgptimg — HOST tool: convert a uBixOS MBR disk image (FAT boot + swap +
 * UbixFS pool, as tools/mkimage.sh builds) in place into a GPT disk using
 * libgpt.  Used to produce the "kernel boots from GPT" test image from a copy
 * of a normal image; never run it on a disk you care about.
 *
 *   cc -Igpth -o mkgptimg mkgptimg.c ../../lib/libgpt/gpt.c   (gpth/ubix/diskgpt.h)
 *   mkgptimg image.img
 *
 * The image is extended by 1 MiB to make room for the backup GPT.  Partition
 * ranges and order are preserved; types become the EFI System / uBix swap /
 * uBix pool GUIDs by MBR type byte (0x0C/0x82/0x9C).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ubix/diskgpt.h>

/**
 * Read sectors from the image file.
 */
static int fd_read(void *ctx, uint64_t lba, uint32_t count, void *buf)
{
	return pread(*(int *)ctx, buf, (size_t)count * 512, (off_t)(lba * 512)) == (ssize_t)(count * 512) ? 0 : -1;
}

/**
 * Write sectors to the image file.
 */
static int fd_write(void *ctx, uint64_t lba, uint32_t count, const void *buf)
{
	return pwrite(*(int *)ctx, buf, (size_t)count * 512, (off_t)(lba * 512)) == (ssize_t)(count * 512) ? 0 : -1;
}

/**
 * Load a little-endian 32-bit value.
 */
static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * Convert the MBR image named by argv[1] to GPT; @return 0 on success.
 */
int main(int argc, char **argv)
{
	static struct gpt_table t, back;
	struct gpt_io io;
	uint8_t mbr[512], guid[16];
	struct stat st;
	uint64_t sectors;
	int fd, i, slot = 0;

	if (argc != 2)
	{
		fprintf(stderr, "usage: mkgptimg image\n");
		return 2;
	}
	fd = open(argv[1], O_RDWR);
	if (fd < 0 || fstat(fd, &st) != 0)
	{
		perror(argv[1]);
		return 1;
	}
	io.read = fd_read;
	io.write = fd_write;
	io.ctx = &fd;

	if (fd_read(&fd, 0, 1, mbr) != 0 || mbr[510] != 0x55 || mbr[511] != 0xAA)
	{
		fprintf(stderr, "mkgptimg: no MBR in %s\n", argv[1]);
		return 1;
	}
	if (ftruncate(fd, st.st_size + 1024 * 1024) != 0)
	{
		perror("ftruncate");
		return 1;
	}
	sectors = (uint64_t)(st.st_size + 1024 * 1024) / 512;

	srandom(0x75426958); /* deterministic ids: reproducible images */
	for (i = 0; i < 16; i++)
		guid[i] = (uint8_t)random();
	if (gpt_init(&t, sectors, guid) != 0)
		return 1;

	for (i = 0; i < 4; i++)
	{
		const uint8_t *r = mbr + 446 + i * 16;
		uint8_t type = r[4];
		uint32_t start = le32(r + 8), len = le32(r + 12);
		struct gpt_entry *e = &t.entries[slot];
		int k;

		if (type == 0 || len == 0)
			continue;
		memcpy(e->type,
		       type == 0x82   ? g_gpt_type_ubix_swap
		       : type == 0x9C ? g_gpt_type_ubix_pool
		                      : g_gpt_type_efi,
		       16);
		for (k = 0; k < 16; k++)
			e->guid[k] = (uint8_t)random();
		e->first_lba = start;
		e->last_lba = (uint64_t)start + len - 1;
		printf("slot %d: mbr type 0x%02x lba %u..%llu\n", slot, type, start, (unsigned long long)e->last_lba);
		slot++;
	}

	if (gpt_write(&io, &t) != 0 || gpt_load(&io, sectors, &back, NULL) != 0 || gpt_check_protective_mbr(&io) != 0)
	{
		fprintf(stderr, "mkgptimg: write/validate failed (partition inside the first 34 sectors?)\n");
		return 1;
	}
	printf("mkgptimg: %s is now GPT, %d partitions, %llu sectors\n", argv[1], slot, (unsigned long long)sectors);
	close(fd);
	return 0;
}
