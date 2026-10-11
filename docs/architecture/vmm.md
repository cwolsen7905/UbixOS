# uBixOS Virtual Memory Manager

**Source:** `sys/vmm/` (machine-independent), `sys/arch/<arch>/vmm/` (machine-dependent),
`sys/include/<arch>/vmm_layout.h` (per-arch user VA layout)

master is 64-bit only. This document covers the two live architectures —
**aarch64** (primary) and **x86_64** (abstraction anchor). The 32-bit i386 VMM is
frozen on `releng/2` and is kept at the end as [labelled history](#history-the-i386-vmm).

---

## Structure

The VMM is split the same way as the rest of the kernel (see
[docs/standards/arch-parity.md](../standards/arch-parity.md)):

| Layer | Where | What |
|-------|-------|------|
| Physical allocator | `sys/vmm/vmm_memory.c` | Page-frame bitmap (`vmmMemoryMap`, one `vmm_page_info_t` per frame: owner pid, `cowCounter`), `vmm_find_free_page()` with a rotating scan cursor, `vmm_find_free_pages_contig()`, free-page accounting |
| User-region policy | `sys/vmm/vmm_uregion.c` | Arch-neutral anonymous `mmap` and `brk` policy, driven by per-arch page mappers |
| VMA tree | `sys/vmm/vm_map.c` | Per-process red-black tree of `vm_map_entry_t` (`VM_MAP_FILE` / `VM_MAP_ANON`) |
| Demand paging | `sys/vmm/vmm_demand.c` | `vmm_demand_fault()` — resolves not-present faults against the VMA tree |
| File-page cache | `sys/vmm/vm_filecache.c` | Shared, refcounted read-only file pages (one physical copy of a library's text across processes) |
| MAP_SHARED write-back | `sys/vmm/vmm_shmap.c` | Registry of writable `MAP_SHARED` file mappings, flushed on `munmap`/`msync`/exit |
| Page tables, faults, TLB | `sys/arch/<arch>/vmm/` and `sys/arch/x86_64/kern/usermode.c` | Everything that touches hardware translation tables |

The 64-bit kernels link only the files above from `sys/vmm/` (see
`AARCH64_GENERIC_SRCS` / `X86_64_GENERIC_SRCS` in the top-level `Makefile`). The rest of
`sys/vmm/` (`vmm_paging.c`, `vmm_copy_virtual_space.c`, `page_fault.S`, …) is the i386
implementation.

Hardware is reached from MI code only through the `md_*` hooks in `<sys/elf_load.h>`
(`md_map_user_page`, `md_map_user_page_shared`, `md_user_mapped`, `md_sync_icache`,
`md_phys_to_virt`), so the demand-paging and loader layers are identical on both arches.

---

## aarch64

### Translation setup

Defined in [mmu.c](../../sys/arch/aarch64/vmm/mmu.c) and
[pmap.c](../../sys/arch/aarch64/vmm/pmap.c):

| Property | Value |
|----------|-------|
| Granule / VA width | 4 KB pages, 39-bit VAs in both halves (`T0SZ = T1SZ = 25`), table walk starts at level 1 |
| Levels | L1 (1 GB per entry) → L2 (2 MB) → L3 (4 KB), 512 entries each |
| Physical address size | `IPS` = 40 bits |
| Memory attributes | `MAIR` attr0 = Normal write-back, attr1 = Device-nGnRE |
| TTBR1 (kernel) | `physmap_l1`: 1 GB block descriptors, reached at `PHYSMAP_BASE = 0xFFFFFF8000000000` |
| TTBR0 (user) | Per-process L1 root, `md.md_ttbr0` (stored as a physmap VA) |

The kernel image is linked at high VAs (`VIRT_OFFSET = 0xFFFFFF8000000000`, VMA = LMA +
`VIRT_OFFSET`, loaded at `KERNEL_PHYS`; see `sys/compile/ldscript.aarch64`) and executes
through the TTBR1 physmap. `AARCH64_PHYS_OF()` / `AARCH64_VIRT_OF()`
([bringup.h](../../sys/arch/aarch64/bringup.h)) convert between a physmap VA and its
physical frame; `md_phys_to_virt()` is `PHYSMAP_BASE + phys`. A low identity mapping is
still built in `l1_table` (TTBR0 at boot), and secondary CPUs reuse the same register
programming (`aarch64_mmu_enable_secondary`).

`pmap_create_user_space()` returns an **empty** L1: all of TTBR0 belongs to the process
(`USER_L1_MIN = 0`), so a static `ET_EXEC` linked at a low VA (e.g. `0x200000`) loads
without colliding with the kernel. The status and phasing of this migration are in
[docs/design/aarch64-higher-half-plan.md](../design/aarch64-higher-half-plan.md).
**TODO: verify** that doc's phase table is up to date with the code above (its
"Current state" section still describes the pre-migration identity map).

Software PTE bits (in the unused high bits of an L3 descriptor):

| Bit | Name | Meaning |
|-----|------|---------|
| 55 | `PTE_COW` | Frame shared read-only after fork; a write faults into `pmap_cow_fault` |
| 56 | `PTE_SHARED` | Shared file-cache page (read-only, refcounted by `vm_filecache`) |
| 57 | `PTE_WIRED` | Device / cross-process shared RW page (framebuffer, shared-region window): fork shares it verbatim, teardown never frees it |

User pages carry `PXN` always and `UXN` for data/stack, so non-executable mappings are
enforced. `g_pmap_lock` serialises page-table mutation across CPUs; PTE loads are
lock-free.

### User VA layout

[sys/include/aarch64/vmm_layout.h](../../sys/include/aarch64/vmm_layout.h) is the single
source of truth for the regions the exec loader, `mmap` and `brk` hand out:

| Region | Base | Notes |
|--------|------|-------|
| `DYN_MAIN_BASE` | `0x100000000` | PIE main executable |
| `DYN_INTERP_BASE` | `0x140000000` | Dynamic linker (`ld-musl`) |
| `DYN_STACK_VA` | `0x180000000` | Initial stack, 64 pages (256 KB) |
| `BRK_BASE` | `0x1C0000000` | `brk`/`sbrk` |
| `MMAP_BASE` | `0x200000000` | Anonymous `mmap` bump allocator |

(The header's prose still says the low 4 GB is the kernel identity map; with an empty
TTBR0 that is no longer enforced — the constants are what the loader uses today.
**TODO: verify** and refresh the header comment, which is outside this doc's scope.)

### Fork, exec and faults

- **Fork** — `pmap_fork_copy()` walks every mapped user page: MMIO and `PTE_WIRED`
  frames and `PTE_SHARED` cache pages are shared verbatim; everything else is shared
  read-only with `PTE_COW` on both sides and the frame's `cowCounter` bumped.
- **Faults** — the EL0 (and EL1) synchronous handler in
  [exceptions.c](../../sys/arch/aarch64/kern/exceptions.c) resolves, in order: a signal-return
  fetch fault, a data-abort *permission* fault on a COW page (`pmap_cow_fault`), and a
  *translation* fault via `vmm_demand_fault()`. Anything unresolved becomes `SIGSEGV`
  with a logged `pc` and `FAR`.
- **Exec** — `aarch64_exec_replace` demand-loads into a local VMA map that is installed
  only at the TTBR0 switch, so a failed load leaves the old image intact.
- **Switch** — `switch_to()` calls `pmap_switch()` only when the incoming task's root
  differs (see [task-switching.md](task-switching.md)).

---

## x86_64

### Translation setup

Built by [boot/start.S](../../sys/arch/x86_64/boot/start.S) (4-level, 4 KB pages with
2 MB large pages for the bootstrap maps), user spaces by
[kern/usermode.c](../../sys/arch/x86_64/kern/usermode.c):

| PML4 slot | Range | Purpose |
|-----------|-------|---------|
| 0 | `0x0` – `0x7FFFFFFFFF` | Low half. `PDPT[0]` = the shared low-1 GB identity PD (kernel bring-up and the MI loader's "frame as pointer" accesses keep working while a user CR3 is active); `PDPT[1..]` = per-process user mappings |
| 256 | `0xFFFF800000000000` | Physmap: all RAM below 4 GB (four PDs of 2 MB pages). `P2V(p) = p + PHYSMAP_BASE`, `V2P(v) = v - PHYSMAP_BASE` ([x86_64.h](../../sys/arch/x86_64/x86_64.h)) |
| 511 | `0xFFFFFFFF80000000` | Kernel image (`KERNBASE`; linked at `KERNBASE + 1 MB`, see `sys/compile/ldscript.x86_64`) |

`x86_64_create_user_space()` allocates a PML4 and a private PDPT under `PML4[0]` (with
`PDPT[0]` copied from the kernel's), and copies `PML4[256..511]` so the kernel stays mapped
across a CR3 switch. A user mapping lives at or above `0x40000000`, so it never lands in
the identity window.

RAM is discovered from the PVH `hvm_start_info` e820 map when the boot supplies one
(`sys/arch/x86_64/vmm/vmm_machdep.c`); only RAM below 4 GB is used, matching the physmap.
Without a PVH map it falls back to 256 MB.

Software PTE bits (available bits of the entry):

| Bit | Name | Meaning |
|-----|------|---------|
| 9 | `PTE_WIRED` | Fork shares the page verbatim; never freed with the address space |
| 10 | `PTE_COW` | Frame shared read-only; a write faults into `x86_64_cow_fault` |
| 11 | `PTE_SHARED` | Shared file-cache page (read-only, `vm_filecache`-owned) |

There is **no NX / W^X yet**: `md_map_user_page` ignores its `executable` argument and
every user page is executable.

### User VA layout

[sys/include/x86_64/vmm_layout.h](../../sys/include/x86_64/vmm_layout.h) uses the same
1 GB-aligned spacing as aarch64 so PIE binaries load identically:

| Region | Base | Notes |
|--------|------|-------|
| `EXEC_BASE` | `0x040000000` | Lowest user VA; a static `ET_EXEC` must load at or above this |
| `DYN_MAIN_BASE` | `0x100000000` | PIE main executable |
| `DYN_INTERP_BASE` | `0x140000000` | Dynamic linker (`ld-musl`) |
| `DYN_STACK_VA` | `0x180000000` | Initial stack, 64 pages |
| `BRK_BASE` | `0x1C0000000` | `brk`/`sbrk` |
| `MMAP_BASE` | `0x200000000` | Anonymous `mmap` bump allocator |

### Fork, exec and faults

- **Fork** — [kern/fork.c](../../sys/arch/x86_64/kern/fork.c) copies the address space
  with COW (`x86_64_map_user_page_cow`); a write fault on a `PTE_COW` page is resolved by
  `x86_64_cow_fault()`.
- **Faults** — `x86_64_exception()` in [kern/idt.c](../../sys/arch/x86_64/kern/idt.c)
  handles vector 14 in this order: signal-return address, COW (write fault), then
  not-present faults via `vmm_demand_fault()`; the remainder prints a register dump on
  COM1.
- **TLB** — `x86_64_tlb_shootdown()` flushes a VA on every CPU running the address space.
- **Teardown** — `x86_64_free_user_space()` frees the private `PDPT[1..]` subtree and
  skips `PDPT[0]`, `PML4[256..511]`, `PTE_WIRED` frames and MMIO frames.

---

## Demand paging (both arches)

**VMA tree** — `sys/vmm/vm_map.c`. Each process owns a red-black tree (`_current->vm_map`)
of `vm_map_entry_t` regions, each `[vm_start, vm_end)` tagged `VM_MAP_FILE`
(`vm_vnode` = an owned `fileDescriptor`, `vm_offset` = file offset) or `VM_MAP_ANON`.
`vm_map_remove`/`vm_map_free` close a file VMA's backing fd on teardown; `vm_map_copy`
duplicates the tree for fork.

**Demand-fault resolver** — `sys/vmm/vmm_demand.c::vmm_demand_fault(aspace_root, far)`.
The arch fault handler calls it on a **not-present (translation)** fault. It looks up the
covering VMA; a `VM_MAP_FILE` page is read with `vfs_pread_locked` (the same
`vfs_io_lock` `fread` uses, so demand reads are SMP-safe against concurrent FS access),
an anon page is demand-zeroed; the frame is mapped with `md_map_user_page`. Returns 0
(retry the access) or -1 (no VMA → SIGSEGV).

**Demand `execve`** — `sys/kern/elf64_demand.c::elf64_load_demand`. For a **static
ET_EXEC**, `execve` reads only the ELF headers and records a file-backed VMA per PT_LOAD
(plus one eager page at the file/BSS boundary and an anon VMA for the BSS tail) instead of
eagerly mapping + copying every page. A dynamic image (ET_DYN / PT_INTERP) takes the
eager `elf64_load` path. This is what lets a 100 MB binary (the on-device `clang`) start
without materializing all of it up front.

**Shared file pages** — read-only, file-identified pages (a library's text/rodata) are
published in `vm_filecache` and mapped `PTE_SHARED`, so there is one physical copy across
processes; a write copies out through the arch COW path.

---

## MMIO frames

Physical addresses at or above `numPages × PAGE_SIZE` are not RAM and have **no entry** in
`vmmMemoryMap`. Passing such an address to anything that indexes the map reads far out of
bounds. The 64-bit page-table code guards this on both arches (aarch64 `pmap_fork_copy` against
`count_memory()`, x86_64 teardown against `numPages`), e.g.:

```c
if ((src >> 12) >= (uintptr_t)count_memory()) {
    /* MMIO — share the mapping verbatim; never COW, count or free it */
}
```

The framebuffer is mapped into the compositor (`views`) by `sys_mapfb` (native syscall 43)
and shared-region windows by `sys_shareregion` (45); both are `PTE_WIRED` so fork and
teardown leave them alone. See [display.md](display.md).

---

## History: the i386 VMM

> Everything below describes the 32-bit i386 VMM, which exists only on the `releng/2`
> branch. It does not apply to master. The i386 layout (PDE-by-PDE map, self-referencing
> page directory, GDT selectors) is in
> [i386-page-directory-map.md](i386-page-directory-map.md). Function names and file names
> in this section refer to that branch.

### Key functions (i386)

#### `vmmInit()`

Top-level initialization entry point. Calls `vmmMemMapInit()` then `vmmPagingInit()`. Halts on failure.

#### `vmmMemMapInit()`

Builds the physical page-frame map — a linked list of all available physical pages. Each entry tracks:

- Physical frame address
- Owning PID
- Reference count (for COW sharing)
- Status flags (`free`, `allocated`, `cow-pending`)

#### `vmmPagingInit()`

Enables hardware paging. Sets up the kernel's initial page directory, identity-maps the lower 4 MB (PD[0], all 1024 PT entries — covers the kernel at `0x300000`), and remaps the physical frame bitmap from its physical staging location into the top 1 GB kernel virtual space at `VMM_MMAP_ADDR_PMODE = 0xC0800000`.

#### `vmmCreateVirtualSpace(pid)`

Allocates and initializes a fresh page directory for `pid`. Returns the physical base address. The shared lower 4 MB identity map and top 1 GB kernel mappings are pre-installed; everything between 4 MB and 3 GB starts unmapped.

#### `vmmCopyVirtualSpace(pid)`

Forks the address space of `pid` using copy-on-write (COW):

1. The entire 2 MB – 3 GB range is duplicated at the page-table level.
2. All pages in that range are marked read-only and COW-pending in both parent and child; no physical memory is copied.
3. On the first write to any shared page, a page fault fires.
4. The page-fault handler allocates a new physical frame, copies the content, clears the COW flag, and rewrites the faulting PTE to the new frame.

---

#### Physical Memory Layout

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

#### MMIO Pages and the vmmMemoryMap Boundary

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

#### Kernel Page Directory Desync After Fork

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

#### Page-Fault Handling

**Source:** `sys/vmm/page_fault.S`, `sys/vmm/pagefault.c`

x86 exception 14 (page fault) is routed to the VMM handler, which distinguishes three cases:

| Case | Condition | Action |
|------|-----------|--------|
| COW fault | Write to a shared COW page | Allocate new frame, copy, update PTE, resume |
| Demand-zero | First access to an allocated but unmapped page | Allocate zeroed frame, map, resume |
| Invalid access | Unmapped or protected region | Deliver `SIGSEGV` to faulting process |

---
