/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * GPT (GUID Partition Table) reader for the kernel's partition layer, written
 * from the UEFI specification (section "GUID Partition Table (GPT) Disk
 * Layout").  Read-only: the writer lives in userland (lib/libgpt).
 *
 * GUIDs are kept as the 16 raw on-disk bytes (first three fields little-endian)
 * so they compare with memcmp and need no byte swapping.
 */
#ifndef _SYS_GPT_PART_H
#define _SYS_GPT_PART_H

#include <sys/types.h>
#include <sys/bus.h>

#define GPT_GUID_LEN 16
#define GPT_SECTOR_SIZE 512

/* Partition-type GUIDs (raw on-disk bytes).  uBix swap/pool are random v4 GUIDs. */
extern const u_int8_t gpt_type_efi[GPT_GUID_LEN];
extern const u_int8_t gpt_type_ubix_swap[GPT_GUID_LEN];
extern const u_int8_t gpt_type_ubix_pool[GPT_GUID_LEN];

/** One in-use GPT entry, reduced to what the partition layer needs. */
struct gpt_part
{
	u_int8_t type[GPT_GUID_LEN]; /**< partition-type GUID (raw bytes) */
	u_int8_t guid[GPT_GUID_LEN]; /**< unique partition GUID (raw bytes) */
	u_int64_t first_lba;         /**< first LBA, inclusive */
	u_int64_t last_lba;          /**< last LBA, inclusive */
	u_int32_t slot;              /**< 0-based index in the entry array */
};

/**
 * IEEE 802.3 CRC-32 (reflected, poly 0xEDB88320), the checksum GPT uses.
 *
 * @param crc  running value; pass 0 to start, the previous return to continue.
 */
u_int32_t gpt_crc32(u_int32_t crc, const void *buf, u_int32_t len);

/**
 * Map a partition-type GUID onto the MBR type byte the kernel keys on (EFI
 * System -> 0x0C, uBix swap -> 0x82, uBix pool -> 0x9C, anything else 0xDA).
 */
u_int8_t gpt_guid_mbr_type(const u_int8_t *guid);

/**
 * Read the GPT on @disk: validate the primary header and entry array (signature,
 * revision, size, header CRC, LBA sanity, entry-array CRC), fall back to the
 * backup header at the last LBA, and return the in-use entries.
 *
 * @param disk          whole-disk block device (512-byte sectors).
 * @param disk_sectors  total sectors of @disk (locates the backup header).
 * @param out           receives up to @max in-use entries.
 * @return number of entries written (>= 0), or -1 if no valid GPT is present.
 */
int gpt_read_partitions(struct ubx_device *disk, u_int64_t disk_sectors, struct gpt_part *out, int max);

#endif /* _SYS_GPT_PART_H */
