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

static void put_le16(u_int8_t *p, u_int16_t v)
{
	p[0] = (u_int8_t)(v);
	p[1] = (u_int8_t)(v >> 8);
}

static void put_le32(u_int8_t *p, u_int32_t v)
{
	p[0] = (u_int8_t)(v);
	p[1] = (u_int8_t)(v >> 8);
	p[2] = (u_int8_t)(v >> 16);
	p[3] = (u_int8_t)(v >> 24);
}

/* Extract the basename (last component) of path into out[]. */
static void path_basename(const char *path, char *out, int maxlen)
{
	const char *base = path;

	for (const char *p = path; *p; p++)
		if (*p == '/')
			base = p + 1;
	strncpy(out, base, (size_t)(maxlen - 1));
	out[maxlen - 1] = '\0';
}

/*
 * Write cluster and size fields of a directory entry in one sector read/write.
 */
static int dirent_set_clus_size(struct fat_fs *fs, u_int32_t sec, u_int16_t off, u_int32_t clus, u_int32_t size)
{
	u_int8_t buf[512];

	if (fat_sector_read(fs, sec, buf) != 0)
		return (-1);
	put_le16(buf + off + 20, (u_int16_t)(clus >> 16));
	put_le16(buf + off + 26, (u_int16_t)(clus & 0xFFFF));
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
static int seek_to_cluster(struct fat_file *f, u_int32_t pos)
{
	struct fat_fs *fs = f->fs;
	u_int32_t cluster_bytes = (u_int32_t)fs->sectors_per_cluster * fs->bytes_per_sector;
	u_int32_t target_idx = pos / cluster_bytes;
	u_int32_t c = f->start_cluster;

	for (u_int32_t i = 0; i < target_idx; i++)
	{
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

struct fat_file *fat_file_open(struct fat_fs *fs, const char *path, u_int8_t mode)
{
	struct fat_raw_dirent ent;
	u_int32_t dir_cluster, start_cluster;
	u_int32_t entry_sec;
	u_int16_t entry_off;
	int found;
	char basename[256];
	struct fat_file *f;

	found = (fat_path_resolve(fs, path, &dir_cluster, &ent, &entry_sec, &entry_off) == 0);

	/* Reject directories */
	if (found && (ent.attr & FAT_ATTR_DIR))
		return (NULL);

	path_basename(path, basename, 256);

	if (mode == FAT_MODE_R)
	{
		if (!found)
			return (NULL);
		start_cluster = ((u_int32_t)ent.clus_hi << 16) | ent.clus_lo;
	}
	else if (mode == FAT_MODE_W)
	{
		if (found)
		{
			/* Truncate: free the existing cluster chain. */
			start_cluster = ((u_int32_t)ent.clus_hi << 16) | ent.clus_lo;
			if (start_cluster >= 2)
				fat_cluster_free_chain(fs, start_cluster);
		}
		/* Allocate the first cluster up front. */
		start_cluster = fat_cluster_alloc(fs, 0);
		if (start_cluster == 0)
			return (NULL);

		if (found)
		{
			if (dirent_set_clus_size(fs, entry_sec, entry_off, start_cluster, 0) != 0)
			{
				fat_cluster_free_chain(fs, start_cluster);
				return (NULL);
			}
		}
		else
		{
			if (fat_dir_create_entry(
			        fs, dir_cluster, basename, FAT_ATTR_ARCH, start_cluster, &entry_sec, &entry_off) != 0)
			{
				fat_cluster_free_chain(fs, start_cluster);
				return (NULL);
			}
		}
	}
	else if (mode == FAT_MODE_A)
	{
		if (found)
		{
			start_cluster = ((u_int32_t)ent.clus_hi << 16) | ent.clus_lo;
			if (start_cluster < 2)
			{
				start_cluster = fat_cluster_alloc(fs, 0);
				if (start_cluster == 0)
					return (NULL);
				dirent_set_clus_size(fs, entry_sec, entry_off, start_cluster, ent.file_size);
			}
		}
		else
		{
			start_cluster = fat_cluster_alloc(fs, 0);
			if (start_cluster == 0)
				return (NULL);
			if (fat_dir_create_entry(
			        fs, dir_cluster, basename, FAT_ATTR_ARCH, start_cluster, &entry_sec, &entry_off) != 0)
			{
				fat_cluster_free_chain(fs, start_cluster);
				return (NULL);
			}
		}
	}
	else
	{
		return (NULL);
	}

	f = (struct fat_file *)kmalloc(sizeof(struct fat_file));
	if (f == NULL)
		return (NULL);

	f->fs = fs;
	f->start_cluster = start_cluster;
	f->cur_cluster = start_cluster;
	f->file_size = (mode == FAT_MODE_W) ? 0 : (found ? ent.file_size : 0);
	f->position = 0;
	f->dir_sector = entry_sec;
	f->dir_offset = entry_off;
	f->mode = mode;
	f->size_dirty = 0;
	f->buf_dirty = 0;
	f->buf_lba = (u_int32_t)-1;
	memset(f->buf, 0, 512);

	/* Append: seek to end of file. */
	if (mode == FAT_MODE_A && f->file_size > 0)
	{
		if (fat_file_seek(f, f->file_size) != 0)
		{
			kfree(f);
			return (NULL);
		}
	}

	return (f);
}

int fat_file_seek(struct fat_file *f, u_int32_t pos)
{
	struct fat_fs *fs = f->fs;
	u_int32_t cluster_bytes = (u_int32_t)fs->sectors_per_cluster * fs->bytes_per_sector;

	if (pos == f->position)
		return (0);

	if (pos == 0)
	{
		f->cur_cluster = f->start_cluster;
		f->position = 0;
		return (0);
	}

	/*
	 * For a forward seek: try to walk forward from cur_cluster when the
	 * target cluster index is >= the current cluster index.  This avoids
	 * restarting the chain walk on sequential reads.
	 */
	u_int32_t cur_idx = f->position / cluster_bytes;
	u_int32_t target_idx = pos / cluster_bytes;

	if (target_idx >= cur_idx)
	{
		u_int32_t c = f->cur_cluster;
		for (u_int32_t i = cur_idx; i < target_idx; i++)
		{
			c = fat_cluster_next(fs, c);
			if (c == FAT_CLUSTER_EOC || c < 2)
				return (-1);
		}
		f->cur_cluster = c;
	}
	else
	{
		/* Backward seek — must restart from start_cluster. */
		if (seek_to_cluster(f, pos) != 0)
			return (-1);
	}

	f->position = pos;
	return (0);
}

int fat_file_read(struct fat_file *f, void *buf, u_int32_t size, u_int32_t *got)
{
	struct fat_fs *fs = f->fs;
	u_int32_t cluster_bytes = (u_int32_t)fs->sectors_per_cluster * fs->bytes_per_sector;
	u_int8_t *dst = (u_int8_t *)buf;
	u_int32_t remaining, bytes_read;
	u_int8_t sector_buf[512];

	/* Clamp to available data. */
	if (f->position >= f->file_size)
	{
		if (got)
			*got = 0;
		return (0);
	}
	remaining = f->file_size - f->position;
	if (size < remaining)
		remaining = size;
	bytes_read = 0;

	while (remaining > 0)
	{
		u_int32_t offset_in_cluster = f->position % cluster_bytes;
		u_int32_t sec_in_clust = offset_in_cluster / 512;
		u_int32_t byte_off = offset_in_cluster % 512;
		u_int32_t lba = fat_cluster_to_lba(fs, f->cur_cluster) + sec_in_clust;

		/*
		 * Fast path: sector-aligned read directly into caller's buffer,
		 * consuming all remaining whole sectors in the current cluster.
		 * Avoids the intermediate sector_buf[] copy and issues one
		 * block-layer call per cluster instead of one per sector.
		 */
		if (byte_off == 0 && remaining >= 512)
		{
			u_int32_t secs_left_clust = fs->sectors_per_cluster - sec_in_clust;
			u_int32_t secs_want = remaining / 512;
			u_int32_t secs_to_read = secs_left_clust < secs_want ? secs_left_clust : secs_want;
			u_int32_t nbytes = secs_to_read * 512;

			if (fs->mp->device->dev_blk_ops->read(fs->mp->device, lba, secs_to_read, dst + bytes_read) != 0)
				break;
			bytes_read += nbytes;
			remaining -= nbytes;
			f->position += nbytes;
			/* Keep cur_cluster consistent with position even when the read ends
			 * exactly on a cluster boundary (remaining == 0): a later seek to
			 * this position sees pos == f->position and trusts cur_cluster, so
			 * it must already point at position's cluster — otherwise the next
			 * read returns the previous cluster's data (corrupts demand-paged
			 * file pages, which always read up to a page/cluster boundary). */
			if (f->position % cluster_bytes == 0 && f->position < f->file_size)
			{
				u_int32_t next = fat_cluster_next(fs, f->cur_cluster);
				if (next == FAT_CLUSTER_EOC || next < 2)
				{
					if (remaining > 0)
						break;
				}
				else
				{
					f->cur_cluster = next;
				}
			}
			continue;
		}

		/* Slow path: partial sector at start/end. */
		{
			u_int32_t can_copy = 512 - byte_off;
			if (can_copy > remaining)
				can_copy = remaining;

			if (fat_sector_read(fs, lba, sector_buf) != 0)
				break;

			memcpy(dst + bytes_read, sector_buf + byte_off, can_copy);
			bytes_read += can_copy;
			remaining -= can_copy;
			f->position += can_copy;

			/* Advance cluster if we landed on a boundary (see the fast-path
			 * note): keep cur_cluster consistent with position even at
			 * remaining == 0 so the next read/seek isn't off by one cluster. */
			if (f->position % cluster_bytes == 0 && f->position < f->file_size)
			{
				u_int32_t next = fat_cluster_next(fs, f->cur_cluster);
				if (next == FAT_CLUSTER_EOC || next < 2)
				{
					if (remaining > 0)
						break;
				}
				else
				{
					f->cur_cluster = next;
				}
			}
		}
	}

	if (got)
		*got = bytes_read;
	return (0);
}

int fat_file_write(struct fat_file *f, const void *buf, u_int32_t size)
{
	struct fat_fs *fs = f->fs;
	u_int32_t cluster_bytes = (u_int32_t)fs->sectors_per_cluster * fs->bytes_per_sector;
	const u_int8_t *src = (const u_int8_t *)buf;
	u_int32_t written = 0;

	while (written < size)
	{
		u_int32_t offset_in_cluster = f->position % cluster_bytes;
		u_int32_t sec_in_clust = offset_in_cluster / 512;
		u_int32_t byte_off = offset_in_cluster % 512;
		u_int32_t lba = fat_cluster_to_lba(fs, f->cur_cluster) + sec_in_clust;
		u_int32_t can_write = 512 - byte_off;

		if (can_write > size - written)
			can_write = size - written;

		if (byte_off == 0 && can_write == 512)
		{
			/*
			 * Full-sector write: bypass the file buffer.
			 * If our buffer happens to hold this sector,
			 * discard it — we are about to overwrite it.
			 */
			if (f->buf_lba == lba)
				f->buf_dirty = 0;
			if (fat_sector_write(fs, lba, src + written) != 0)
				return (-1);
		}
		else
		{
			/* Partial write: use the per-file write buffer. */
			if (f->buf_lba != lba)
			{
				/* Evict old buffer slot. */
				if (f->buf_dirty)
				{
					if (fat_sector_write(fs, f->buf_lba, f->buf) != 0)
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
		written += can_write;

		if (f->position > f->file_size)
		{
			f->file_size = f->position;
			f->size_dirty = 1;
		}

		/* Cross cluster boundary: follow or allocate. */
		if (written < size && f->position % cluster_bytes == 0)
		{
			u_int32_t next = fat_cluster_next(fs, f->cur_cluster);
			if (next == FAT_CLUSTER_EOC || next < 2)
			{
				next = fat_cluster_alloc(fs, f->cur_cluster);
				if (next == 0)
					return (-1);
			}
			f->cur_cluster = next;
		}
	}

	return (0);
}

int fat_file_flush(struct fat_file *f)
{
	if (f->buf_dirty)
	{
		if (fat_sector_write(f->fs, f->buf_lba, f->buf) != 0)
			return (-1);
		f->buf_dirty = 0;
	}
	if (f->size_dirty)
	{
		if (fat_dir_update_size(f->fs, f->dir_sector, f->dir_offset, f->file_size) != 0)
			return (-1);
		f->size_dirty = 0;
	}
	return (0);
}

int fat_file_truncate(struct fat_file *f, u_int32_t new_size)
{
	struct fat_fs *fs = f->fs;
	u_int32_t cluster_bytes = (u_int32_t)fs->sectors_per_cluster * fs->bytes_per_sector;
	u_int32_t c, next;

	/* Make sure any dirty data is on disk before we mutate the chain — we
	 * don't want a buffered sector for a cluster we are about to free. */
	if (f->buf_dirty)
	{
		if (fat_sector_write(fs, f->buf_lba, f->buf) != 0)
			return (-1);
		f->buf_dirty = 0;
	}
	f->buf_lba = (u_int32_t)-1;

	/* Growing: just bump the tracked size; subsequent writes fill the gap. */
	if (new_size >= f->file_size)
	{
		f->file_size = new_size;
		f->size_dirty = 1;
		return (fat_file_flush(f));
	}

	/* Shrinking to zero: free everything and allocate one fresh cluster
	 * so the file still has a valid start_cluster for future writes. */
	if (new_size == 0)
	{
		if (f->start_cluster >= 2)
			fat_cluster_free_chain(fs, f->start_cluster);
		f->start_cluster = fat_cluster_alloc(fs, 0);
		if (f->start_cluster == 0)
			return (-1);
		f->cur_cluster = f->start_cluster;
		f->position = 0;
		f->file_size = 0;
		f->size_dirty = 0;
		return (dirent_set_clus_size(fs, f->dir_sector, f->dir_offset, f->start_cluster, 0));
	}

	/* Shrinking but keeping data: walk to the cluster that holds the
	 * last byte we keep (byte at offset new_size - 1), terminate the
	 * chain there, free the tail. */
	{
		u_int32_t target_idx = (new_size - 1) / cluster_bytes;
		c = f->start_cluster;
		for (u_int32_t i = 0; i < target_idx; i++)
		{
			next = fat_cluster_next(fs, c);
			if (next == FAT_CLUSTER_EOC || next < 2)
				return (-1);
			c = next;
		}
	}
	next = fat_cluster_next(fs, c);
	if (fat_cluster_write_entry(fs, c, FAT_CLUSTER_EOC) != 0)
		return (-1);
	if (next != FAT_CLUSTER_EOC && next >= 2)
	{
		if (fat_cluster_free_chain(fs, next) != 0)
			return (-1);
	}

	f->file_size = new_size;
	f->size_dirty = 1;
	if (f->position > new_size)
	{
		f->position = new_size;
		/* Force seek_to_cluster on next read/write by resetting the
		 * tracked current cluster. */
		f->cur_cluster = c;
	}
	return (fat_file_flush(f));
}

void fat_file_close(struct fat_file *f)
{
	fat_file_flush(f);
	kfree(f);
}
