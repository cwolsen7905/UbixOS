/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * libgpt — GPT writer/reader, written from the UEFI specification.  See
 * <ubix/diskgpt.h>.
 */

#include <string.h>
#include <ubix/diskgpt.h>

#define GPT_HDR_SIZE 92U
#define GPT_REVISION 0x00010000U
#define GPT_PRIMARY_HDR_LBA 1ULL

static const uint8_t g_zero_guid[16];

const uint8_t gpt_type_efi[16] = {
    0x28,
    0x73,
    0x2A,
    0xC1,
    0x1F,
    0xF8,
    0xD2,
    0x11,
    0xBA,
    0x4B,
    0x00,
    0xA0,
    0xC9,
    0x3E,
    0xC9,
    0x3B,
};
const uint8_t gpt_type_ubix_swap[16] = {
    0x4F,
    0xE2,
    0x7A,
    0x2E,
    0x72,
    0x96,
    0x6D,
    0x43,
    0x98,
    0xC8,
    0x38,
    0x30,
    0x88,
    0x20,
    0x44,
    0xD5,
};
const uint8_t gpt_type_ubix_pool[16] = {
    0x44,
    0x95,
    0x47,
    0xDE,
    0xB7,
    0xD2,
    0x93,
    0x46,
    0x9B,
    0x24,
    0x3A,
    0x0E,
    0x63,
    0x8C,
    0xCF,
    0x81,
};

/** Little-endian stores into an unaligned buffer. */
static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

/**
 * Store a 64-bit value little-endian.
 */
static void put64(uint8_t *p, uint64_t v)
{
	put32(p, (uint32_t)v);
	put32(p + 4, (uint32_t)(v >> 32));
}

/**
 * Load a little-endian 32-bit value.
 */
static uint32_t get32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * Load a little-endian 64-bit value.
 */
static uint64_t get64(const uint8_t *p)
{
	return (uint64_t)get32(p) | ((uint64_t)get32(p + 4) << 32);
}

/**
 * CRC-32 (IEEE 802.3, reflected) as used by GPT; see <ubix/diskgpt.h>.
 */
uint32_t gpt_crc32(uint32_t crc, const void *buf, uint32_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	uint32_t i;
	int k;

	crc = ~crc;
	for (i = 0; i < len; i++)
	{
		crc ^= p[i];
		for (k = 0; k < 8; k++)
			crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320U : crc >> 1;
	}
	return ~crc;
}

/**
 * Initialise an empty table for a disk of @total_sectors; see <ubix/diskgpt.h>.
 */
int gpt_init(struct gpt_table *t, uint64_t total_sectors, const uint8_t disk_guid[16])
{
	if (t == 0 || total_sectors < GPT_MIN_SECTORS)
		return GPT_E_INVALID;
	memset(t, 0, sizeof(*t));
	t->total_sectors = total_sectors;
	if (disk_guid != 0)
		memcpy(t->disk_guid, disk_guid, 16);
	t->first_usable = 2 + GPT_ARRAY_SECTORS;
	t->last_usable = total_sectors - 2 - GPT_ARRAY_SECTORS;
	return 0;
}

/**
 * Serialise one entry into its 128-byte on-disk form.
 */
static void pack_entry(uint8_t *p, const struct gpt_entry *e)
{
	int i;

	memcpy(p, e->type, 16);
	memcpy(p + 16, e->guid, 16);
	put64(p + 32, e->first_lba);
	put64(p + 40, e->last_lba);
	put64(p + 48, e->attr);
	for (i = 0; i < 36; i++)
	{
		p[56 + i * 2] = (uint8_t)e->name[i];
		p[57 + i * 2] = (uint8_t)(e->name[i] >> 8);
	}
}

/**
 * Parse one 128-byte on-disk entry.
 */
static void unpack_entry(struct gpt_entry *e, const uint8_t *p)
{
	int i;

	memcpy(e->type, p, 16);
	memcpy(e->guid, p + 16, 16);
	e->first_lba = get64(p + 32);
	e->last_lba = get64(p + 40);
	e->attr = get64(p + 48);
	for (i = 0; i < 36; i++)
		e->name[i] = (uint16_t)(p[56 + i * 2] | ((uint16_t)p[57 + i * 2] << 8));
}

