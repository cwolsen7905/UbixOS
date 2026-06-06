# UbixOS VMM Overhaul Plan

## Status

| # | Task | Phase | Status |
|---|------|-------|--------|
| 1.1 | Red-black tree for `vm_map` entries (VMA lookup) | 1 | ✅ Done — `sys/lib/rbtree.c` + `sys/include/lib/rbtree.h`; intrusive RB tree with insert/erase/find/first/next |
| 1.2 | `vm_map_entry` struct — replace linear VMA list | 1 | ✅ Done — `sys/include/vmm/vm_map.h` + `sys/vmm/vm_map.c`; `vm_map_t vm_map` embedded in `kTask_t`; fork copies, exec frees |
| 1.3 | O(log n) `mmap`/`munmap`/page-fault VMA lookup | 1 | ✅ Done — `vm_map_insert` in `sys_mmap` (anon paths); `vm_map_remove` in `sys_munmap`; `vm_map_lookup` in page fault handler for demand-zero of anon VMAs |
| 2.1 | Lazy page allocation (demand-zero pages) | 2 | ✅ Done — `sys_mmap(MAP_ANON)` now records VMA only (no physical pages); `vmm_reserve_anon_range` finds free VA without mapping; page fault handler backs pages on first touch; covers both MAP_FIXED and non-fixed anon; PT-missing case handled in `pageDir == 0` fault branch |
| 2.2 | File-backed `mmap` (shared libraries, executables) | 2 | ✅ **Demand-paged DONE** (`89652e853`). `mmap(fd)` maps no pages: it opens a private backing fd (by path, survives caller `close`), reserves the VA, trims overlapping VMAs (proper `MAP_FIXED` replace), and records a `VM_MAP_FILE` VMA. The fault handler (`vmm_demand_file_page`) reads each page on first touch — RO pages de-duplicated via the shared file-page cache (`sys/vmm/vm_filecache.c`, keyed by mount/`fd->ino`/offset; `PAGE_SHARED` + per-page refcount, one physical copy across processes), writable pages private. Backing fds `fclose`d on teardown, re-`fopen`d on fork. Eager read + de-dup retained as a fallback when no backing fd opens. Fault-time read is safe because the IDE driver polls (never sleeps). Stage A (`70e8df341`) added the file VMAs + overlap trimming; depended on the FAT random-access fix (`c975ef93f`) since per-page cluster-boundary reads exposed a latent stale-`cur_cluster` bug. **Optional remaining:** COW/writeback for `MAP_SHARED` writable file pages. |
| 2.3 | `msync` — flush dirty file-backed pages | 2 | ✅ Done (`fd4a9a5c1`). `msync`/`munmap` flush dirty `MAP_SHARED` writable file pages to the backing file via `vfsWrite` and clear the PTE dirty bit; the demand handler clears dirty after the read so only app-modified pages write back; last page clamped to file size; `sys_mmap2` page-offset masked to 32 bits. Write-back semantics (private writable copy flushed to disk), not cross-process live-shared pages. Verified by `bin/vmtest`. Exposed + fixed a write-side FAT cluster-boundary bug (`fee1c67b5`). |
| — | `/proc/meminfo` — total/free pages + `FileCache` count | 2 | ✅ Done (`2dad05bdc`) — userland memory readout; used to measure sharing/leaks |
| — | Labeled segfault report (pid/name, fault/eip/esp/cs/err, pde/pte, bracketing VMAs) | 2 | ✅ Done (`d63d9a8ee`) — `vmm_report_segfault`; distinguishes a VMM demand bug from a wild pointer |
| 3.1 | Swap device integration — page out to swap partition | 3 | ✅ Done — `sys/vmm/swap.c`: slot bitmap, `swap_write/read_page`, `swap_evict_page` (clock); page fault handler handles `PAGE_SWAPPED` PTEs |
| 3.2 | Pageout daemon — proactive page reclaim | 3 | ✅ Done — `sys/vmm/pageout.c`; polls every 100 ticks, iterates task list with CR3 switching, calls `swap_evict_page` per task until high watermark; launched from `kmain` at `QOS_BACKGROUND` |
| 3.3 | `madvise` hints (MADV_SEQUENTIAL, MADV_DONTNEED) | 3 | ⬜ Not started |

