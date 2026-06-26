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

---

## 64-bit demand paging (aarch64 / x86_64)

> The sections above describe the **i386** VMM (the frozen `releng/2` arch). The two
> 64-bit kernels add a **machine-independent VMA-tracked demand-paging layer** on top of
> the same physical allocator. Hardware is reached only through the `md_*` hooks in
> `<sys/elf_load.h>`, so the layer is identical on aarch64 and x86_64.

**VMA tree** — `sys/vmm/vm_map.c`. Each process owns a red-black tree (`_current->vm_map`)
of `vm_map_entry_t` regions, each `[vm_start, vm_end)` tagged `VM_MAP_FILE`
(`vm_vnode` = an owned `fileDescriptor`, `vm_offset` = file offset) or `VM_MAP_ANON`.
`vm_map_remove`/`vm_map_free` close a file VMA's backing fd on teardown; `vm_map_copy`
duplicates the tree for fork. *(`vm_map.c` was historically i386-only; it is now in both
64-bit generic source lists.)*

**Demand-fault resolver** — `sys/vmm/vmm_demand.c::vmm_demand_fault(aspace_root, far)`.
The arch fault handler calls it on a **not-present (translation)** fault. It looks up the
covering VMA; a `VM_MAP_FILE` page is read with `vfs_pread_locked` (the same
`vfs_io_lock` `fread` uses, so demand reads are SMP-safe against concurrent FS access),
an anon page is demand-zeroed; the frame is mapped with `md_map_user_page`. Returns 0
(retry the access) or -1 (no VMA → SIGSEGV). The first implementation maps **private**
pages; de-duplicating read-only file pages through `vm_filecache` is a planned
optimization.

**Demand `execve`** — `sys/kern/elf64_demand.c::elf64_load_demand`. For a **static
ET_EXEC**, `execve` reads only the ELF headers and records a file-backed VMA per PT_LOAD
(plus one eager page at the file/BSS boundary and an anon VMA for the BSS tail) instead of
eagerly mapping + copying every page. A dynamic image (ET_DYN / PT_INTERP) returns to the
eager `elf64_load` path. This is what lets a 100 MB binary (the on-device `clang`) start
without materializing all of it up front.

**Arch wiring.** aarch64 `sys/arch/aarch64/kern/exceptions.c` routes EL0 **and** EL1
translation faults (data + instruction aborts) to `vmm_demand_fault` before SIGSEGV;
`aarch64_exec_replace` demand-loads into a local VMA map installed only at the TTBR
switch (so a load failure leaves the old image intact). x86_64 `idt.c` wiring is pending.

## AArch64 address-space layout and the higher-half migration

The aarch64 kernel currently runs **entirely in TTBR0 (low VAs)** as a flat identity map
(`sys/arch/aarch64/vmm/mmu.c`: one L1 of 1 GB block descriptors, `EPD1=1` so TTBR1 is
unused). `pmap_create_user_space` memcpy's the kernel L1 so every address space maps the
kernel + peripherals; per-process user mappings are meant to live at **block 4 (4 GB) and
up** (`USER_L1_MIN`), with blocks 0–3 shared by pointer.

**Constraint:** a **static `ET_EXEC` linked at a low VA** (clang links at `0x200000`,
block 0) lands in the kernel-identity region; mapping it makes the table walker replace
shared 2 MB kernel-identity *blocks* with private L3 tables **in the shared kernel L1**,
corrupting the global kernel identity. PIE binaries avoid this because they load high.

**In progress:** the **higher-half migration** relocates the kernel + its identity/physmap
to **TTBR1**, freeing all of TTBR0 for user so static low-linked binaries work and each
TTBR0 space drops its kernel-identity copy. The hard part is unwinding the pervasive
`phys == virt` identity assumption (introducing a `PHYSMAP_BASE`). Full phased plan +
status: **`docs/design/aarch64-higher-half-plan.md`**. x86_64 is also kernel-low and will
follow the same migration.
