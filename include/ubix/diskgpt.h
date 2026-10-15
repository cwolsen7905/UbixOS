/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * libgpt — create, read and validate GUID Partition Tables.  Written from the
 * UEFI specification ("GUID Partition Table (GPT) Disk Layout").  Pure library:
 * all disk access goes through a caller-supplied gpt_io, so the same code runs
 * over ubix_disk_rawio() in-OS and over a file or memory buffer on a host.
 *
 * Sector size is fixed at 512 bytes.  GUIDs are the 16 raw on-disk bytes (first
 * three fields little-endian).
 */
#ifndef UBIXOS_DISKGPT_H
#define UBIXOS_DISKGPT_H

#include <stdint.h>

#define GPT_SECTOR 512
#define GPT_ENTRY_SIZE 128
#define GPT_NUM_ENTRIES 128 /* spec minimum: 16 KiB array = 32 sectors */
#define GPT_ARRAY_SECTORS ((GPT_NUM_ENTRIES * GPT_ENTRY_SIZE) / GPT_SECTOR)
#define GPT_MIN_SECTORS (1 + 1 + GPT_ARRAY_SECTORS + 1 + GPT_ARRAY_SECTORS + 1 + 1)

/* Partition-type GUIDs (raw on-disk bytes); same values as the kernel's. */
extern const uint8_t g_gpt_type_efi[16];
extern const uint8_t g_gpt_type_ubix_swap[16];
extern const uint8_t g_gpt_type_ubix_pool[16];

/** Sector I/O supplied by the caller; return 0 on success, nonzero on error. */
struct gpt_io
{
	int (*read)(void *ctx, uint64_t lba, uint32_t count, void *buf);
	int (*write)(void *ctx, uint64_t lba, uint32_t count, const void *buf);
	void *ctx;
};

/** One partition: a type, a unique id, an inclusive LBA range and a name. */
struct gpt_entry
{
	uint8_t type[16]; /**< partition-type GUID; all-zero marks an unused slot */
	uint8_t guid[16]; /**< unique partition GUID */
	uint64_t first_lba;
	uint64_t last_lba;
	uint64_t attr;
	uint16_t name[36]; /**< UTF-16LE, NUL padded */
};

/** An in-memory partition table for one disk. */
struct gpt_table
{
	uint64_t total_sectors; /**< disk size */
	uint8_t disk_guid[16];
	uint64_t first_usable;
	uint64_t last_usable;
	struct gpt_entry entries[GPT_NUM_ENTRIES];
};

/** gpt_load() source: which copy of the table was used. */
#define GPT_FROM_PRIMARY 1
#define GPT_FROM_BACKUP 2

/** Errors (negative return values). */
#define GPT_E_IO (-1)      /**< sector I/O failed */
#define GPT_E_INVALID (-2) /**< no valid GPT (or bad arguments to write) */
#define GPT_E_OVERLAP (-3) /**< entries overlap or fall outside usable area */

/**
 * IEEE 802.3 CRC-32 as used by GPT; pass 0 to start or a prior result to
 * continue.
 */
uint32_t gpt_crc32(uint32_t crc, const void *buf, uint32_t len);

/**
 * Compute first/last usable LBAs for a disk of @total_sectors and clear the
 * table; fill entries afterwards and call gpt_write().
 *
 * @return 0, or GPT_E_INVALID if the disk is too small for a GPT.
 */
int gpt_init(struct gpt_table *t, uint64_t total_sectors, const uint8_t disk_guid[16]);

/**
 * Validate @t's entries (inside the usable area, no overlap) and write the
 * protective MBR, primary header + array, backup array + header.
 *
 * @return 0, GPT_E_OVERLAP, GPT_E_INVALID or GPT_E_IO.
 */
int gpt_write(const struct gpt_io *io, const struct gpt_table *t);

/**
 * Read and validate a GPT: signature, revision, header CRC, own/alternate LBA,
 * usable range, array CRC.  The primary is preferred; if it is damaged the
 * backup at the last LBA is used.
 *
 * @param source  receives GPT_FROM_PRIMARY or GPT_FROM_BACKUP; may be NULL.
 * @return 0, GPT_E_INVALID if neither copy is valid, or GPT_E_IO.
 */
int gpt_load(const struct gpt_io *io, uint64_t total_sectors, struct gpt_table *t, int *source);

/**
 * Check that the protective MBR at LBA 0 is well formed (0x55AA, one 0xEE
 * partition starting at LBA 1).  @return 0 if valid, GPT_E_INVALID, GPT_E_IO.
 */
int gpt_check_protective_mbr(const struct gpt_io *io);

#endif /* UBIXOS_DISKGPT_H */
