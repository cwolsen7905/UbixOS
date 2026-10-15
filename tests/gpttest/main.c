/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * gpttest — writes a GPT, re-reads and validates it, damages the primary and
 * checks the backup is used, then proves the kernel refuses unsafe raw writes.
 *
 * In-OS the table is written into a window of unused sectors at the end of the
 * boot disk via ubix_disk_rawio(), so the live partition table is never
 * touched.  Built with -DGPTTEST_HOST the same checks run over a memory disk.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ubix/diskgpt.h>

#ifndef GPTTEST_HOST
#include <api/ubix_disk.h>
#endif

#define WINDOW_SECTORS 2048ULL

static int g_fail;

/** Record one check. */
static void check(int ok, const char *what)
{
	printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok)
		g_fail = 1;
}

/** A window of the disk: LBAs are offset by base before reaching the device. */
struct window
{
	uint64_t base;
#ifdef GPTTEST_HOST
	uint8_t *mem;
#else
	uint32_t disk;
#endif
};

/**
 * Read @count sectors from the window (chunked to the rawio limit).
 */
static int win_read(void *ctx, uint64_t lba, uint32_t count, void *buf)
{
	struct window *w = (struct window *)ctx;
#ifdef GPTTEST_HOST
	memcpy(buf, w->mem + (w->base + lba) * 512, (size_t)count * 512);
	return 0;
#else
	uint8_t *p = (uint8_t *)buf;

	while (count > 0)
	{
		uint32_t n = count > UBIX_DISK_RAW_MAXSECS ? UBIX_DISK_RAW_MAXSECS : count;

		if (ubix_disk_rawio(w->disk, UBIX_DISK_RAW_READ, w->base + lba, n, p) != 0)
			return -1;
		lba += n;
		p += n * 512;
		count -= n;
	}
	return 0;
#endif
}

/**
 * Write @count sectors into the window (chunked to the rawio limit).
 */
static int win_write(void *ctx, uint64_t lba, uint32_t count, const void *buf)
{
	struct window *w = (struct window *)ctx;
#ifdef GPTTEST_HOST
	memcpy(w->mem + (w->base + lba) * 512, buf, (size_t)count * 512);
	return 0;
#else
	const uint8_t *p = (const uint8_t *)buf;

	while (count > 0)
	{
		uint32_t n = count > UBIX_DISK_RAW_MAXSECS ? UBIX_DISK_RAW_MAXSECS : count;

		if (ubix_disk_rawio(w->disk, UBIX_DISK_RAW_WRITE, w->base + lba, n, (void *)p) != 0)
			return -1;
		lba += n;
		p += n * 512;
		count -= n;
	}
	return 0;
#endif
}

/** Fill the entry with a type, a derived unique GUID, a range and an ASCII name. */
static void set_entry(
    struct gpt_entry *e, const uint8_t type[16], uint8_t id, uint64_t first, uint64_t last, const char *name)
{
	int i;

	memset(e, 0, sizeof(*e));
	memcpy(e->type, type, 16);
	memset(e->guid, id, 16);
	e->first_lba = first;
	e->last_lba = last;
	for (i = 0; name[i] != '\0' && i < 36; i++)
		e->name[i] = (uint16_t)name[i];
}

/** Compare the meaningful content of two tables. */
static int tables_equal(const struct gpt_table *a, const struct gpt_table *b)
{
	return a->total_sectors == b->total_sectors && a->first_usable == b->first_usable &&
	       a->last_usable == b->last_usable && memcmp(a->disk_guid, b->disk_guid, 16) == 0 &&
	       memcmp(a->entries, b->entries, sizeof(a->entries)) == 0;
}

/**
 * The round-trip suite over a window of @total sectors.
 */
static void run_suite(struct window *w, uint64_t total)
{
	static struct gpt_table want, got;
	static const uint8_t efi[16] = GPT_TYPE_EFI_BYTES, swp[16] = GPT_TYPE_UBIX_SWAP_BYTES,
	                     pool[16] = GPT_TYPE_UBIX_POOL_BYTES;
	uint8_t disk_guid[16];
	uint8_t zero[512];
	struct gpt_io io = {win_read, win_write, w};
	int src = 0, rc, i;

	for (i = 0; i < 16; i++)
		disk_guid[i] = (uint8_t)(0xA0 + i);

	check(gpt_crc32(0, "123456789", 9) == 0xCBF43926U, "crc32 check value 0xCBF43926");

	check(gpt_init(&want, total, disk_guid) == 0, "gpt_init");
	set_entry(&want.entries[0], efi, 1, want.first_usable, want.first_usable + 255, "ESP");
	set_entry(&want.entries[1], swp, 2, want.first_usable + 256, want.first_usable + 511, "swap");
	set_entry(&want.entries[2], pool, 3, want.first_usable + 512, want.last_usable, "pool");

	rc = gpt_write(&io, &want);
	check(rc == 0, "gpt_write");
	check(gpt_check_protective_mbr(&io) == 0, "protective MBR valid");

	memset(&got, 0, sizeof(got));
	rc = gpt_load(&io, total, &got, &src);
	check(rc == 0 && src == GPT_FROM_PRIMARY, "re-read validates from primary");
	check(tables_equal(&want, &got), "re-read table equals written table");

	/* Damage the primary header: the backup must take over. */
	memset(zero, 0, sizeof(zero));
	check(win_write(w, 1, 1, zero) == 0, "wipe primary header");
	memset(&got, 0, sizeof(got));
	rc = gpt_load(&io, total, &got, &src);
	check(rc == 0 && src == GPT_FROM_BACKUP, "primary damaged: backup used");
	check(tables_equal(&want, &got), "backup table equals written table");

	/* Flip one byte in the primary entry array: its CRC must reject it. */
	check(gpt_write(&io, &want) == 0, "rewrite GPT");
	{
		uint8_t sec[512];

		check(win_read(w, 2, 1, sec) == 0, "read primary array sector");
		sec[40] ^= 0xFF;
		check(win_write(w, 2, 1, sec) == 0, "corrupt primary array");
	}
	memset(&got, 0, sizeof(got));
	rc = gpt_load(&io, total, &got, &src);
	check(rc == 0 && src == GPT_FROM_BACKUP, "primary array corrupt: backup used");

	/* Overlapping entries are refused before anything is written. */
	set_entry(&want.entries[3], swp, 4, want.first_usable + 100, want.first_usable + 300, "bad");
	check(gpt_write(&io, &want) == GPT_E_OVERLAP, "overlapping entries rejected");
	memset(&want.entries[3], 0, sizeof(want.entries[3]));

	/* Leave the window clean. */
	check(win_write(w, 0, 1, zero) == 0, "clear protective MBR");
	check(win_write(w, 1, 1, zero) == 0, "clear primary header");
	check(win_write(w, total - 1, 1, zero) == 0, "clear backup header");
	check(gpt_load(&io, total, &got, &src) == GPT_E_INVALID, "no GPT after clearing");
}

