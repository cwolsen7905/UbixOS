# UbixOS VMM Overhaul Plan

## Status

| # | Task | Phase | Status |
|---|------|-------|--------|
| 1.1 | Red-black tree for `vm_map` entries (VMA lookup) | 1 | ⬜ Not started |
| 1.2 | `vm_map_entry` struct — replace linear VMA list | 1 | ⬜ Not started |
| 1.3 | O(log n) `mmap`/`munmap`/page-fault VMA lookup | 1 | ⬜ Not started |
| 2.1 | Lazy page allocation (demand-zero pages) | 2 | 🔄 Partial — page fault handler demand-zeroes data/stack ranges; `sys_mmap(MAP_ANON)` still pre-allocates |
| 2.2 | File-backed `mmap` (shared libraries, executables) | 2 | ⬜ Not started — current mmap(fd) reads file into pre-allocated pages, no VMA backing |
| 2.3 | `msync` — flush dirty file-backed pages | 2 | ⬜ Not started |
| 3.1 | Swap device integration — page out to swap partition | 3 | ✅ Done — `sys/vmm/swap.c`: slot bitmap, `swap_write/read_page`, `swap_evict_page` (clock); page fault handler handles `PAGE_SWAPPED` PTEs |
| 3.2 | Pageout daemon — proactive page reclaim | 3 | 🔄 Partial — `swap_evict_page` clock algorithm exists but no background task calls it; eviction only triggered reactively on OOM in `vmm_findFreePage` |
| 3.3 | `madvise` hints (MADV_SEQUENTIAL, MADV_DONTNEED) | 3 | ⬜ Not started |

**Legend:** ⬜ Not started · 🔄 In progress · ✅ Done · ⏸ Blocked

**Prerequisites:** Dynamic linking plan complete (mmap2/munmap/mprotect done ✅).
Phase 2 (file-backed mmap) needs the dynamic linker working end-to-end first.

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

`vm_map_entry` already has `vm_vnode` + `vm_offset`. When a page fault hits
a file-backed VMA:
1. Allocate a physical page.
2. Read `PAGE_SIZE` bytes from `vm_vnode` at `vm_offset + (fault_addr - vm_start)`.
3. Map the page into the faulting process's address space.

This is the foundation for executing shared libraries without copying them
into anonymous memory — the dynamic linker's `PT_LOAD` segments map directly
from the ELF file.

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
