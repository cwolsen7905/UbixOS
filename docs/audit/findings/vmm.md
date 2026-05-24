# VMM Audit Findings

## Summary

18 findings: 2 critical, 5 high, 7 medium, 4 low

---

## Findings

### VMM-001 — vmm_remapPage: kpanic before unreachable fallback code
**Severity:** 🔴 Critical
**File:** `sys/vmm/paging.c:260`
**Description:** When `pageTable[destPageTableIndex]` is already present the function calls `kpanic(...)` unconditionally, but then falls through to dead code that attempts to handle the COW case and call `freePage`. Because `kpanic` never returns, the post-panic logic (COW check, `freePage`, `source = 0`) is entirely unreachable.
**Impact:** Any caller that passes a destination address that already has a present PTE — including a COW entry that should be replaced — causes an immediate kernel panic rather than following the intended COW-remap path.
**Suggested fix:** Remove the `kpanic` and promote the dead code to be the active logic: check for COW, free the existing page, and proceed with the remap. Assert or warn with `kprintf` instead.

---

### VMM-002 — freebsd6_mmap: user-supplied `addr` never assigned; maps pages at virtual 0x0
**Severity:** 🔴 Critical
**File:** `sys/vmm/vmm_mmap.c:104–107`
**Description:** In `freebsd6_mmap`, `addr` is declared `vm_offset_t addr = 0x0` and is **never assigned from `uap->addr`**. The `MAP_ANON` loop `for (i = addr; ...)` therefore iterates from 0, mapping user-allocated physical pages at virtual address 0 — overwriting the identity-mapped kernel/IVT region. In `sys_mmap`, a non-null `uap->addr` is accepted without bounds-checking against `VMM_USER_START`/`VMM_USER_END`, allowing a user process to supply a kernel-space address.
**Impact:** `freebsd6_mmap` silently corrupts the identity-mapped first page on every `MAP_ANON` call. `sys_mmap` with a non-null `addr` can give user processes write access to kernel virtual memory (privilege escalation).
**Suggested fix:** Assign `addr` from a `vmm_getFreeVirtualPage` call in `freebsd6_mmap`. In `sys_mmap`, validate that the requested address range lies within `[VMM_USER_START, VMM_USER_END]` before mapping.

---

### VMM-003 — vmm_share_region: dst->oInfo.vmStart modified without lock; partial failure leaks virtual range
**Severity:** 🟠 High
**File:** `sys/vmm/vmm_share_region.c:91–93, 132–141`
**Description:** `dst->oInfo.vmStart` is incremented while `_current` is the source task and interrupts are still enabled. A concurrent fork or mmap on the destination task races against this update. On partial failure of the `vmm_remapPage` loop, the already-bumped `dst->oInfo.vmStart` is not rolled back and the partially-mapped pages are never freed.
**Impact:** Virtual address space corruption in the destination process; leaked physical pages mapped but unreachable because `vmStart` has moved past them.
**Suggested fix:** Hold a lock (or `cvsSpinLock`) around `vmStart` updates. On failure, unmap already-mapped pages and restore `dst->oInfo.vmStart`.

---

### VMM-004 — vmm_copyVirtualSpace: PD[1] child PTE uses KERNEL_PAGE_DEFAULT (no PAGE_USER)
**Severity:** 🟠 High
**File:** `sys/vmm/copyvirtualspace.c:102`
**Description:** Child PD[1] PTEs are built as `(phys | (KERNEL_PAGE_DEFAULT & ~PAGE_WRITE) | PAGE_COW)`. `KERNEL_PAGE_DEFAULT` is `PAGE_PRESENT|PAGE_WRITE` — it lacks `PAGE_USER`. User-mode accesses to the 4–8 MB range in the child process will fault because the PTE has no `PAGE_USER` bit. In `createvirtualspace.c` line 93, the same range is set COW but `PAGE_WRITE` is not cleared, making COW ineffective.
**Impact:** After `fork`, child user-space code touching data in the second 4 MB faults. In `createvirtualspace`, two processes silently share a writable page without COW triggering.
**Suggested fix:** Use `(PAGE_DEFAULT & ~PAGE_WRITE) | PAGE_COW` for user-accessible PD[1] entries in both `copyvirtualspace.c` and `createvirtualspace.c`.