**Legend:** ⬜ Not started · 🔄 In progress · ✅ Done · ⏸ Blocked

**Prerequisites:** Dynamic linking plan complete (mmap2/munmap/mprotect done ✅).
Phase 2 (file-backed mmap) needs the dynamic linker working end-to-end first.

**Known open issue (surfaced 2026-06-04 via `/proc/meminfo`):** a ~51-page leak
per process lifecycle in the **private/anon teardown** path (free pages ratchet
down across open/close cycles while `FileCache` stays flat — so it is *not* the
file-page cache). A clean `malloc`+touch×2 repro did not reproduce it, so it is a
more specific pattern (e.g. `fread` into a lazy-anon page, or many VMAs). Not yet
localized; chase with `/proc/meminfo` before/after controlled process cycles.

**Other open items (2026-06-06):**
- **Shared-region leak.** `vmm_share_region` now marks the *source* pages
  `PAGE_SHARED` (fix for a physical use-after-free that rebooted the OS on
  logout, `0e695e4d3`), so shared frames are freed only when the owning process
  exits — a bounded leak (~3 MB per logout cycle). Proper fix: refcount via
  `cowCounter` (+1 on share; each side's unmap decrements; free at 0; teardown
  must distinguish file-cache `PAGE_SHARED` from share_region `PAGE_SHARED`).
- **`open(O_RDWR)` truncates on FAT** — ✅ FIXED (`21c055ed2`). Truncation is now
  driven by a `fileTrunc` flag (`O_TRUNC`/`"w"`), separate from write access;
  `O_RDWR` without `O_TRUNC` opens in place (`FAT_MODE_R`). mmap-editing an
  existing file the POSIX way now works. *Remaining minor deviation:* `O_WRONLY`
  without `O_TRUNC` still truncates (the `O_WRONLY|O_CREAT|O_TRUNC` callers rely
  on it) — left as-is to avoid changing write-from-scratch behaviour.
- **3.3 `madvise`** still not started (MADV_DONTNEED / MADV_SEQUENTIAL).
- **Cross-process `MAP_SHARED` coherence.** msync is write-back only; two
  processes mapping the same file do not see each other's writes live (each gets
  a private writable copy). A shared writable page cache is the larger follow-up.

---

## Overview

The current VMM uses a flat array/list for virtual memory area (VMA) tracking.
This works at low process complexity but becomes O(n) for every page fault,
`mmap`, and `munmap` as the number of mappings grows. The overhaul replaces
the VMA list with a red-black tree (same as FreeBSD `vm_map` and Linux `mm_struct`)
and adds lazy allocation, file-backed pages, and eventually swap.

---

## Phase 1 — Red-Black Tree for VMA Lookup

### Why a red-black tree here

VMAs are ordered, non-overlapping address ranges. Every page fault, `mmap`,
and `munmap` needs to find the VMA containing a given virtual address.
With a linear list this is O(n); with an RB tree keyed on `vm_start` it is
O(log n). For a process with hundreds of mappings (e.g. a dynamically-linked
binary with many shared libraries), this is a significant win.

This is the data structure FreeBSD uses in `vm_map` and Linux uses in
`mm_struct`. For UbixOS's fixed 32-level scheduler priorities we chose a
bitmask (O(1)) over an RB tree; but for VMAs the *continuous ordering*
requirement makes the RB tree the right tool.

### 1.1 Red-black tree implementation

Add `sys/lib/rbtree.c` + `sys/include/lib/rbtree.h` — a generic intrusive
RB tree (same pattern as FreeBSD `<sys/tree.h>` `RB_*` macros):

```c
/* sys/include/lib/rbtree.h */
struct rb_node {
    struct rb_node *rb_parent;
    struct rb_node *rb_left;
    struct rb_node *rb_right;
    int             rb_color;  /* RB_RED=0, RB_BLACK=1 */
};

struct rb_root { struct rb_node *rb_node; };

void  rb_insert(struct rb_root *, struct rb_node *,
                int (*cmp)(struct rb_node *, struct rb_node *));
void  rb_erase(struct rb_root *, struct rb_node *);
struct rb_node *rb_find(struct rb_root *, uintptr_t key,
                        int (*cmp_key)(struct rb_node *, uintptr_t));
struct rb_node *rb_first(struct rb_root *);
struct rb_node *rb_next(struct rb_node *);
```

