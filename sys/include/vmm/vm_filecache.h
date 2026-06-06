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

#ifndef _VMM_VM_FILECACHE_H_
#define _VMM_VM_FILECACHE_H_

#include <sys/types.h>

/*
 * Shared file-page cache.
 *
 * A read-only file page is identified by (mount point, file id, page-aligned
 * byte offset).  The first process to mmap that page allocates a physical page
 * and inserts it; every later mapper of the same page shares that one physical
 * page (mapped read-only with PAGE_SHARED).  This makes a shared library's
 * text/rodata shared across all processes instead of copied per-process
 * (VMM plan Phase 2.2).
 *
 * Lifetime is by **physical page refcount**, kept in the cache entry.  The
 * mmap path refs by key (lookup_ref / insert); process teardown and fork
 * ref/unref by physical address at the spots that already special-case
 * PAGE_SHARED pages.  When the last reference drops, the physical page is
 * freed and the entry removed, so a later lookup never returns a stale page.
 *
 * `mp` is an opaque `struct vfs_mountPoint *` identity token; `fileid` is a
 * stable per-file id (the file's start cluster/block).  Neither is dereferenced.
 */

/**
 * Look up the cached page for (mp, fileid, offset) and, on a hit, take one
 * reference (for a new mapping).  Returns the physical page, or 0 if absent.
 */
u_int32_t vm_filecache_lookup_ref(void *mp, u_int32_t fileid, off_t offset);

/**
 * Insert a freshly read page for (mp, fileid, offset) → phys with refcount 1.
 * If another mapper inserted the same key first (race), takes a reference on
 * the existing entry instead and returns -1 so the caller frees its own @phys.
 * Returns 0 if @phys was inserted (caller's page is now owned by the cache),
 * -1 if a duplicate existed (caller must free @phys), or -1 on OOM.
 *
 * @param phys_out  on a duplicate, receives the winning entry's physical page
 *                  so the caller can map that instead; unchanged on success.
 */
int vm_filecache_insert(void *mp, u_int32_t fileid, off_t offset, u_int32_t phys, u_int32_t *phys_out);

/**
 * Add one reference to the cache entry that owns physical page @phys.
 * No-op if @phys is not a cached page (e.g. a vmm_share_region page).
 * Used by fork when a PAGE_SHARED page is shared into the child.
 *
 * @return 1 if @phys was a cached page (the cache refcount was bumped),
 *         0 if @phys was not in the cache (caller handles its own refcount).
 */
int vm_filecache_ref_phys(u_int32_t phys);

/**
 * Drop one reference from the cache entry that owns @phys; frees the page and
 * removes the entry when the count reaches zero.  No-op if @phys is not cached
 * (so it is safe to call on every PAGE_SHARED page during teardown — a
 * vmm_share_region page simply isn't found).
 *
 * @return 1 if @phys was a cached page (caller must NOT free it itself),
 *         0 if @phys was not in the cache.
 */
int vm_filecache_unref_phys(u_int32_t phys);

/** Number of live cache entries (== shared physical pages held). For /proc/meminfo. */
u_int32_t vm_filecache_page_count(void);

#endif /* _VMM_VM_FILECACHE_H_ */
