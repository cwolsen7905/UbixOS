/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <sys/bus.h>
#include <lib/kprintf.h>
#include <fs/fat/fat_clust.h>
#include <fs/fat/fat_sector.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static uint16_t
le16(const uint8_t *p)
{
	return ((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t
le32(const uint8_t *p)
{
	return ((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static void
put_le16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v);
	p[1] = (uint8_t)(v >> 8);
}

static void
put_le32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v);
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

/* ------------------------------------------------------------------ */
/* Cluster ↔ LBA                                                       */
/* ------------------------------------------------------------------ */

uint32_t
fat_cluster_to_lba(struct fat_fs *fs, uint32_t cluster)
{
	if (cluster < 2) {
		kprintf("fat_cluster_to_lba: invalid cluster %u\n", cluster);
		return (0);
	}
	/* Cast to 64-bit before multiply to avoid overflow on large FAT32 volumes. */
	return (fs->data_lba +
	    (uint32_t)((uint64_t)(cluster - 2) * fs->sectors_per_cluster));
}

/* ------------------------------------------------------------------ */
/* FAT entry read                                                      */
/* ------------------------------------------------------------------ */

uint32_t
fat_cluster_next(struct fat_fs *fs, uint32_t cluster)
{
	uint8_t		 buf[512];
	uint32_t	 fat_offset, fat_sector, offset_in_sector;
	uint32_t	 entry;

	switch (fs->type) {
	case 12: {
		/*
		 * FAT12: each entry is 1.5 bytes.  The entry may straddle a
		 * sector boundary, so we read two consecutive bytes regardless.
		 * We handle the cross-sector case by reading both sectors.
		 */
		fat_offset	 = cluster + cluster / 2;
		fat_sector	 = fs->fat_begin_lba + fat_offset / 512;
		offset_in_sector = fat_offset % 512;

		if (fat_sector_read(fs, fat_sector, buf) != 0)
			return (0);

		if (offset_in_sector == 511) {
			/* Entry straddles two sectors — grab second byte. */
			uint8_t	 byte0 = buf[511];
			uint8_t	 buf2[512];
			if (fat_sector_read(fs, fat_sector + 1, buf2) != 0)
				return (0);
			entry = (uint32_t)byte0 | ((uint32_t)buf2[0] << 8);
		} else {
			entry = le16(buf + offset_in_sector);
		}

		entry = (cluster & 1) ? (entry >> 4) : (entry & 0x0FFF);

		/* Normalise EOC: FAT12 EOC is 0xFF8–0xFFF */
		if (entry >= 0xFF8)
			entry = FAT_CLUSTER_EOC;
		return (entry);
	}

	case 16:
		fat_offset	 = cluster * 2;
		fat_sector	 = fs->fat_begin_lba + fat_offset / 512;
		offset_in_sector = fat_offset % 512;
		if (fat_sector_read(fs, fat_sector, buf) != 0)
			return (0);
		entry = le16(buf + offset_in_sector);
		/* FAT16 EOC: 0xFFF8–0xFFFF */
		if (entry >= 0xFFF8)
			entry = FAT_CLUSTER_EOC;
		return (entry);

	case 32:
		fat_offset	 = cluster * 4;
		fat_sector	 = fs->fat_begin_lba + fat_offset / 512;
		offset_in_sector = fat_offset % 512;
		if (fat_sector_read(fs, fat_sector, buf) != 0)
			return (0);
		entry = le32(buf + offset_in_sector) & 0x0FFFFFFF;
		if (entry >= 0x0FFFFFF8)
			entry = FAT_CLUSTER_EOC;
		return (entry);

	default:
		kprintf("fat_cluster_next: unknown FAT type %d\n", fs->type);
		return (0);
	}
}

/* ------------------------------------------------------------------ */
/* FAT entry write                                                     */
/* ------------------------------------------------------------------ */

int
fat_cluster_write_entry(struct fat_fs *fs, uint32_t cluster, uint32_t value)
{
	uint8_t		 buf[512];
	uint32_t	 fat_offset, fat_sector, offset_in_sector;

	switch (fs->type) {
	case 12: {
		fat_offset	 = cluster + cluster / 2;
		fat_sector	 = fs->fat_begin_lba + fat_offset / 512;
		offset_in_sector = fat_offset % 512;

		if (fat_sector_read(fs, fat_sector, buf) != 0)
			return (-1);

		if (offset_in_sector == 511) {
			/* Entry straddles two sectors. */
			uint8_t	 buf2[512];
			if (fat_sector_read(fs, fat_sector + 1, buf2) != 0)
				return (-1);

			if (cluster & 1) {
				buf[511] = (buf[511] & 0x0F) |
				    (uint8_t)((value & 0x0F) << 4);
				buf2[0]  = (uint8_t)(value >> 4);
			} else {
				buf[511] = (uint8_t)(value & 0xFF);
				buf2[0]  = (buf2[0] & 0xF0) |
				    (uint8_t)((value >> 8) & 0x0F);
			}
			if (fat_sector_write(fs, fat_sector, buf) != 0)
				return (-1);
			if (fat_sector_write(fs, fat_sector + 1, buf2) != 0)
				return (-1);
		} else {
			uint16_t word = le16(buf + offset_in_sector);
			if (cluster & 1)
				word = (word & 0x000F) | (uint16_t)(value << 4);
			else
				word = (word & 0xF000) | (uint16_t)(value & 0x0FFF);
			put_le16(buf + offset_in_sector, word);
			if (fat_sector_write(fs, fat_sector, buf) != 0)
				return (-1);
		}
		return (0);
	}

	case 16: {
		fat_offset	 = cluster * 2;
		fat_sector	 = fs->fat_begin_lba + fat_offset / 512;
		offset_in_sector = fat_offset % 512;
		if (fat_sector_read(fs, fat_sector, buf) != 0)
			return (-1);
		put_le16(buf + offset_in_sector, (uint16_t)(value & 0xFFFF));
		return (fat_sector_write(fs, fat_sector, buf));
	}

	case 32: {
		fat_offset	 = cluster * 4;
		fat_sector	 = fs->fat_begin_lba + fat_offset / 512;
		offset_in_sector = fat_offset % 512;
		if (fat_sector_read(fs, fat_sector, buf) != 0)
			return (-1);
		/* Preserve top 4 bits (reserved) */
		uint32_t existing = le32(buf + offset_in_sector);
		uint32_t updated  = (existing & 0xF0000000) | (value & 0x0FFFFFFF);
		put_le32(buf + offset_in_sector, updated);
		return (fat_sector_write(fs, fat_sector, buf));
	}

	default:
		kprintf("fat_cluster_write_entry: unknown FAT type %d\n",
		    fs->type);
		return (-1);
	}
}

/* ------------------------------------------------------------------ */
/* Cluster allocation                                                  */
/* ------------------------------------------------------------------ */

uint32_t
fat_cluster_alloc(struct fat_fs *fs, uint32_t after)
{
	uint32_t	 start, c, next;

	start = (fs->type == 32 && fs->free_cluster_hint >= 2)
	    ? fs->free_cluster_hint : 2;

	/* Linear scan from hint, wrap if needed. */
	for (c = start; c <= fs->total_clusters + 1; c++) {
		next = fat_cluster_next(fs, c);
		if (next == FAT_CLUSTER_FREE) {
			if (fat_cluster_write_entry(fs, c,
			    FAT_CLUSTER_EOC) != 0)
				return (0);
			if (after != 0) {
				if (fat_cluster_write_entry(fs, after, c) != 0) {
					/* Roll back the allocation. */
					fat_cluster_write_entry(fs, c,
					    FAT_CLUSTER_FREE);
					return (0);
				}
			}
			fs->free_cluster_hint = c + 1;
			return (c);
		}
	}

	/* Wrap around and retry from cluster 2 if we started mid-disk. */
	if (start > 2) {
		for (c = 2; c < start; c++) {
			next = fat_cluster_next(fs, c);
			if (next == FAT_CLUSTER_FREE) {
				if (fat_cluster_write_entry(fs, c,
				    FAT_CLUSTER_EOC) != 0)
					return (0);
				if (after != 0) {
					if (fat_cluster_write_entry(fs, after,
					    c) != 0) {
						fat_cluster_write_entry(fs, c,
						    FAT_CLUSTER_FREE);
						return (0);
					}
				}
				fs->free_cluster_hint = c + 1;
				return (c);
			}
		}
	}

	kprintf("fat_cluster_alloc: disk full\n");
	return (0);
}

/* ------------------------------------------------------------------ */
/* Cluster chain free                                                  */
/* ------------------------------------------------------------------ */

int
fat_cluster_free_chain(struct fat_fs *fs, uint32_t start)
{
	uint32_t	 c, next;

	c = start;
	while (c >= 2 && c < FAT_CLUSTER_BAD) {
		next = fat_cluster_next(fs, c);
		if (next == 0) {
			kprintf("fat_cluster_free_chain: I/O error at %u\n", c);
			return (-1);
		}
		if (fat_cluster_write_entry(fs, c, FAT_CLUSTER_FREE) != 0)
			return (-1);
		c = next;
	}
	return (0);
}