### 1.2 `vm_map_entry` struct

```c
/* sys/include/vmm/vm_map.h */
typedef struct vm_map_entry {
    struct rb_node   rb;         /* embedded RB node — must be first */
    uintptr_t        vm_start;   /* first byte of range (page-aligned) */
    uintptr_t        vm_end;     /* first byte past range */
    uint32_t         vm_prot;    /* VM_PROT_READ | WRITE | EXEC */
    uint32_t         vm_flags;   /* VM_MAP_SHARED, VM_MAP_FIXED, ... */
    struct vnode    *vm_vnode;   /* NULL for anonymous, vnode for file-backed */
    off_t            vm_offset;  /* offset into vm_vnode (file-backed only) */
} vm_map_entry_t;

typedef struct vm_map {
    struct rb_root   vm_root;    /* RB tree of vm_map_entry, keyed on vm_start */
    uintptr_t        vm_min;     /* lowest allowed VA (usually 0x1000) */
    uintptr_t        vm_max;     /* highest allowed VA (usually 0xBFFF_F000) */
    uint32_t         vm_nentries;
} vm_map_t;
```

Each `kTask_t` / future `kProc_t` gains a `vm_map_t vm_map` replacing the
current `td.vm_taddr/vm_tsize/vm_daddr/vm_dsize` fields (which stay as
convenience accessors populated from the map at exec time).

### 1.3 VMA lookup at page fault and mmap

Page fault handler (`vmm/paging.c` `page_fault_handler`):
```c
/* O(log n) — replaces linear scan */
vm_map_entry_t *vma = vm_map_lookup(&_current->vm_map, fault_addr);
if (vma == NULL) { /* segfault */ ... }
```

`sys_mmap` / `sys_munmap`:
```c
vm_map_entry_t *e = kmalloc(sizeof *e);
e->vm_start = addr; e->vm_end = addr + len;
rb_insert(&task->vm_map.vm_root, &e->rb, vm_map_cmp);
```

`vm_map_lookup` walks the tree: if `fault_addr < node->vm_start` go left,
if `fault_addr >= node->vm_end` go right, otherwise return this node.

---

## Phase 2 — Lazy Allocation and File-Backed mmap

### 2.1 Demand-zero pages

Currently `mmap(ANON)` pre-allocates and maps a physical page. Change to:
- Record the VMA in the RB tree but **do not** allocate a physical page.
- On first access, page fault handler sees a present VMA with no PTE → 
  allocate a zero page and map it. 
- This is how all modern kernels work: `malloc` never touches physical
  memory until the page is first read/written.

### 2.2 File-backed mmap

**Shipped (eager + shared), 2026-06-04 — `63ec7852a`, fix `d63d9a8ee`.**

`sys_mmap(fd)` keeps the historical path — allocate the VA range and read the
whole file in one `fread` (syscall context, proven correct) — then
**de-duplicates read-only pages** through a shared file-page cache
(`sys/vmm/vm_filecache.c`):
- First mapper of a `(mount, fd->ino, offset)` page publishes it: its PTE is
  downgraded in place (`vmm_set_page_attributes`) to `PAGE_PRESENT|PAGE_USER|
  PAGE_SHARED` and the page is inserted into the cache (refcount 1).
- Later mappers `lookup_ref` the cache, drop their just-read copy, and map the
  shared physical page read-only — so a library's text/rodata is **one physical
  copy across all processes**.
- Writable pages stay private. Teardown (`vmm_unmap_page`,
  `vmm_clean_virtual_space`) and fork (`vmm_copy_virtual_space`) ref/unref the
  cache by physical page; the last release frees it.

