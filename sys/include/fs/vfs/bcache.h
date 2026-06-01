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
 * 256 fixed slots × 512 bytes = 128 KB static footprint.
 * Clock-hand eviction.  On a read miss, up to BCACHE_PREFETCH_SECS
 * contiguous sectors are fetched in a single device call and loaded
 * into consecutive cache slots, so sequential file reads hit cache
 * after the first miss per cluster.
 *
 * Write-through: bcache_write updates the matching slot (if present)
 * and immediately issues one device write.
 *
 * All operations are serialised by the caller (e.g. per-mount spinlock).
 * The cache itself contains no locks.
 */

#define BCACHE_SLOTS		256
#define BCACHE_PREFETCH_SECS	1	/* one sector per miss — safe for all devices */

void	bcache_init(void);

/*
 * Read sector <lba> from <dev> into <buf> (512 bytes).
 * Returns 0 on success, -1 on device error.
 */
int	bcache_read(struct ubx_device *dev, u_int32_t lba, void *buf);

/*
 * Write 512 bytes from <buf> to sector <lba> on <dev>.
 * Updates any matching cache slot, then writes through to device.
 * Returns 0 on success, -1 on device error.
 */
int	bcache_write(struct ubx_device *dev, u_int32_t lba, const void *buf);

/*
 * Invalidate all cache slots belonging to <dev>.
 * Call on unmount.
 */
void	bcache_invalidate(struct ubx_device *dev);

#endif /* _VFS_BCACHE_H */
