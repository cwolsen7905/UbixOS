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

#ifndef _VFS_BCACHE_H
#define _VFS_BCACHE_H

#include <sys/types.h>
#include <sys/bus.h>

/*
 * VFS block buffer cache.
 *
 * BCACHE_SLOTS fixed 512-byte slots (4 MB), found through a hash on
 * (device, lba), with clock-hand eviction.  A multi-sector read that misses
 * fetches the whole run in one device call; only the sectors the caller asked
 * for are cached (no speculative read-ahead), so a write that bypasses the
 * cache can only leave behind a sector someone already read.
 *
 * Write-through: bcache_write updates the matching slot (if present) and
 * immediately writes the device.
 *
 * The cache serialises itself with one internal lock, held across the device
 * read on a miss.
 */

#define BCACHE_SLOTS 8192
#define BCACHE_HASH 4096 /* hash buckets; a power of two */
#define BCACHE_SEC_BYTES 512

void bcache_init(void);

/* Read sector @lba of @dev into @buf (512 bytes).  0, or -1 on a device error. */
int bcache_read(struct ubx_device *dev, u_int32_t lba, void *buf);

/* Read @count sectors from @lba into @buf; a miss reads the run in one device
 * call.  0, or -1 on a device error. */
int bcache_read_multi(struct ubx_device *dev, u_int32_t lba, u_int32_t count, void *buf);

/* Write 512 bytes from @buf to sector @lba: update a cached copy, then write the
 * device.  0, or -1 on a device error. */
int bcache_write(struct ubx_device *dev, u_int32_t lba, const void *buf);

/* Write @count sectors from @buf at @lba in one device call (write-through). */
int bcache_write_multi(struct ubx_device *dev, u_int32_t lba, u_int32_t count, const void *buf);

/* Invalidate all cache slots belonging to @dev.  Call on unmount. */
void bcache_invalidate(struct ubx_device *dev);

#endif /* _VFS_BCACHE_H */