#ifdef GPTTEST_HOST
/**
 * Run the GPT suite; @return 0 if every check passed.
 */
int main(void)
{
	struct window w;

	w.base = 0;
	w.mem = calloc(WINDOW_SECTORS, 512);
	run_suite(&w, WINDOW_SECTORS);
	printf(g_fail ? "gpttest: FAIL\n" : "gpttest: PASS\n");
	return g_fail;
}
#else
/**
 * Run the GPT suite; @return 0 if every check passed.
 */
int main(void)
{
	static struct ubix_disk_info info[UBIX_DISK_MAX];
	struct window w;
	uint8_t sec[512];
	uint64_t disk_sectors = 0, used_end = 0, avail;
	int n, i, ndisk = -1, di = 0;

	n = ubix_disk_query(info, UBIX_DISK_MAX);
	check(n > 0, "ubix_disk_query returns a disk");
	if (n <= 0)
		return 1;

	for (i = 0; i < n; i++)
	{
		if (info[i].kind == UBIX_DK_DISK)
		{
			if (ndisk < 0)
			{
				ndisk = i;
				disk_sectors = info[i].sectors;
			}
			di++;
		}
	}
	for (i = 0; i < n; i++)
		if (info[i].kind == UBIX_DK_PART && info[i].parent == ndisk &&
		    info[i].lba_start + info[i].sectors > used_end)
			used_end = info[i].lba_start + info[i].sectors;
	printf("disk %s: %llu sectors, partitions end at %llu\n",
	       info[ndisk].name,
	       (unsigned long long)disk_sectors,
	       (unsigned long long)used_end);

	w.disk = 0;
	check(ubix_disk_rawio(0, UBIX_DISK_RAW_READ, 0, 1, sec) == 0, "raw read of LBA 0");
	check(ubix_disk_rawio(99, UBIX_DISK_RAW_READ, 0, 1, sec) != 0, "raw read of bad disk index refused");
	check(ubix_disk_rawio(0, UBIX_DISK_RAW_READ, disk_sectors, 1, sec) != 0, "raw read past end refused");
	check(ubix_disk_rawio(0, UBIX_DISK_RAW_WRITE, 0, 1, sec) != 0, "write to LBA 0 of mounted disk refused");

	for (i = 0; i < n; i++)
	{
		if (info[i].kind == UBIX_DK_PART && info[i].parent == ndisk && (info[i].flags & UBIX_DF_MOUNTED))
		{
			check(ubix_disk_rawio(0, UBIX_DISK_RAW_WRITE, info[i].lba_start + 8, 1, sec) != 0,
			      "write inside a mounted partition refused");
			break;
		}
	}
	printf("scheme: %s\n", (info[ndisk].flags & UBIX_DF_GPT) ? "GPT" : "MBR");

	w.base = (used_end + 7) & ~7ULL;
	/* Stop 64 sectors short of the end: a GPT disk's own backup lives there. */
	avail = disk_sectors > w.base + 64 ? disk_sectors - 64 - w.base : 0;
	if (avail < GPT_MIN_SECTORS + 1024)
	{
		printf("FAIL: need >= %d free sectors after the last partition (have %llu)\n",
		       GPT_MIN_SECTORS + 1024,
		       (unsigned long long)avail);
		return 1;
	}
	{
		int wrc = ubix_disk_rawio(0, UBIX_DISK_RAW_WRITE, w.base, 1, sec);

		printf("raw write into free tail: rc=%d (base %llu)\n", wrc, (unsigned long long)w.base);
		check(wrc == 0, "raw write into the unused tail");
	}
	run_suite(&w, avail < WINDOW_SECTORS ? avail : WINDOW_SECTORS);

	printf(g_fail ? "gpttest: FAIL\n" : "gpttest: PASS\n");
	return g_fail;
}
#endif