/**
 * Build one header sector in @sec (zero padded to 512 bytes).
 */
static void build_header(
    uint8_t *sec, const struct gpt_table *t, uint64_t my_lba, uint64_t alt_lba, uint64_t array_lba, uint32_t array_crc)
{
	memset(sec, 0, GPT_SECTOR);
	memcpy(sec, "EFI PART", 8);
	put32(sec + 8, GPT_REVISION);
	put32(sec + 12, GPT_HDR_SIZE);
	put64(sec + 24, my_lba);
	put64(sec + 32, alt_lba);
	put64(sec + 40, t->first_usable);
	put64(sec + 48, t->last_usable);
	memcpy(sec + 56, t->disk_guid, 16);
	put64(sec + 72, array_lba);
	put32(sec + 80, GPT_NUM_ENTRIES);
	put32(sec + 84, GPT_ENTRY_SIZE);
	put32(sec + 88, array_crc);
	put32(sec + 16, gpt_crc32(0, sec, GPT_HDR_SIZE)); /* CRC field was zero */
}

/**
 * Validate and write the full GPT layout; see <ubix/diskgpt.h>.
 */
int gpt_write(const struct gpt_io *io, const struct gpt_table *t)
{
	static uint8_t array[GPT_NUM_ENTRIES * GPT_ENTRY_SIZE];
	uint8_t sec[GPT_SECTOR];
	uint32_t array_crc;
	uint64_t n, backup_array, backup_hdr;
	int i, j;

	if (io == 0 || io->write == 0 || t == 0 || t->total_sectors < GPT_MIN_SECTORS)
		return GPT_E_INVALID;
	n = t->total_sectors;
	if (t->first_usable != 2 + GPT_ARRAY_SECTORS || t->last_usable != n - 2 - GPT_ARRAY_SECTORS)
		return GPT_E_INVALID;

	for (i = 0; i < GPT_NUM_ENTRIES; i++)
	{
		const struct gpt_entry *a = &t->entries[i];

		if (memcmp(a->type, g_zero_guid, 16) == 0)
			continue;
		if (a->first_lba > a->last_lba || a->first_lba < t->first_usable || a->last_lba > t->last_usable)
			return GPT_E_OVERLAP;
		for (j = i + 1; j < GPT_NUM_ENTRIES; j++)
		{
			const struct gpt_entry *b = &t->entries[j];

			if (memcmp(b->type, g_zero_guid, 16) == 0)
				continue;
			if (a->first_lba <= b->last_lba && b->first_lba <= a->last_lba)
				return GPT_E_OVERLAP;
		}
	}

	memset(array, 0, sizeof(array));
	for (i = 0; i < GPT_NUM_ENTRIES; i++)
		pack_entry(array + (size_t)i * GPT_ENTRY_SIZE, &t->entries[i]);
	array_crc = gpt_crc32(0, array, sizeof(array));

	backup_array = n - 1 - GPT_ARRAY_SECTORS;
	backup_hdr = n - 1;

	/* Backup first, then primary, then the protective MBR (UEFI 5.3.2). */
	if (io->write(io->ctx, backup_array, GPT_ARRAY_SECTORS, array) != 0)
		return GPT_E_IO;
	build_header(sec, t, backup_hdr, GPT_PRIMARY_HDR_LBA, backup_array, array_crc);
	if (io->write(io->ctx, backup_hdr, 1, sec) != 0)
		return GPT_E_IO;
	if (io->write(io->ctx, 2, GPT_ARRAY_SECTORS, array) != 0)
		return GPT_E_IO;
	build_header(sec, t, GPT_PRIMARY_HDR_LBA, backup_hdr, 2, array_crc);
	if (io->write(io->ctx, GPT_PRIMARY_HDR_LBA, 1, sec) != 0)
		return GPT_E_IO;

	memset(sec, 0, sizeof(sec));
	sec[446 + 0] = 0x00; /* not bootable */
	sec[446 + 1] = 0x00; /* CHS start 0/0/2 */
	sec[446 + 2] = 0x02;
	sec[446 + 3] = 0x00;
	sec[446 + 4] = 0xEE; /* GPT protective */
	sec[446 + 5] = 0xFF; /* CHS end: max */
	sec[446 + 6] = 0xFF;
	sec[446 + 7] = 0xFF;
	put32(sec + 446 + 8, 1);
	put32(sec + 446 + 12, (n - 1 > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (uint32_t)(n - 1));
	sec[510] = 0x55;
	sec[511] = 0xAA;
	if (io->write(io->ctx, 0, 1, sec) != 0)
		return GPT_E_IO;
	return 0;
}

/**
 * Try one header+array copy.  @return 0 valid, GPT_E_INVALID, or GPT_E_IO.
 */
static int try_copy(const struct gpt_io *io, uint64_t total, uint64_t hdr_lba, uint64_t expect_alt, struct gpt_table *t)
{
	static uint8_t array[GPT_NUM_ENTRIES * GPT_ENTRY_SIZE];
	uint8_t sec[GPT_SECTOR];
	uint32_t crc_self, size;
	uint64_t array_lba;
	int i;

	if (io->read(io->ctx, hdr_lba, 1, sec) != 0)
		return GPT_E_IO;
	if (memcmp(sec, "EFI PART", 8) != 0 || get32(sec + 8) != GPT_REVISION)
		return GPT_E_INVALID;
	size = get32(sec + 12);
	if (size < GPT_HDR_SIZE || size > GPT_SECTOR)
		return GPT_E_INVALID;
	crc_self = get32(sec + 16);
	put32(sec + 16, 0);
	if (gpt_crc32(0, sec, size) != crc_self)
		return GPT_E_INVALID;
	if (get64(sec + 24) != hdr_lba || get64(sec + 32) != expect_alt)
		return GPT_E_INVALID;
	if (get32(sec + 80) != GPT_NUM_ENTRIES || get32(sec + 84) != GPT_ENTRY_SIZE)
		return GPT_E_INVALID; /* only the spec-minimum geometry is produced/accepted */

	t->first_usable = get64(sec + 40);
	t->last_usable = get64(sec + 48);
	memcpy(t->disk_guid, sec + 56, 16);
	array_lba = get64(sec + 72);
	if (t->first_usable > t->last_usable || t->last_usable >= total || array_lba + GPT_ARRAY_SECTORS > total)
		return GPT_E_INVALID;

	{
		uint32_t crc_arr = get32(sec + 88);

		if (io->read(io->ctx, array_lba, GPT_ARRAY_SECTORS, array) != 0)
			return GPT_E_IO;
		if (gpt_crc32(0, array, sizeof(array)) != crc_arr)
			return GPT_E_INVALID;
	}
	for (i = 0; i < GPT_NUM_ENTRIES; i++)
		unpack_entry(&t->entries[i], array + (size_t)i * GPT_ENTRY_SIZE);
	t->total_sectors = total;
	return 0;
}

/**
 * Load and validate a GPT, primary then backup; see <ubix/diskgpt.h>.
 */
int gpt_load(const struct gpt_io *io, uint64_t total_sectors, struct gpt_table *t, int *source)
{
	int rc;

	if (io == 0 || io->read == 0 || t == 0 || total_sectors < GPT_MIN_SECTORS)
		return GPT_E_INVALID;

	rc = try_copy(io, total_sectors, GPT_PRIMARY_HDR_LBA, total_sectors - 1, t);
	if (rc == 0)
	{
		if (source != 0)
			*source = GPT_FROM_PRIMARY;
		return 0;
	}
	rc = try_copy(io, total_sectors, total_sectors - 1, GPT_PRIMARY_HDR_LBA, t);
	if (rc == 0 && source != 0)
		*source = GPT_FROM_BACKUP;
	return rc;
}

/**
 * Verify the protective MBR; see <ubix/diskgpt.h>.
 */
int gpt_check_protective_mbr(const struct gpt_io *io)
{
	uint8_t sec[GPT_SECTOR];

	if (io == 0 || io->read == 0 || io->read(io->ctx, 0, 1, sec) != 0)
		return GPT_E_IO;
	if (sec[510] != 0x55 || sec[511] != 0xAA || sec[446 + 4] != 0xEE || get32(sec + 446 + 8) != 1)
		return GPT_E_INVALID;
	return 0;
}
