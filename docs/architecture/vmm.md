# UbixOS Virtual Memory Manager

**Source:** `sys/vmm/`

---

## Memory Layout

Each process has a private 4 GB virtual address space: a shared identity-mapped
low 4 MB, per-process user space (4 MB – 3 GB), and a shared kernel-only top
1 GB. The full per-PDE breakdown, physical low-memory layout, and GDT selectors
live in the canonical map — see
**[i386-page-directory-map.md](i386-page-directory-map.md)**. This document
covers VMM *behaviour* (functions, COW, MMIO guards, fork PD re-sync).

---

## Key Functions

### `vmmInit()`

Top-level initialization entry point. Calls `vmmMemMapInit()` then `vmmPagingInit()`. Halts on failure.

### `vmmMemMapInit()`

Builds the physical page-frame map — a linked list of all available physical pages. Each entry tracks:

- Physical frame address
- Owning PID
- Reference count (for COW sharing)
- Status flags (`free`, `allocated`, `cow-pending`)

### `vmmPagingInit()`

Enables hardware paging. Sets up the kernel's initial page directory, identity-maps the lower 4 MB (PD[0], all 1024 PT entries — covers the kernel at `0x300000`), and remaps the physical frame bitmap from its physical staging location into the top 1 GB kernel virtual space at `VMM_MMAP_ADDR_PMODE = 0xC0800000`.

### `vmmCreateVirtualSpace(pid)`

Allocates and initializes a fresh page directory for `pid`. Returns the physical base address. The shared lower 4 MB identity map and top 1 GB kernel mappings are pre-installed; everything between 4 MB and 3 GB starts unmapped.

### `vmmCopyVirtualSpace(pid)`

Forks the address space of `pid` using copy-on-write (COW):

1. The entire 2 MB – 3 GB range is duplicated at the page-table level.
2. All pages in that range are marked read-only and COW-pending in both parent and child; no physical memory is copied.
3. On the first write to any shared page, a page fault fires.
4. The page-fault handler allocates a new physical frame, copies the content, clears the COW flag, and rewrites the faulting PTE to the new frame.

---

## Physical Memory Layout

```
0x00000000 – 0x0009FFFF   Conventional RAM (640 KB) — first 1 MB reserved (ISA)
0x000A0000 – 0x000FFFFF   VGA/ROM/BIOS — not RAM, never allocated
0x00100000 – 0x002FFFFF   RAM: old bitmap staging area; now free pages (former VMM_MMAP_ADDR_RMODE)
0x00300000 – ~0x00392000  Kernel image (text + rodata + data + BSS)
~0x00392000 – bitmap_end  Page bitmap: numPages × sizeof(mMap) bytes, placed at page_align(_end)
bitmap_end  – top of RAM  Free pages, managed by vmm_findFreePage
```

The bitmap physical base is computed at runtime by `vmm_memMapInit` as
`page_align_up(_end)` and stored in `vmm_bitmap_phys`. The remap loop in
`vmm_pagingInit` uses this variable to map bitmap pages into kernel virtual
space — no hardcoded physical address appears in either function.

With QEMU's default `-m 256`: bitmap = 1 MB, free pages start at ~`0x492000`.
With 4 GB RAM: bitmap = 16 MB, free pages start at ~`0x1492000`.

---

## MMIO Pages and the vmmMemoryMap Boundary

Physical addresses at or above `numPages × PAGE_SIZE` (≥ 256 MB with the default QEMU `-m 256`
configuration) are **MMIO** — framebuffer, PCI BARs, device registers, etc.  These frames have
**no entry** in `vmmMemoryMap` (the array only covers RAM frames 0–numPages-1).  Passing such an
address to any function that indexes into `vmmMemoryMap` computes an index in the billions,
then accesses `0xC0800000 + huge_offset`, which is unmapped — causing a triple fault.

Three code sites must guard against this:

| Site | Guard | Without it |
|------|-------|-----------|
| `copyvirtualspace.c` — COW loop | `(phys >> 12) >= numPages` → share PTE as-is, skip `adjustCowCounter` | COW counter corrupted for MMIO frame |
| `vmm_cleanVirtualSpace` (`paging.c`) — execve user-space teardown | `(phys >> 12) >= numPages` → clear PTE, skip `freePage` | `freePage(0xFD000000)` indexes `vmmMemoryMap[0xFD000]` → unmapped → kernel fault |
| `freePage` (`vmm_memory.c`) | Explicit bounds check — returns `-1` for out-of-range index | Silent out-of-bounds array write |

**MMIO detection idiom:**
```c
if ((phys >> 12) >= (uint32_t)numPages) {
    /* MMIO — do not touch vmmMemoryMap */
}
```

The framebuffer for a 1024×768×24 display at LFB=`0xFD000000` occupies 576 pages
(0xFD000000–0xFD240000).  These are mapped into the viewing process at virtual `0x10000000`
by `sys_mapfb` (syscall 43).  After a `fork`, these pages must be shared as-is — any attempt
to COW or free them corrupts the kernel.

---

## Kernel Page Directory Desync After Fork

`vmm_copyVirtualSpace` must **re-sync kernel PD entries (indices 770–1015) from the parent
AFTER all allocations are complete**, not before.

The reason: building the child's page tables calls `vmm_getFreeKernelPage` and
`vmm_getFreePage`, which may allocate new kernel pages and, in doing so, add new PDE entries
to the parent's kernel range (770–1015).  If the kernel PD entries are copied into the child
*before* these allocations, any newly-added parent PDE will have no corresponding entry in
the child — the child's PDE slot is zero.  The first time the child accesses that kernel
address range, it takes a page fault with no handler, and the kernel triple-faults.

**The fix** (already in `sys/vmm/copyvirtualspace.c`): the re-sync loop

```c
for (x = PD_INDEX(VMM_KERN_START); x <= PD_INDEX(VMM_KERN_END); x++)
    newPageDirectory[x] = parentPageDirectory[x];
```

runs **after** all `vmm_getFreeKernelPage`/`vmm_getFreePage` calls, just before the
PT_BASE_ADDR self-map page is filled in.  PD_BASE_ADDR and PT_BASE_ADDR (indices 768–769)
are below VMM_KERN_START (770) and are not overwritten by this loop.

---

## Page-Fault Handling

**Source:** `sys/vmm/page_fault.S`, `sys/vmm/pagefault.c`

x86 exception 14 (page fault) is routed to the VMM handler, which distinguishes three cases:

| Case | Condition | Action |
|------|-----------|--------|
| COW fault | Write to a shared COW page | Allocate new frame, copy, update PTE, resume |
| Demand-zero | First access to an allocated but unmapped page | Allocate zeroed frame, map, resume |
| Invalid access | Unmapped or protected region | Deliver `SIGSEGV` to faulting process |