---

### VMM-005 — vmm_cleanVirtualSpace: open-coded PD index calculation instead of PD_INDEX macro
**Severity:** 🟠 High
**File:** `sys/vmm/paging.c:735`
**Description:** `x = (addr / (PD_ENTRIES * PAGE_SIZE))` is an open-coded version of `PD_INDEX(addr)` but applied at 4 MB granularity. If `addr` is not 4 MB-aligned, `x` still rounds down to the 4 MB PD boundary and could revisit already-freed entries or — if `addr` is `0x400000` — try to free pages in PD[1] which includes kernel-mapped pages.
**Impact:** Double-free risk for pages near the start of user space; potential interaction with kernel-mapped region in PD[1].
**Suggested fix:** Replace with `PD_INDEX(addr)` to use the canonical shift-based calculation used everywhere else.

---

### VMM-006 — vmm_unmapPage: freePage called without MMIO guard (flags==0 path)
**Severity:** 🟠 High
**File:** `sys/vmm/unmappage.c:70`
**Description:** `vmm_unmapPage` with `flags == 0` calls `freePage(pageTable[pageTableIndex] & 0xFFFFF000)` unconditionally. There is no `(phys >> 12) < numPages` check. `freePage` has an internal bounds check and returns -1 for MMIO addresses, but the error is silently discarded by the caller.
**Impact:** MMIO-backed mappings (framebuffer, PCI BARs) silently fail to free, log spurious error messages, and leave `freePages` in an inconsistent state.
**Suggested fix:** Add `if ((phys >> 12) < (uint32_t)numPages)` guard before calling `freePage`, mirroring the pattern in `vmm_cleanVirtualSpace` and `obreak`.

---

