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

/** One cached sector. */
struct bcache_slot
{
	struct ubx_device *dev;
	u_int32_t lba;
	int32_t next; /* next slot in this hash bucket, -1 = end */
	u_int8_t valid;
	u_int8_t ref; /* clock second-chance bit */
	u_int8_t data[BCACHE_SEC_BYTES];
};

/* Guards the slot table and buckets; held across the device read on a miss. */
static volatile int g_bcache_lock;
static struct bcache_slot g_slots[BCACHE_SLOTS];
static int32_t g_bucket[BCACHE_HASH]; /* first slot per bucket, -1 = empty */
static int g_clock_hand;

/** Take the cache lock, yielding while another CPU holds it. */
static void bcache_acquire(void)
{
	while (__sync_lock_test_and_set(&g_bcache_lock, 1))
		sched_yield();
}

/** Release the cache lock. */
static void bcache_release(void)
{
	__sync_lock_release(&g_bcache_lock);
}

/** @return the hash bucket for (@dev, @lba). */
static u_int32_t bcache_hash(struct ubx_device *dev, u_int32_t lba)
{
	return ((lba ^ (u_int32_t)((uintptr_t)dev >> 4) * 0x9E3779B1u) & (BCACHE_HASH - 1));
}

/** @return the slot caching (@dev, @lba), or -1. */
static int slot_find(struct ubx_device *dev, u_int32_t lba)
{
	int32_t i;

	for (i = g_bucket[bcache_hash(dev, lba)]; i >= 0; i = g_slots[i].next)
		if (g_slots[i].dev == dev && g_slots[i].lba == lba)
			return (i);
	return (-1);
}

/** Remove valid slot @idx from its hash bucket and mark it invalid. */
static void slot_unlink(int idx)
{
	int32_t *pp = &g_bucket[bcache_hash(g_slots[idx].dev, g_slots[idx].lba)];

	while (*pp >= 0)
	{
		if (*pp == idx)
		{
			*pp = g_slots[idx].next;
			break;
		}
		pp = &g_slots[*pp].next;
	}
	g_slots[idx].valid = 0;
}

/** Clock eviction. @return a free slot index (unlinked, invalid). */
static int slot_evict(void)
{
	int tries, victim;

	for (tries = 0; tries < BCACHE_SLOTS * 2; tries++)
	{
		struct bcache_slot *s = &g_slots[g_clock_hand];

		victim = g_clock_hand;
		g_clock_hand = (g_clock_hand + 1) % BCACHE_SLOTS;
		if (!s->valid)
			return (victim);
		if (!s->ref)
		{
			slot_unlink(victim);
			return (victim);
		}
		s->ref = 0;
	}
	victim = g_clock_hand; /* every slot referenced: take the next one */
	g_clock_hand = (g_clock_hand + 1) % BCACHE_SLOTS;
	if (g_slots[victim].valid)
		slot_unlink(victim);
	return (victim);
}

/** Cache @data as sector (@dev, @lba), replacing any older copy. */
static void slot_store(struct ubx_device *dev, u_int32_t lba, const void *data)
{
	int idx = slot_find(dev, lba);

	if (idx < 0)
	{
		u_int32_t h = bcache_hash(dev, lba);

		idx = slot_evict();
		g_slots[idx].dev = dev;
		g_slots[idx].lba = lba;
		g_slots[idx].valid = 1;
		g_slots[idx].next = g_bucket[h];
		g_bucket[h] = idx;
	}
	g_slots[idx].ref = 1;
	memcpy(g_slots[idx].data, data, BCACHE_SEC_BYTES);
}

/**
 * Reset the cache to empty.
 */
void bcache_init(void)
{
	int i;

	memset(g_slots, 0, sizeof(g_slots));
	for (i = 0; i < BCACHE_HASH; i++)
		g_bucket[i] = -1;
	g_clock_hand = 0;
	g_bcache_lock = 0;
}

/**
 * Read @count sectors starting at @lba of @dev into @buf.
 *
 * If every sector is cached they are copied out; otherwise the whole run is read
 * from the device in one call (write-through keeps the device current, so cached
 * and device copies agree) and every sector of it is cached.
 *
 * @return 0 on success, -1 on a device error.
 */
int bcache_read_multi(struct ubx_device *dev, u_int32_t lba, u_int32_t count, void *buf)
{
	u_int8_t *out = buf;
	u_int32_t i;

	bcache_acquire();
	for (i = 0; i < count; i++)
		if (slot_find(dev, lba + i) < 0)
			break;
	if (i == count)
	{
		for (i = 0; i < count; i++)
		{
			int idx = slot_find(dev, lba + i);

			g_slots[idx].ref = 1;
			memcpy(out + (size_t)i * BCACHE_SEC_BYTES, g_slots[idx].data, BCACHE_SEC_BYTES);
		}
		bcache_release();
		return (0);
	}

	if (dev->dev_blk_ops->read(dev, lba, count, out) != 0)
	{
		bcache_release();
		return (-1);
	}
	for (i = 0; i < count; i++)
		slot_store(dev, lba + i, out + (size_t)i * BCACHE_SEC_BYTES);
	bcache_release();
	return (0);
}

/**
 * Read sector @lba of @dev into @buf (512 bytes).
 *
 * @return 0 on success, -1 on a device error.
 */
int bcache_read(struct ubx_device *dev, u_int32_t lba, void *buf)
{
	return (bcache_read_multi(dev, lba, 1, buf));
}

/**
 * Write @count sectors from @buf at @lba of @dev: refresh any cached copies, then
 * write the run through to the device in one call.
 *
 * @return 0 on success, -1 on a device error.
 */
int bcache_write_multi(struct ubx_device *dev, u_int32_t lba, u_int32_t count, const void *buf)
{
	const u_int8_t *in = buf;
	u_int32_t i;

	bcache_acquire();
	for (i = 0; i < count; i++)
	{
		int idx = slot_find(dev, lba + i);

		if (idx >= 0)
		{
			memcpy(g_slots[idx].data, in + (size_t)i * BCACHE_SEC_BYTES, BCACHE_SEC_BYTES);
			g_slots[idx].ref = 1;
		}
	}
	bcache_release();

	/* Write-through: flush to the device outside the cache lock. */
	return (dev->dev_blk_ops->write(dev, lba, count, (void *)(uintptr_t)buf));
}

/**
 * Write 512 bytes from @buf to sector @lba of @dev (write-through).
 *
 * @return 0 on success, -1 on a device error.
 */
int bcache_write(struct ubx_device *dev, u_int32_t lba, const void *buf)
{
	return (bcache_write_multi(dev, lba, 1, buf));
}

/**
 * Drop every cached sector of @dev (on unmount).
 */
void bcache_invalidate(struct ubx_device *dev)
{
	int i;

	bcache_acquire();
	for (i = 0; i < BCACHE_SLOTS; i++)
		if (g_slots[i].valid && g_slots[i].dev == dev)
			slot_unlink(i);
	bcache_release();
}
