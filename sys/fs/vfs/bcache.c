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

#include <fs/vfs/bcache.h>
#include <ubixos/sched.h>
#include <string.h>

#define BCACHE_SEC_BYTES	512

/*
 * bcache has its own spinlock, independent of the per-mount fat lock.
 * This allows two volumes (sys: and USB:) to perform FAT metadata reads
 * concurrently.  The lock is held across the device I/O on a miss so that
 * the shared _prefetch_buf is not raced; the miss path is uncommon after
 * the first access to each FAT/directory sector.
 */
static volatile int		_bcache_lock;
static u_int8_t			_prefetch_buf[BCACHE_PREFETCH_SECS * BCACHE_SEC_BYTES];

static void
bcache_acquire(void)
{
	while (__sync_lock_test_and_set(&_bcache_lock, 1))
		sched_yield();
}

static void
bcache_release(void)
{
	__sync_lock_release(&_bcache_lock);
}

/* ------------------------------------------------------------------ */
/* Slot table                                                          */
/* ------------------------------------------------------------------ */

struct bcache_slot {
	struct ubx_device	*dev;
	u_int32_t		 lba;
	u_int8_t			 valid;
	u_int8_t			 ref;		/* clock second-chance bit */
	u_int8_t			 data[BCACHE_SEC_BYTES];
};

static struct bcache_slot	_slots[BCACHE_SLOTS];
static int			_clock_hand;

/* ------------------------------------------------------------------ */
/* Internal helpers (called with _bcache_lock held)                    */
/* ------------------------------------------------------------------ */

static int
slot_find(struct ubx_device *dev, u_int32_t lba)
{
	int	i;

	for (i = 0; i < BCACHE_SLOTS; i++) {
		if (_slots[i].valid && _slots[i].dev == dev &&
		    _slots[i].lba == lba)
			return (i);
	}
	return (-1);
}

/* Clock eviction: return index of a slot safe to reuse. */
static int
slot_evict(void)
{
	int	tries;

	for (tries = 0; tries < BCACHE_SLOTS * 2; tries++) {
		struct bcache_slot *s = &_slots[_clock_hand];
		if (!s->valid || !s->ref) {
			int victim = _clock_hand;
			_clock_hand = (_clock_hand + 1) % BCACHE_SLOTS;
			s->valid = 0;
			return (victim);
		}
		s->ref = 0;
		_clock_hand = (_clock_hand + 1) % BCACHE_SLOTS;
	}
	/* All slots referenced — force-evict current position. */
	{
		int victim = _clock_hand;
		_slots[victim].valid = 0;
		_clock_hand = (_clock_hand + 1) % BCACHE_SLOTS;
		return (victim);
	}
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void
bcache_init(void)
{
	memset(_slots, 0, sizeof(_slots));
	_clock_hand  = 0;
	_bcache_lock = 0;
}

int
bcache_read(struct ubx_device *dev, u_int32_t lba, void *buf)
{
	int	 idx;

	bcache_acquire();

	idx = slot_find(dev, lba);
	if (idx >= 0) {
		_slots[idx].ref = 1;
		memcpy(buf, _slots[idx].data, BCACHE_SEC_BYTES);
		bcache_release();
		return (0);
	}

	/*
	 * Cache miss.  Read one sector into the slot.  A single-sector policy
	 * is safe for all device types (USB bulk transfers stall on multi-sector
	 * reads if the UHCI driver's timeout is tight).  The 256-slot table
	 * provides caching across FAT-chain walks without thrashing.
	 */
	if (dev->dev_blk_ops->read(dev, lba, 1, _prefetch_buf) != 0) {
		bcache_release();
		return (-1);
	}

	/* n == 1: store the single sector that was read. */
	idx = slot_find(dev, lba);
	if (idx >= 0) {
		_slots[idx].ref = 1;
		memcpy(_slots[idx].data, _prefetch_buf, BCACHE_SEC_BYTES);
	} else {
		idx = slot_evict();
		_slots[idx].dev   = dev;
		_slots[idx].lba   = lba;
		_slots[idx].valid = 1;
		_slots[idx].ref   = 1;
		memcpy(_slots[idx].data, _prefetch_buf, BCACHE_SEC_BYTES);
	}
	memcpy(buf, _slots[idx].data, BCACHE_SEC_BYTES);
	bcache_release();
	return (0);
}

int
bcache_write(struct ubx_device *dev, u_int32_t lba, const void *buf)
{
	int	idx;
	int	r;

	bcache_acquire();
	idx = slot_find(dev, lba);
	if (idx >= 0) {
		memcpy(_slots[idx].data, buf, BCACHE_SEC_BYTES);
		_slots[idx].ref = 1;
	}
	bcache_release();

	/* Write-through: flush to device outside the cache lock. */
	r = dev->dev_blk_ops->write(dev, lba, 1, (void *)(uintptr_t)buf);
	return (r);
}

void
bcache_invalidate(struct ubx_device *dev)
{
	int	i;

	bcache_acquire();
	for (i = 0; i < BCACHE_SLOTS; i++)
		if (_slots[i].valid && _slots[i].dev == dev)
			_slots[i].valid = 0;
	bcache_release();
}
