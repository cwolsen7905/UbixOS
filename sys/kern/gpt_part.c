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
 * gpt_part — GPT reader, written from the UEFI specification.  Validates the
 * primary header + entry array, falls back to the backup header.  The entry
 * array is walked a sector at a time so no large kernel stack buffer is needed.
 */

#include <sys/gpt_part.h>
#include <dev/partition.h> /* MBR_TYPE_* */
#include <string.h>

#define GPT_SIG "EFI PART"
#define GPT_REV_1_0 0x00010000U
#define GPT_HDR_MIN 92U
#define GPT_MAX_ENTRIES 1024U /* sanity bound; spec default is 128 */

/** Little-endian loads from an unaligned sector buffer. */
static u_int32_t gpt_le32(const u_int8_t *p)
{
	return ((u_int32_t)p[0]) | ((u_int32_t)p[1] << 8) | ((u_int32_t)p[2] << 16) | ((u_int32_t)p[3] << 24);
}

/**
 * Load a little-endian 64-bit value.
 */
static u_int64_t gpt_le64(const u_int8_t *p)
{
	return (u_int64_t)gpt_le32(p) | ((u_int64_t)gpt_le32(p + 4) << 32);
}

/**
 * IEEE 802.3 CRC-32 (reflected); see sys/gpt_part.h.
 */
u_int32_t gpt_crc32(u_int32_t crc, const void *buf, u_int32_t len)
{
	const u_int8_t *p = (const u_int8_t *)buf;
	u_int32_t i;
	int k;

	crc = ~crc;
	for (i = 0; i < len; i++)
	{
		crc ^= p[i];
		for (k = 0; k < 8; k++)
			crc = (crc >> 1) ^ (0xEDB88320U & (u_int32_t) - (int32_t)(crc & 1));
	}
	return (~crc);
}

/**
 * Map a partition-type GUID to an MBR type byte; see sys/gpt_part.h.
 */
u_int8_t gpt_guid_mbr_type(const u_int8_t *guid)
{
	static const u_int8_t efi[GPT_GUID_LEN] = GPT_TYPE_EFI_BYTES;
	static const u_int8_t swap[GPT_GUID_LEN] = GPT_TYPE_UBIX_SWAP_BYTES;
	static const u_int8_t pool[GPT_GUID_LEN] = GPT_TYPE_UBIX_POOL_BYTES;

	if (memcmp(guid, efi, GPT_GUID_LEN) == 0)
		return (MBR_TYPE_FAT32);
	if (memcmp(guid, swap, GPT_GUID_LEN) == 0)
		return (MBR_TYPE_SWAP);
	if (memcmp(guid, pool, GPT_GUID_LEN) == 0)
		return (MBR_TYPE_UBPOOL);
	return (MBR_TYPE_OTHER);
}

/**
 * Read one 512-byte sector through the device's block ops.
 *
 * @return 0 on success, -1 on error.
 */
static int gpt_rd(struct ubx_device *disk, u_int64_t lba, void *buf)
{
	if (lba > 0xFFFFFFFFULL)
		return (-1);
	return (disk->dev_blk_ops->read(disk, (u_int32_t)lba, 1, buf) == 0 ? 0 : -1);
}

/**
 * Validate the header at @hdr_lba and, if good, walk its entry array.
 *
 * @param out  may be NULL to validate only; else receives in-use entries.
 * @return in-use entry count (>= 0), or -1 if the header/array is invalid.
 */
static int gpt_try(struct ubx_device *disk, u_int64_t disk_sectors, u_int64_t hdr_lba, struct gpt_part *out, int max)
{
	u_int8_t sec[GPT_SECTOR_SIZE];
	u_int32_t hsize, crc_self, crc_tab, nent, entsz, tab_secs, total, s, per;
	u_int64_t tab_lba, first_usable, last_usable;
	u_int32_t crc = 0;
	u_int32_t idx = 0;
	int n = 0;

	if (gpt_rd(disk, hdr_lba, sec) != 0)
		return (-1);
	if (memcmp(sec, GPT_SIG, 8) != 0 || gpt_le32(sec + 8) != GPT_REV_1_0)
		return (-1);
	hsize = gpt_le32(sec + 12);
	if (hsize < GPT_HDR_MIN || hsize > GPT_SECTOR_SIZE)
		return (-1);

	crc_self = gpt_le32(sec + 16);
	sec[16] = sec[17] = sec[18] = sec[19] = 0;
	if (gpt_crc32(0, sec, hsize) != crc_self)
		return (-1);
	if (gpt_le64(sec + 24) != hdr_lba)
		return (-1);

	first_usable = gpt_le64(sec + 40);
	last_usable = gpt_le64(sec + 48);
	tab_lba = gpt_le64(sec + 72);
	nent = gpt_le32(sec + 80);
	entsz = gpt_le32(sec + 84);
	crc_tab = gpt_le32(sec + 88);

	if (first_usable > last_usable || (disk_sectors != 0 && last_usable >= disk_sectors))
		return (-1);
	/* Entry size: >= 128 and a power of two; array bounded. */
	if (entsz < 128 || (entsz & (entsz - 1)) != 0 || entsz > GPT_SECTOR_SIZE || nent == 0 || nent > GPT_MAX_ENTRIES)
		return (-1);
	if (disk_sectors != 0 && tab_lba + 1 > disk_sectors)
		return (-1);

	per = GPT_SECTOR_SIZE / entsz;
	total = nent * entsz;
	tab_secs = (total + GPT_SECTOR_SIZE - 1) / GPT_SECTOR_SIZE;

	for (s = 0; s < tab_secs; s++)
	{
		u_int32_t e;
		u_int32_t bytes = total - s * GPT_SECTOR_SIZE;

		if (bytes > GPT_SECTOR_SIZE)
			bytes = GPT_SECTOR_SIZE;
		if (gpt_rd(disk, tab_lba + s, sec) != 0)
			return (-1);
		crc = gpt_crc32(crc, sec, bytes);

		for (e = 0; e < per && idx < nent; e++, idx++)
		{
			const u_int8_t *ent = sec + e * entsz;
			static const u_int8_t zero[GPT_GUID_LEN] = {0};
			u_int64_t first, last;

			if (out == 0 || n >= max || memcmp(ent, zero, GPT_GUID_LEN) == 0)
				continue;
			first = gpt_le64(ent + 32);
			last = gpt_le64(ent + 40);
			if (first > last || first < first_usable || last > last_usable)
				continue;
			memcpy(out[n].type, ent, GPT_GUID_LEN);
			memcpy(out[n].guid, ent + 16, GPT_GUID_LEN);
			out[n].first_lba = first;
			out[n].last_lba = last;
			out[n].slot = idx;
			n++;
		}
	}
	if (crc != crc_tab)
		return (-1);
	return (n);
}

/**
 * Read the in-use GPT entries, primary then backup; see sys/gpt_part.h.
 */
int gpt_read_partitions(struct ubx_device *disk, u_int64_t disk_sectors, struct gpt_part *out, int max)
{
	int n;

	if (disk == 0 || disk->dev_blk_ops == 0 || disk->dev_blk_ops->read == 0 || out == 0 || max <= 0)
		return (-1);

	n = gpt_try(disk, disk_sectors, 1, out, max);
	if (n >= 0)
		return (n);
	if (disk_sectors > 2)
		return (gpt_try(disk, disk_sectors, disk_sectors - 1, out, max));
	return (-1);
}
