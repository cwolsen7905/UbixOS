# UbixOS Virtual Memory Subsystem — Technical Audit

*Audited 2026-05-16 against the `feature/macos-build-qemu` branch.*

---

## 1. Physical Memory Bootstrap

**Initialization path:** `vmm_init()` (`sys/vmm/vmm_init.c:43`) calls `vmm_memMapInit()` then `vmm_pagingInit()`.

**Memory counting:** `countMemory()` (`sys/vmm/vmm_memory.c:110–211`) probes RAM by writing `0x55AA55AA` / `0xAA55AA55` patterns upward from 8 KB, disabling IRQ1/IRQ2 during the scan to prevent interference. Returns page count: `(memKb * 1024 * 1024) / PAGE_SIZE`.

**Memory map structure** (`sys/vmm/vmm_memory.c:46`, `sys/include/vmm/vmm.h:106–112`):
- Global `mMap *vmmMemoryMap` array, one entry per physical page.
- Initial location: `0x101000` (pre-paging real-mode address).
- Relocated to: `0xC0800000` after paging is enabled.
- Entry fields: `pageAddr` (physical address), `status` (avail/not), `pid` (owner), `cowCounter` (ref count).

**Bootstrap initialization** (`sys/vmm/vmm_memory.c:57–99`):
- Page at `0x100000` marked available.
- `memStart` = `0x100000/0x1000` + pages consumed by the mMap array itself.
- With 256 MB RAM (65536 pages) the mMap occupies ~256 KB = 64 pages.
- All pages from `memStart` onward marked available.
- Global `numPages` set by `countMemory()`.

---

## 2. Page Table Layout

**Virtual address space partition** (`sys/include/vmm/vmm.h`):

| Range | Purpose |
|---|---|
| `0x00000000–0x007FEFFF` | Kernel code (VMM_KERN_CODE, ~8 MB, identity-mapped) |
| `0x007FF000` | VMM_USER_LDT |
| `0x00800000–0xBFFFFFFF` | User space (VMM_USER_START/END, ~3 GB) |
| `0xC0000000` | PT_BASE_ADDR — page-table self-map |
| `0xC0400000` | PD_BASE_ADDR — page-directory self-map |
| `0xC0800000–0xFDFFFFFF` | Kernel heap / drivers (VMM_KERN_START/END) |
| `0xFE000000–0xFFFFFFFF` | Kernel stacks (VMM_KERN_STACK) |

**Kernel loaded at:** virtual `0x20000` (linker script `sys/compile/ldscript.i386`). Shared across all processes via PD[0–1].

**Kernel page directory** (`sys/vmm/paging.c:65`):
- Single persistent PD allocated as the first free physical page.
- Loaded into CR3 (`paging.c:171–177`).
- PD[0]: first 4 MB identity-mapped (BIOS/V86 region, shared).
- PD[1]: second 4 MB (kernel code), COW'd during fork.
- PD[770–1023]: kernel heap/drivers, shared across all processes.
- PD[1023]: kernel stacks.

**Self-map mechanism:**
- **PT_BASE_ADDR (`0xC0000000`, PD index 768):** Maps all 1024 page-table physical addresses. Access any PTE as `((uint32_t*)PT_BASE_ADDR)[pdIndex * 1024 + ptIndex]`.
- **PD_BASE_ADDR (`0xC0400000`, PD index 769):** Maps the PD itself. Access any PDE as `((uint32_t*)PD_BASE_ADDR)[pdIndex]`.

---

## 3. Address Space Creation (vmm_createVirtualSpace)

`sys/vmm/createvirtualspace.c:55–198`. Called for initial task creation (not fork).

1. Allocate new PD via `vmm_getFreePage(pid)` (line 68).
2. Share lower 8 MB: `newPD[0] = parentPD[0]`; allocate new PT for `newPD[1]`.
3. COW-mark PD[1] pages: set `PAGE_COW`, increment `cowCounter` by 2 (first time) or 1 (already shared).
4. Copy kernel region: `newPD[x] = parentPD[x]` for x in 770–1023.
5. Stack pages: allocate new PT for PD[1023], COW-mark.
6. Self-maps: allocate PTs for PD[768–769], fill with PD entries and PD physical address.

**Key difference from fork:** Does not COW-mark user pages (PD[2–767]). Used only for fresh address spaces (init, shell via `execve`).

---

## 4. Fork — vmm_copyVirtualSpace

`sys/vmm/copyvirtualspace.c:52–328`, called from `sys/arch/i386/fork.c`.

A static spinlock (`line 36`) prevents concurrent forks.

### Phase 1 — New PD (lines 63–74)
Allocate new PD with `vmm_getFreeKernelPage(pid, 1)`. Zero-fill.

