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
#include <lib/kmalloc.h>
#include <string.h>
#include <fs/fat/fat_file.h>
#include <fs/fat/fat_sector.h>
#include <fs/fat/fat_clust.h>
#include <fs/fat/fat_dir.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

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

/* Extract the basename (last component) of path into out[]. */
static void
path_basename(const char *path, char *out, int maxlen)
{
	const char	*base = path;

	for (const char *p = path; *p; p++)
		if (*p == '/')
			base = p + 1;
	strncpy(out, base, (size_t)(maxlen - 1));
	out[maxlen - 1] = '\0';
}

/*
 * Write cluster and size fields of a directory entry in one sector read/write.
 */
static int
dirent_set_clus_size(struct fat_fs *fs, uint32_t sec, uint16_t off,
    uint32_t clus, uint32_t size)
{
	uint8_t	 buf[512];

	if (fat_sector_read(fs, sec, buf) != 0)
		return (-1);
	put_le16(buf + off + 20, (uint16_t)(clus >> 16));
	put_le16(buf + off + 26, (uint16_t)(clus & 0xFFFF));
	put_le32(buf + off + 28, size);
	return (fat_sector_write(fs, sec, buf));
}

/* ------------------------------------------------------------------ */
/* Cluster chain walk                                                  */
/* ------------------------------------------------------------------ */

/*
 * Walk the cluster chain to find which cluster contains byte position pos,
 * then update f->cur_cluster.  Always walks from start_cluster so that seeks
 * backward work correctly.
 *
 * Returns 0 on success, -1 if the chain is too short for pos.
 */
