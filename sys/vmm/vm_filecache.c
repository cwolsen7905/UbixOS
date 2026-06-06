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
 * Shared file-page cache — see vm_filecache.h for the contract.
 *
 * Entries are indexed two ways: by key (mp, fileid, offset) for the mmap path,
 * and by physical page for the teardown/fork path.  An entry is linked into
 * both hash chains and carries a single refcount that owns its physical page.
 * One yielding spinlock guards the whole structure.
 */

#include <vmm/vm_filecache.h>
#include <vmm/vmm.h>
#include <lib/kmalloc.h>
#include <ubixos/spinlock.h>
#include <sys/types.h>

#define VM_FILECACHE_BUCKETS 256u

struct vm_filecache_entry
{
	void *mp;                             /* mount-point identity token */
	u_int32_t fileid;                     /* per-file id (start cluster/block) */
	off_t offset;                         /* page-aligned byte offset into file */
	u_int32_t phys;                       /* physical page address (page-aligned) */
	u_int32_t refcnt;                     /* live mappings of this page */
	struct vm_filecache_entry *key_next;  /* by-key bucket chain */
	struct vm_filecache_entry *phys_next; /* by-phys bucket chain */
};

static struct vm_filecache_entry *g_key_buckets[VM_FILECACHE_BUCKETS];
static struct vm_filecache_entry *g_phys_buckets[VM_FILECACHE_BUCKETS];
static struct spinLock g_filecache_lock = SPIN_LOCK_INITIALIZER;
static u_int32_t g_filecache_count = 0; /* live entries (== shared pages) */

u_int32_t vm_filecache_page_count(void)
{
	return (g_filecache_count);
}

/** Hash a (mp, fileid, offset) key into a bucket index. */
static inline u_int32_t key_hash(void *mp, u_int32_t fileid, off_t offset)
{
	u_int32_t h = fileid * 2654435761u; /* Knuth multiplicative */
	h ^= (u_int32_t)((u_int64_t)offset >> 12);
	h ^= (u_int32_t)(uintptr_t)mp >> 4;
	return (h % VM_FILECACHE_BUCKETS);
}

/** Hash a physical page address into a bucket index. */
static inline u_int32_t phys_hash(u_int32_t phys)
{
	return (((phys >> 12) * 2654435761u) % VM_FILECACHE_BUCKETS);
}

/** Find the entry for a key, or NULL.  Caller holds g_filecache_lock. */
static struct vm_filecache_entry *find_key(void *mp, u_int32_t fileid, off_t offset)
{
	struct vm_filecache_entry *e;

	for (e = g_key_buckets[key_hash(mp, fileid, offset)]; e != NULL; e = e->key_next)
	{
		if (e->mp == mp && e->fileid == fileid && e->offset == offset)
			return (e);
	}
	return (NULL);
}

/** Find the entry owning a physical page, or NULL.  Caller holds the lock. */
static struct vm_filecache_entry *find_phys(u_int32_t phys)
{
	struct vm_filecache_entry *e;

	for (e = g_phys_buckets[phys_hash(phys)]; e != NULL; e = e->phys_next)
	{
		if (e->phys == phys)
			return (e);
	}
	return (NULL);
}

u_int32_t vm_filecache_lookup_ref(void *mp, u_int32_t fileid, off_t offset)
{
	struct vm_filecache_entry *e;
	u_int32_t phys = 0;

	spinLock(&g_filecache_lock);
	e = find_key(mp, fileid, offset);
	if (e != NULL)
	{
		e->refcnt++;
		phys = e->phys;
	}
	spinUnlock(&g_filecache_lock);
	return (phys);
}

int vm_filecache_insert(void *mp, u_int32_t fileid, off_t offset, u_int32_t phys, u_int32_t *phys_out)
{
	struct vm_filecache_entry *e;
	u_int32_t kb, pb;

	spinLock(&g_filecache_lock);

	/* Lost a race — another mapper inserted this key first.  Adopt it. */
	e = find_key(mp, fileid, offset);
	if (e != NULL)
	{
		e->refcnt++;
		if (phys_out != NULL)
			*phys_out = e->phys;
		spinUnlock(&g_filecache_lock);
		return (-1);
	}

	e = kmalloc(sizeof(*e));
	if (e == NULL)
	{
		spinUnlock(&g_filecache_lock);
		return (-1);
	}

	e->mp = mp;
	e->fileid = fileid;
	e->offset = offset;
	e->phys = phys;
	e->refcnt = 1;

	kb = key_hash(mp, fileid, offset);
	e->key_next = g_key_buckets[kb];
	g_key_buckets[kb] = e;

	pb = phys_hash(phys);
	e->phys_next = g_phys_buckets[pb];
	g_phys_buckets[pb] = e;

	g_filecache_count++;

	spinUnlock(&g_filecache_lock);
	return (0);
}

int vm_filecache_ref_phys(u_int32_t phys)
{
	struct vm_filecache_entry *e;
	int found;

	spinLock(&g_filecache_lock);
	e = find_phys(phys);
	found = (e != NULL);
	if (e != NULL)
		e->refcnt++;
	spinUnlock(&g_filecache_lock);
	return (found);
}

int vm_filecache_unref_phys(u_int32_t phys)
{
	struct vm_filecache_entry *e, **pp;
	u_int32_t free_phys = 0;
	int found = 0;

	spinLock(&g_filecache_lock);

	e = find_phys(phys);
	if (e != NULL)
	{
		found = 1;
		if (--e->refcnt == 0)
		{
			/* Unlink from both hash chains. */
			for (pp = &g_phys_buckets[phys_hash(phys)]; *pp != NULL; pp = &(*pp)->phys_next)
			{
				if (*pp == e)
				{
					*pp = e->phys_next;
					break;
				}
			}
			for (pp = &g_key_buckets[key_hash(e->mp, e->fileid, e->offset)]; *pp != NULL;
			     pp = &(*pp)->key_next)
			{
				if (*pp == e)
				{
					*pp = e->key_next;
					break;
				}
			}
			free_phys = e->phys;
			kfree(e);
			g_filecache_count--;
		}
	}

	spinUnlock(&g_filecache_lock);

	if (free_phys != 0)
		free_page(free_phys);

	return (found);
}