### Phase 2 — Lower 8 MB (lines 76–126)
- `newPD[0] = parentPD[0]` (share identity map).
- Allocate fresh PT for `newPD[1]`.
- For each present PTE in parent's PD[1]:
  - If `(phys >> 12) >= numPages` → MMIO page: copy PTE as-is, no COW.
  - Else: clear `PAGE_WRITE`, set `PAGE_COW`; `cowCounter += 2` (first time) or `+= 1` (already COW).

### Phase 3 — User space COW (lines 182–269)
Same logic as Phase 2, applied to PD[2–767]:
- Stack pages (marked `PAGE_STACK`): `memcpy` full page — child gets independent copy.
- MMIO pages: share as-is.
- Normal pages: COW-mark.

### Phase 4 — Kernel stacks (lines 132–173)
PD[1023–1027]: allocate fresh PT per entry, `memcpy` each stack page — no COW sharing.

### Phase 5 — Kernel PD re-sync (lines 304–313) ⚠️ Critical ordering

```c
for (x = PD_INDEX(VMM_KERN_START); x <= PD_INDEX(VMM_KERN_END); x++)
    newPageDirectory[x] = parentPageDirectory[x];
```

This re-copy happens **after all `vmm_getFreeKernelPage` / `vmm_getFreePage` calls**. Those calls may expand the parent's kernel PD into new PD indices; copying early would leave the child with stale (zero) entries for the newly-expanded ranges. Copying late — after all allocations — guarantees the child sees the full current parent state.

### Phase 6 — Self-maps (lines 271–322)
Allocate PTs for PD[768–769]; fill with current PD entries and the new PD's physical address.