**Critical caveat:** file mmaps insert **no** `vm_map` VMA. rtld maps a
library's anonymous BSS with `MAP_FIXED` *overlapping* the file segment; a
non-anon file VMA in the tree made `vm_map_lookup` return it instead of the
anon BSS VMA, so demand-zero was skipped and the first BSS write faulted
"not mapped" (deterministic SIGSEGV). File pages are eager (always present,
never demand-faulted) so they need no VMA — leaving the tree anon-only fixes it.

**Demand-paged (lazy) file mmap — DONE 2026-06-05 (`89652e853`).**
`sys_mmap(fd)` now maps **no** pages: it opens a private backing fd (re-opened by
path so it survives the caller closing its own fd), reserves the VA range, trims
overlapping VMAs (`vm_map_remove` — proper `MAP_FIXED` replace, e.g. rtld's anon
BSS over a file segment's tail), and records a `VM_MAP_FILE` VMA. The page-fault
handler's `vmm_demand_file_page()` reads each page on first touch:
- **Read-only** pages go through the shared file-page cache — a hit maps the one
  shared physical copy `PAGE_SHARED`; a miss reads the page, publishes it, and
  downgrades the live mapping to shared RO.
- **Writable** pages get a private copy (read in, mapped RW).

Backing fds are `fclose`d on VMA teardown (`vm_map_free`/`vm_map_remove`) and
re-`fopen`d per-VMA on fork (`vm_map_copy`). If a backing fd can't be opened,
`sys_mmap` falls back to the historical eager read + cache de-dup so the mapping
still works.

The fault-time read is safe **without** dropping `g_page_fault_spin_lock`: the
IDE driver (`sys/pci/hd.c`) is pure polling (never sleeps), and `fat_acquire`
only yields on contention — no hard deadlock in this yielding-lock kernel. This
also exposed and fixed a latent FAT random-access bug (`c975ef93f`): per-page
cluster-boundary reads left `cur_cluster` stale, which eager whole-file reads
never tripped.

**Optional remaining:** COW / writeback for `MAP_SHARED` writable file pages
(`MAP_PRIVATE` writable is already correct — private copy on fault).

### 2.3 `msync`

Flush dirty file-backed pages back to the vnode. Walk the VMA's PTEs, find
dirty bits, write back via VFS, clear dirty.

---

## Phase 3 — Swap and Page Reclaim

The swap partition is already recognised at boot (`ad0s2`, 64 MB).

### 3.1 Swap integration

- `swap_pager`: when `freePage` can't satisfy an allocation, pick a victim
  page (clock algorithm), write it to swap, record the swap slot in the
  PTE (using the "not-present + custom bits" encoding), free the physical
  page.
- On fault to a swapped PTE: read from swap slot, map page, clear swap bit.

### 3.2 Clock (second-chance) page reclaim

Walk the active page list. If the accessed bit (PTE bit 5) is set, clear it
and advance. If clear, the page is a reclaim candidate. Simpler than LRU,
same asymptotic behaviour for most workloads.

### 3.3 `madvise`

`MADV_SEQUENTIAL` — prefetch ahead on file-backed VMAs.
`MADV_DONTNEED` — immediately reclaim pages in range (useful for `malloc`
trim). Maps to `vmm_unmapPage` over the range without removing the VMA.

---

## Key Files

| File | Phase | Purpose |
|------|-------|---------|
| `sys/lib/rbtree.c` + `.h` | 1 | Generic intrusive RB tree |
| `sys/include/vmm/vm_map.h` | 1 | `vm_map_entry_t`, `vm_map_t` |
| `sys/vmm/vm_map.c` | 1 | `vm_map_lookup`, `vm_map_insert`, `vm_map_remove` |
| `sys/vmm/paging.c` | 1–2 | Page fault handler — VMA lookup, demand-zero |
| `sys/vmm/vmm_mmap.c` | 1–2 | `sys_mmap`/`sys_munmap` — populate RB tree |
| `sys/vmm/swap.c` _(new)_ | 3 | Swap pager — write/read swap slots |
| `sys/vmm/pageout.c` _(new)_ | 3 | Clock reclaim daemon |

## Effort Estimate

| Phase | Effort |
|-------|--------|
| 1 — RB tree + VMA struct | ~2 days |
| 2 — Lazy alloc + file-backed mmap | ~3 days |
| 3 — Swap + reclaim | ~1 week |
