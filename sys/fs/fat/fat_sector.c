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
#include <string.h>
#include <fs/fat/fat_sector.h>

/*
 * Single-slot write-behind cache.  All access is serialised by the caller
 * holding fat_acquire(); no internal locking is needed here.
 */
static uint8_t	 _cache_buf[512];
static uint32_t	 _cache_lba   = (uint32_t)-1;
static uint8_t	 _cache_dirty = 0;

int
fat_sector_flush(struct fat_fs *fs)
{
	if (!_cache_dirty)
		return (0);
	if (fs->mp->device->dev_blk_ops->write(fs->mp->device,
	    _cache_lba, 1, _cache_buf) != 0) {
		kprintf("fat_sector: flush write failed lba=%u\n", _cache_lba);
		return (-1);
	}
	_cache_dirty = 0;
	return (0);
}

int
fat_sector_read(struct fat_fs *fs, uint32_t lba, void *buf)
{
	if (lba != _cache_lba) {
		if (fat_sector_flush(fs) != 0)
			return (-1);
		if (fs->mp->device->dev_blk_ops->read(fs->mp->device,
		    lba, 1, _cache_buf) != 0) {
			kprintf("fat_sector: read failed lba=%u\n", lba);
			return (-1);
		}
		_cache_lba = lba;
	}
	memcpy(buf, _cache_buf, 512);
	return (0);
}

int
fat_sector_write(struct fat_fs *fs, uint32_t lba, const void *buf)
{
	/* Evict a dirty cache line that belongs to a different sector. */
	if (_cache_lba != lba && _cache_dirty) {
		if (fat_sector_flush(fs) != 0)
			return (-1);
	}
	memcpy(_cache_buf, buf, 512);
	_cache_lba   = lba;
	_cache_dirty = 1;
	/* Flush immediately so callers do not need to call fat_sector_flush. */
	return (fat_sector_flush(fs));
}