**Returns:** physical address of the new page directory (child's CR3 value).

---

## 5. COW Page Fault Handler

Vector 14 → `page_fault.S` → `trap()` → `vmm_pageFault(trapframe, cr2)` (`sys/vmm/pagefault.c:56–190`).

| Step | Lines | Action |
|---|---|---|
| Lock | 67 | Acquire `pageFaultSpinLock` |
| V86 | 73–86 | Identity-map faulted page on demand for VM86 tasks |
| Null pointer | 92–97 | `memAddr == 0` → kpanic |
| No PT | 105–112 | `pageDir[index] == 0` → kill task |
| **COW fault** | 117–141 | See below |
| Permission fault | 143–154 | Present non-COW PTE → security violation |
| Demand alloc | 155–169 | Within data segment: allocate fresh zeroed page |
| Unmapped | 170–182 | All other cases → kill task |
| TLB flush | 184–185 | Reload CR3 |

**COW fault path (lines 117–141):**
1. Confirm `PAGE_COW` flag is set on faulted PTE.
2. `src = faultAddr & 0xFFFFF000` — source physical page.
3. Allocate new page via `vmm_getFreeVirtualPage()`.
4. `memcpy` 4096 bytes from old page to new page.
5. `adjustCowCounter(src, -1)` — decrement (and free if counter reaches 0).
6. Rewrite PTE: clear `PAGE_COW`, set `PAGE_WRITE`, point to new physical page.
7. Unmap the temporary kernel mapping.

---

## 6. Execve Address Space Teardown (vmm_cleanVirtualSpace)

`sys/vmm/paging.c:682–725`.

Walks PD[addr/4MB … 767] (user space only — kernel PD entries are left in place):
- For each present PT:
  - For each present PTE:
    - Extract `phys = PTE & 0xFFFFF000`.
    - If `PAGE_COW`: `adjustCowCounter(phys, -1)` — lazy free when counter hits 0.
    - Else if `(phys >> 12) < numPages`: `freePage(phys)` — immediate reclaim.
    - MMIO pages (`(phys >> 12) >= numPages`): skip — no freePage call.
    - Clear PTE to 0.
- Reload CR3 to flush TLB.

---

## 7. Kernel Page Allocation

### vmm_getFreeKernelPage(pid, count) — `sys/vmm/paging.c:337–399`
- Allocates `count` consecutive pages from `0xC0800000–0xFDFFFFFF`.
- Holds `pdSpinLock` (kernel space is shared across all address spaces).
- Iterates PD[770–1023]; allocates a new PT via `vmm_allocPageTable` if needed.
- Scans PT for `count` consecutive free slots; maps physical pages and zero-fills each.
- Returns virtual address: `(pdIndex * 0x400000) + (ptIndex * 0x1000)`.

### vmm_getFreePage(pid) — `sys/vmm/getfreepage.c:46–85`
- Simpler single-page version; same kernel address range.
- Finds first free PT entry, maps and returns.

### vmm_getFreeVirtualPage(pid, count, type) — `sys/vmm/getfreevirtualpage.c:49–143`
- Allocates in user space (`0x00800000–0xBFFFFFFF`).
- Used by the COW fault handler and data segment growth.
- `VM_TASK` type: starts from `vmStart`; `VM_THRD` type: starts from `vm_daddr + vm_dsize`.
- Scans forward for `count` consecutive unmapped pages.
- Updates `vmStart` or `vm_dsize` accordingly.

---

## 8. Reference Counting

**Storage:** `vmmMemoryMap[physPage >> 12].cowCounter` (signed int).

| Counter value | Meaning |
|---|---|
| 0 | Free / available |
| 1 | Single owner with `PAGE_COW` still set (transitional) |
| ≥2 | Shared among N processes (counter = N + 1) |

**Fork increment** (`copyvirtualspace.c:105–114`):
```c
if (PTE & PAGE_COW)
    adjustCowCounter(phys, +1);   // already shared
else {
    adjustCowCounter(phys, +2);   // first share
    parentPTE |= PAGE_COW;
    parentPTE &= ~PAGE_WRITE;
}
```

**Decrement paths:**
1. COW fault (`pagefault.c:137`): `adjustCowCounter(phys, -1)`.
2. Execve cleanup (`paging.c:705`): `adjustCowCounter(phys, -1)` for COW pages.
3. Process exit (`vmm_memory.c:374–390`): skips COW pages entirely (see BUG-COW-03).

**`adjustCowCounter`** (`sys/vmm/vmm_memory.c:313–343`): acquires spinlock, applies delta; if counter ≤ 0 marks page available and increments `freePages`.

---

## 9. Known Bugs and Fragile Invariants

### BUG-COW-03 — Process exit cannot walk dying task's PTEs
`sys/vmm/vmm_memory.c:365–371`. `vmm_freeProcessPages` cannot access the dead task's page tables because `PT_BASE_ADDR` reflects the *running* task's CR3, not the dying task's. **Workaround:** the PTE walk is disabled; only pages with `cowCounter == 0` are freed. **Impact:** COW-shared pages leak references when a parent dies before its children have faulted them in. **Severity: HIGH.**

### BUG-COW-07 — Double decrement on COW pages during exit
`sys/vmm/vmm_memory.c:385–389`. If `vmm_cleanVirtualSpace` (execve) already decremented a COW counter, `vmm_freeProcessPages` would decrement it again, corrupting the counter. **Workaround:** COW pages skipped entirely in `vmm_freeProcessPages`.

### Critical ordering — kernel PD re-sync after fork allocations
`copyvirtualspace.c:304–313`. Kernel PD entries **must** be re-copied from parent to child *after* all `vmm_getFreeKernelPage` / `vmm_getFreePage` calls complete. Those calls may expand the parent's kernel PD into previously-empty PD indices; an early copy would leave the child with stale zero entries for those ranges, causing a page fault or triple-fault on first kernel access from the child. See also: `CLAUDE.md`.

### MMIO bounds guards
Physical addresses `>= numPages * PAGE_SIZE` have no `vmmMemoryMap` entry. Calling `freePage` on such an address computes an array index in the billions, dereferences `0xC0800000 + huge_offset` (unmapped), and triple-faults. Guards exist at:
- `copyvirtualspace.c:94, 234` — skip COW for MMIO PTEs.
- `paging.c:707` — skip `freePage` in `vmm_cleanVirtualSpace`.
- `vmm_memory.c:273–276, 319–322` — explicit bounds check in `freePage` / `adjustCowCounter`.

---

## 10. Incomplete / Stub Implementations

| Function | File | Status |
|---|---|---|
| `vmm_freeVirtualPage` | `sys/vmm/freevirtualpage.c:31–35` | Stub — returns 0, never reclaims user VA |
| `brk()` shrink | `sys/vmm/paging.c:630–680` | Growing works; shrinking commented "not yet supported" |
| Memory pressure / OOM | `vmm_findFreePage` | Panics on OOM; no swap, eviction, or LRU |
| `vmm_mapFromTask` | `sys/vmm/paging.c:428–558` | Partially implemented, not used in fork/exec |
| `vmm_share_region` | `sys/vmm/vmm_share_region.c:47–114` | Display-server only; not wired to a general mmap syscall |

---

## Summary

| Area | Status |
|---|---|
| Bootstrap (countMemory, memMapInit, pagingInit) | Working |
| Basic page allocation (kernel + user) | Working |
| Fork COW marking | Working |
| COW page fault resolution | Working |
| Execve address space teardown | Working (MMIO guards in place) |
| Process exit cleanup | **Buggy** — BUG-COW-03, BUG-COW-07 |
| Kernel PD re-sync ordering in fork | Correct but fragile — must not be reordered |
| Address space shrinking | Not implemented |
| Memory pressure handling | Not implemented |