static int
seek_to_cluster(struct fat_file *f, uint32_t pos)
{
	struct fat_fs	*fs = f->fs;
	uint32_t	 cluster_bytes =
	    (uint32_t)fs->sectors_per_cluster * fs->bytes_per_sector;
	uint32_t	 target_idx = pos / cluster_bytes;
	uint32_t	 c = f->start_cluster;

	for (uint32_t i = 0; i < target_idx; i++) {
		c = fat_cluster_next(fs, c);
		if (c == FAT_CLUSTER_EOC || c < 2)
			return (-1);
	}
	f->cur_cluster = c;
	return (0);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

struct fat_file *
fat_file_open(struct fat_fs *fs, const char *path, uint8_t mode)
{
	struct fat_raw_dirent	 ent;
	uint32_t		 dir_cluster, start_cluster;
	uint32_t		 entry_sec;
	uint16_t		 entry_off;
	int			 found;
	char			 basename[256];
	struct fat_file		*f;

	found = (fat_path_resolve(fs, path, &dir_cluster, &ent,
	    &entry_sec, &entry_off) == 0);

	/* Reject directories */
	if (found && (ent.attr & FAT_ATTR_DIR))
		return (NULL);

	path_basename(path, basename, 256);

	if (mode == FAT_MODE_R) {
		if (!found)
			return (NULL);
		start_cluster = ((uint32_t)ent.clus_hi << 16) | ent.clus_lo;

	} else if (mode == FAT_MODE_W) {
		if (found) {
			/* Truncate: free the existing cluster chain. */
			start_cluster = ((uint32_t)ent.clus_hi << 16) |
			    ent.clus_lo;
			if (start_cluster >= 2)
				fat_cluster_free_chain(fs, start_cluster);
		}
		/* Allocate the first cluster up front. */
		start_cluster = fat_cluster_alloc(fs, 0);
		if (start_cluster == 0)
			return (NULL);

		if (found) {
			if (dirent_set_clus_size(fs, entry_sec, entry_off,
			    start_cluster, 0) != 0) {
				fat_cluster_free_chain(fs, start_cluster);
				return (NULL);
			}
		} else {
			if (fat_dir_create_entry(fs, dir_cluster, basename,
			    FAT_ATTR_ARCH, start_cluster,
			    &entry_sec, &entry_off) != 0) {
				fat_cluster_free_chain(fs, start_cluster);
				return (NULL);
			}
		}

	} else if (mode == FAT_MODE_A) {
		if (found) {
			start_cluster = ((uint32_t)ent.clus_hi << 16) |
			    ent.clus_lo;
			if (start_cluster < 2) {
				start_cluster = fat_cluster_alloc(fs, 0);
				if (start_cluster == 0)
					return (NULL);
				dirent_set_clus_size(fs, entry_sec, entry_off,
				    start_cluster, ent.file_size);
			}
		} else {
			start_cluster = fat_cluster_alloc(fs, 0);
			if (start_cluster == 0)
				return (NULL);
			if (fat_dir_create_entry(fs, dir_cluster, basename,
			    FAT_ATTR_ARCH, start_cluster,
			    &entry_sec, &entry_off) != 0) {
				fat_cluster_free_chain(fs, start_cluster);
				return (NULL);
			}
		}
	} else {
		return (NULL);
	}

	f = (struct fat_file *)kmalloc(sizeof(struct fat_file));
	if (f == NULL)
		return (NULL);

	f->fs		= fs;
	f->start_cluster= start_cluster;
	f->cur_cluster	= start_cluster;
	f->file_size	= (mode == FAT_MODE_W) ? 0 :
	    (found ? ent.file_size : 0);
	f->position	= 0;
	f->dir_sector	= entry_sec;
	f->dir_offset	= entry_off;
	f->mode		= mode;
	f->size_dirty	= 0;
	f->buf_dirty	= 0;
	f->buf_lba	= (uint32_t)-1;
	memset(f->buf, 0, 512);

	/* Append: seek to end of file. */
	if (mode == FAT_MODE_A && f->file_size > 0) {
		if (fat_file_seek(f, f->file_size) != 0) {
			kfree(f);
			return (NULL);
		}
	}

	return (f);
}

int
fat_file_seek(struct fat_file *f, uint32_t pos)
{
	struct fat_fs	*fs = f->fs;
	uint32_t	 cluster_bytes =
	    (uint32_t)fs->sectors_per_cluster * fs->bytes_per_sector;

	if (pos == f->position)
		return (0);

	if (pos == 0) {
		f->cur_cluster = f->start_cluster;
		f->position    = 0;
		return (0);
	}

	/*
	 * For a forward seek: try to walk forward from cur_cluster when the
	 * target cluster index is >= the current cluster index.  This avoids
	 * restarting the chain walk on sequential reads.
	 */
	uint32_t cur_idx    = f->position / cluster_bytes;
	uint32_t target_idx = pos / cluster_bytes;

	if (target_idx >= cur_idx) {
		uint32_t c = f->cur_cluster;
		for (uint32_t i = cur_idx; i < target_idx; i++) {
			c = fat_cluster_next(fs, c);
			if (c == FAT_CLUSTER_EOC || c < 2)
				return (-1);
		}
		f->cur_cluster = c;
	} else {
		/* Backward seek — must restart from start_cluster. */
		if (seek_to_cluster(f, pos) != 0)
			return (-1);
	}

	f->position = pos;
	return (0);
}

int
fat_file_read(struct fat_file *f, void *buf, uint32_t size, uint32_t *got)
{
	struct fat_fs	*fs = f->fs;
	uint32_t	 cluster_bytes =
	    (uint32_t)fs->sectors_per_cluster * fs->bytes_per_sector;
	uint8_t		*dst = (uint8_t *)buf;
	uint32_t	 remaining, bytes_read;
	uint8_t		 sector_buf[512];

	/* Clamp to available data. */
	if (f->position >= f->file_size) {
		if (got)
			*got = 0;
		return (0);
	}
	remaining = f->file_size - f->position;
	if (size < remaining)
		remaining = size;
	bytes_read = 0;

	while (remaining > 0) {
		uint32_t offset_in_cluster = f->position % cluster_bytes;
		uint32_t sec_in_clust      = offset_in_cluster / 512;
		uint32_t byte_off          = offset_in_cluster % 512;
		uint32_t lba = fat_cluster_to_lba(fs, f->cur_cluster) +
		    sec_in_clust;

		/*
		 * Fast path: sector-aligned read directly into caller's buffer,
		 * consuming all remaining whole sectors in the current cluster.
		 * Avoids the intermediate sector_buf[] copy and issues one
		 * block-layer call per cluster instead of one per sector.
		 */
		if (byte_off == 0 && remaining >= 512) {
			uint32_t secs_left_clust = fs->sectors_per_cluster -
			    sec_in_clust;
			uint32_t secs_want       = remaining / 512;
			uint32_t secs_to_read    = secs_left_clust < secs_want ?
			    secs_left_clust : secs_want;
			uint32_t nbytes          = secs_to_read * 512;

			if (fs->mp->device->dev_blk_ops->read(
			    fs->mp->device, lba, secs_to_read,
			    dst + bytes_read) != 0)
				break;
			bytes_read  += nbytes;
			remaining   -= nbytes;
			f->position += nbytes;
			if (remaining > 0 && f->position % cluster_bytes == 0) {
				uint32_t next = fat_cluster_next(fs, f->cur_cluster);
				if (next == FAT_CLUSTER_EOC || next < 2)
					break;
				f->cur_cluster = next;
			}
			continue;
		}

		/* Slow path: partial sector at start/end. */
		{
			uint32_t can_copy = 512 - byte_off;
			if (can_copy > remaining)
				can_copy = remaining;

			if (fat_sector_read(fs, lba, sector_buf) != 0)
				break;

			memcpy(dst + bytes_read, sector_buf + byte_off, can_copy);
			bytes_read  += can_copy;
			remaining   -= can_copy;
			f->position += can_copy;

			/* Advance cluster if we just crossed a boundary. */
			if (remaining > 0 && f->position % cluster_bytes == 0) {
				uint32_t next = fat_cluster_next(fs, f->cur_cluster);
				if (next == FAT_CLUSTER_EOC || next < 2)
					break;
				f->cur_cluster = next;
			}
		}
	}

	if (got)
		*got = bytes_read;
	return (0);
}

int
fat_file_write(struct fat_file *f, const void *buf, uint32_t size)
{
	struct fat_fs		*fs = f->fs;
	uint32_t		 cluster_bytes =
	    (uint32_t)fs->sectors_per_cluster * fs->bytes_per_sector;
	const uint8_t		*src = (const uint8_t *)buf;
	uint32_t		 written = 0;

	while (written < size) {
		uint32_t offset_in_cluster = f->position % cluster_bytes;
		uint32_t sec_in_clust      = offset_in_cluster / 512;
		uint32_t byte_off          = offset_in_cluster % 512;
		uint32_t lba = fat_cluster_to_lba(fs, f->cur_cluster) +
		    sec_in_clust;
		uint32_t can_write = 512 - byte_off;

		if (can_write > size - written)
			can_write = size - written;

		if (byte_off == 0 && can_write == 512) {
			/*
			 * Full-sector write: bypass the file buffer.
			 * If our buffer happens to hold this sector,
			 * discard it — we are about to overwrite it.
			 */
			if (f->buf_lba == lba)
				f->buf_dirty = 0;
			if (fat_sector_write(fs, lba, src + written) != 0)
				return (-1);
		} else {
			/* Partial write: use the per-file write buffer. */
			if (f->buf_lba != lba) {
				/* Evict old buffer slot. */
				if (f->buf_dirty) {
					if (fat_sector_write(fs, f->buf_lba,
					    f->buf) != 0)
						return (-1);
					f->buf_dirty = 0;
				}
				if (fat_sector_read(fs, lba, f->buf) != 0)
					return (-1);
				f->buf_lba = lba;
			}
			memcpy(f->buf + byte_off, src + written, can_write);
			f->buf_dirty = 1;
		}

		f->position += can_write;
		written     += can_write;

		if (f->position > f->file_size) {
			f->file_size  = f->position;
			f->size_dirty = 1;
		}

		/* Cross cluster boundary: follow or allocate. */
		if (written < size && f->position % cluster_bytes == 0) {
			uint32_t next = fat_cluster_next(fs, f->cur_cluster);
			if (next == FAT_CLUSTER_EOC || next < 2) {
				next = fat_cluster_alloc(fs, f->cur_cluster);
				if (next == 0)
					return (-1);
			}
			f->cur_cluster = next;
		}
	}

	return (0);
}

int
fat_file_flush(struct fat_file *f)
{
	if (f->buf_dirty) {
		if (fat_sector_write(f->fs, f->buf_lba, f->buf) != 0)
			return (-1);
		f->buf_dirty = 0;
	}
	if (f->size_dirty) {
		if (fat_dir_update_size(f->fs, f->dir_sector, f->dir_offset,
		    f->file_size) != 0)
			return (-1);
		f->size_dirty = 0;
	}
	return (0);
}

void
fat_file_close(struct fat_file *f)
{
	fat_file_flush(f);
	kfree(f);
}