### VMM-007 — pagefault.c: PTE written before swap_read_page; stale data window on failure
**Severity:** 🟠 High
**File:** `sys/vmm/pagefault.c:156–167`
**Description:** During swap-in, `pageTable[pageTableIndex] = newPage | PAGE_DEFAULT` and the TLB flush are issued before `swap_read_page`. If `swap_read_page` fails and the task is not immediately destroyed, any CPU access to that virtual address reads garbage from the uninitialized physical page.
**Impact:** Information disclosure (another process's former data) or use of uninitialized state before the task is killed.
**Suggested fix:** Perform the `swap_read_page` into the new physical page while it is not yet reachable via the PTE, then update the PTE and issue `invlpg` only on success.

---

### VMM-008 — vmm_pagingInit: vmm_findFreePage return value not checked for stack pages
**Severity:** 🟡 Medium
**File:** `sys/vmm/paging.c:123–124`
**Description:** `pageTable[1023] = (vmm_findFreePage(sysID) | KERNEL_PAGE_DEFAULT | PAGE_STACK)` and the line below OR the result directly into the PTE without a NULL check. If `vmm_findFreePage` returns 0, the PTE becomes `0 | flags = flags` — mapping the null page as a kernel stack page, silently passing the `PAGE_PRESENT` check.
**Impact:** Kernel stack mapped to physical page 0 on very low-memory systems; writes to the kernel stack corrupt real-mode IVT/BDA.
**Suggested fix:** Assign to a temp variable and `K_PANIC` if zero, consistent with every other `vmm_findFreePage` call in the same function.

---

### VMM-009 — countMemory: probes physical RAM without using multiboot memory map
**Severity:** 🟡 Medium
**File:** `sys/vmm/vmm_memory.c:179–207`
**Description:** Memory detection is done by writing test patterns to successive megabyte boundaries. The loop cap of 4096 MB means on a system with exactly 4 GB of RAM `memKb` never exceeds 4096 and the `memKb--` at line 213 yields 4095 MB = an off-by-one in the page count. The approach also does not account for memory holes reported by the BIOS/ACPI and risks touching MMIO ranges on non-QEMU hardware.
**Impact:** Incorrect `numPages` value; bitmap could over- or under-allocate, causing out-of-bounds bitmap accesses later.
**Suggested fix:** Parse the multiboot memory map (E820 entries) provided in the multiboot info structure; it is always present when booting via GRUB2 and is the correct authoritative source for usable RAM ranges.

---

### VMM-010 — vmm_getFreeMallocPage: contiguous search can span two page tables
**Severity:** 🟡 Medium
**File:** `sys/vmm/paging.c:590–611`
**Description:** The inner verification loop `for (c = 0; c < count; c++)` checks `y + c < 1024` but the outer loop only advances `y` within a single page table. If a requested run of pages spans the end of one page table (y + count > 1024), the inner loop exits at `y + c == 1024` without setting `c = -1`, allowing the allocation to "succeed" with a virtual range that crosses into the next page directory entry.
**Impact:** Multi-page kmalloc allocations crossing PT boundaries could overlap existing mappings; caught only if destination PTEs are already present (kpanic in `vmm_remapPage`).
**Suggested fix:** In the inner verification loop, break with `c = -1` when `y + c >= PT_ENTRIES`, not just when it finds a present PTE.

---

### VMM-011 — vmm_getFreeVirtualPage: start_page advance on collision is one page short
**Severity:** 🟡 Medium
**File:** `sys/vmm/getfreevirtualpage.c:108–111`
**Description:** When a present PTE blocks a multi-page run, `start_page += (PAGE_SIZE * counter)` advances only by the already-counted pages, not past the blocking page. The next iteration restarts on the blocking page, creating an infinite scan loop or a one-page-too-early allocation.
**Impact:** Infinite loop or allocation overlapping the blocking page for multi-page allocations.
**Suggested fix:** Use `start_page += PAGE_SIZE * (counter + 1)` to skip the blocking page, then reset `counter = 0` and `map_from = 0`.

---

### VMM-012 — vmm_allocPageTable: spinLock commented out — race on shared PD entry
**Severity:** 🟡 Medium
**File:** `sys/vmm/vmm_allocpagetable.c:17, 43`
**Description:** `spinLock(&pdSpinLock)` / `spinUnlock(&pdSpinLock)` are commented out. The function writes `pageDirectory[pdI]` and the self-map entry `pageTable[pdI]` without any lock. Callers hold different locks (`pdSpinLock` in `vmm_getFreeKernelPage`; `rmpSpinLock` or `pdSpinLock` in `vmm_remapPage`), so two concurrent callers can allocate two different physical pages for the same PD index, leaking one.
**Impact:** Physical page leak; stale PD entry under concurrent load (SMP or re-entrant interrupt paths).
**Suggested fix:** Restore the `spinLock(&pdSpinLock)` / `spinUnlock` calls, or document that callers always hold an equivalent lock.

---

### VMM-013 — vmm_copyVirtualSpace: vmm_getFreePage instead of vmm_getFreeKernelPage for self-map tables
**Severity:** 🟡 Medium
**File:** `sys/vmm/copyvirtualspace.c:282, 297`
**Description:** The PT_BASE_ADDR and PD_BASE_ADDR self-map pages are allocated via `vmm_getFreePage`, which does not hold `pdSpinLock` during its kernel-space scan.
**Impact:** Race with concurrent kernel page allocations could return the same virtual address twice, corrupting the self-map.
**Suggested fix:** Use `vmm_getFreeKernelPage(pid, 1)` (which acquires `pdSpinLock`) consistently for all page table allocations in `vmm_copyVirtualSpace`.

---

### VMM-014 — adjustCowCounter/freePage: double-free race via TOCTOU on cowCounter
**Severity:** 🟡 Medium
**File:** `sys/vmm/vmm_memory.c:322–337`
**Description:** `freePage` reads `vmmMemoryMap[pageIndex].cowCounter == 0` **outside** the spinlock (line 322), then acquires the lock. A concurrent `adjustCowCounter` could decrement the counter to 0 between the read and the lock, causing both code paths to free the same page, incrementing `freePages` twice for one page.
**Impact:** Double-free / physical page aliased to two different callers; `freePages` overcounted.
**Suggested fix:** Move the `cowCounter == 0` check inside the spinlock in `freePage`.

---

### VMM-015 — vmm_pagingInit: PD[1] page table allocated but left empty
**Severity:** 🔵 Low
**File:** `sys/vmm/paging.c:95–101`
**Description:** A page is allocated and assigned to `kernelPageDirectory[1]` but never populated (only `bzero`'d). The second 4 MB is handled lazily elsewhere, making this allocation unused at init time.
**Impact:** One physical page wasted during kernel initialisation.
**Suggested fix:** Either populate it with an identity map for 4–8 MB, or remove the allocation and let `vmm_allocPageTable` handle PD[1] lazily.

---

### VMM-016 — vmm_mapFromTask: fixed window 0x5A01000 can race with VMM_CHILD_PD_WINDOW in vmm_share_region
**Severity:** 🔵 Low
**File:** `sys/vmm/paging.c:464, 512, 522–527`
**Description:** `vmm_mapFromTask` uses the hard-coded range starting at `0x5A01000`, which overlaps `VMM_CHILD_PD_WINDOW` (0x5A00000) used by `vmm_share_region`. There is no mutual exclusion between these two functions. Additionally, the cleanup loops at lines 522–527 iterate 0x1000 (4096) times when only up to 1024 PD entries exist, unmapping 4× more kernel pages than needed.
**Impact:** Concurrent use of these two functions corrupts the shared window. The over-unmap (4096 iterations) could unmap valid kernel mappings.
**Suggested fix:** Serialise access to the `VMM_CHILD_PD_WINDOW` range with a dedicated lock, and fix cleanup loop bounds to `PD_ENTRIES`.

---

### VMM-017 — vmm_findFreePage: O(n) scan with lock held on every allocation
**Severity:** 🔵 Low
**File:** `sys/vmm/vmm_memory.c:261–278`
**Description:** Every allocation scans the full bitmap from index 0 under `vmmSpinLock`. With 256 MB RAM (65536 pages), this is up to 65536 iterations per call, all while holding the spinlock and blocking other allocators.
**Impact:** Spinlock hold times grow with RAM size, degrading interrupt latency and allocation throughput under heavy fork/exec/kmalloc load.
**Suggested fix:** Maintain a `next_free_hint` index (roving pointer / clock algorithm) so average scan time is O(1) amortised.

---

### VMM-019 — vmm_remapPage: non-COW "already present" path silently skips mapping; callers panic
**Severity:** 🔴 Critical (investigation)
**File:** `sys/vmm/paging.c:258–274`, `sys/vmm/vmm_mmap.c:66–71`, `sys/vmm/unmappage.c`
**Description:** When `vmm_remapPage` encounters a present non-COW PTE, the active logic (restored from dead code in VMM-001 fix) sets `source = 0` and jumps to `rmDone`, returning `0x0` to the caller. Any caller that checks the return value for `0x0` failure (e.g. `sys_mmap` calls `K_PANIC("Remap Page Failed")`) will still panic — the panic has just moved from inside `vmm_remapPage` to the caller.

The root question is: **how does a present non-COW PTE exist at a destination address that the caller believes is free?**

Known callers that do unmap-before-remap (should be safe):
- `sys_mmap` — calls `vmm_unmapPage(map_base + x, VMM_FREE)` before `vmm_remapPage`
- `execFile` load loop — calls `vmm_unmapPage` before each `vmm_remapPage`

If these callers still hit the "already present" path, the bug is in `vmm_unmapPage` failing to clear the PTE (e.g. the page-table self-map is stale after a page-directory allocation, or the wrong virtual address is being computed).

**Suggested investigation:**
1. Audit `vmm_unmapPage` to confirm it writes `pageTable[idx] = 0` and issues `invlpg` for the target address.
2. Confirm `PT_BASE_ADDR + (PAGE_SIZE * pdIdx)` resolves to the correct page table for `dest` immediately after `vmm_allocPageTable` is called for a fresh PD entry.
3. Add a `klog(KLOG_CRIT, ...)` + caller address dump when the non-COW path is hit to identify which caller is triggering it in practice.

---

### VMM-018 — vmm_mapFromTask: dead code with no callers
**Severity:** 🔵 Low
**File:** `sys/vmm/paging.c:432–562`
**Description:** `vmm_mapFromTask` has no call sites in the codebase. It is dead code. If ever called, the 4096-iteration cleanup loops (lines 522–527, 548–553) would over-unmap valid kernel pages.
**Impact:** No immediate impact. Maintenance burden and latent over-unmap bug if re-activated.
**Suggested fix:** Remove `vmm_mapFromTask` from `paging.c` and its declaration from `sys/include/vmm/paging.h`, or move it to a clearly marked "experimental/unused" section with a comment.
