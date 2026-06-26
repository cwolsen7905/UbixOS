# Demand-Paged `execve` (MI, both 64-bit arches)

## Problem

The ELF exec path is **eager**: `read_elf_file` reads the entire binary into a
`kmalloc` buffer, then `load_segment` allocates, populates, copies, and i-cache-syncs
**every page of every PT_LOAD** before the program's first instruction. For a normal
KB–MB binary that's invisible. For the on-device **clang** (104 MB) it's fatal:

```
read_elf_file: /usr/bin/clang sz=104131144 kmalloc...   ← 104 MB kmalloc (succeeds)
read_elf_file: fread -> 104131144                        ← whole file read
elf64: load_segment va=0x200000  pages=7057  exec=0      ← data: 7057 pages eager
elf64: load_segment va=0x1DA0000 pages=17411 exec=1      ← text: 17411 pages eager
elf64:   ...16384/17411 pages                             ← ~6 ms/page → minutes, looks hung
```

~24,000 pages materialized up front, when `clang --version` only *executes* a few MB.
This is the pre-virtual-memory "swap the whole image in" model; every modern OS
(Linux, FreeBSD, XNU, NT) **demand-pages** executables from the file instead.

## Goal

`execve` maps each PT_LOAD segment **lazily**: set up VMAs (file-backed + demand-zero
BSS), jump to the entry point, and fault pages in **one at a time, only as touched**,
through the file page cache (one physical copy of read-only/exec pages shared across
processes). Result: instant exec regardless of binary size, and the 104 MB `kmalloc`
disappears.

**Built MI-first for BOTH arches** (aarch64 primary, x86_64 anchor) — see
`feedback_multiarch_first`. The shared logic lives in `sys/vmm` / `sys/kern` and
reaches the hardware only through `md_*` hooks; each arch implements the hooks + one
fault-handler call site.

## Current state (recon)

| Concern | aarch64 | x86_64 | i386 (frozen) |
|---|---|---|---|
| Fault handler | `exceptions.c` — COW only, no demand | `idt.c` — COW only, no demand | `vmm_page_fault.c` — **has** `vmm_demand_file_page` |
| Exec | `execfile.c` → eager `elf64_load` | `execfile.c` → eager `elf64_load` | eager |
| `vm_map` (VMA tree) | **unused** | **unused** | used (mmap) |
| mmap | eager `fread` whole region | (early) | demand-capable |

So neither 64-bit arch has *any* VMA-tracked demand-fault path. This adds the first one,
MI. The MD page-map hooks already exist for both (`md_map_user_page`, `md_phys_to_virt`,
`md_sync_icache`, used by the MI `elf64_load.c`); the one new hook is a **shared** map.

The VMA machinery is ready: `vm_map_insert_file(map,start,end,prot,flags,fd,off)`,
`vm_map_lookup`, `vm_map_remove`/`vm_map_free` (already `fclose` a file VMA's backing
fd — line 144 of `vm_map.c`), `vm_map_copy` (for fork). `vm_filecache_lookup_ref/insert`
give cross-process sharing. `vfs_pread_locked` is the SMP-safe positional read.

## Design

### 1. New MD hook — shared user-page map
`<sys/elf_load.h>`: `int md_map_user_page_shared(u_int64_t *aspace, u_int64_t va,
u_int64_t pa, int exec);` (RO, EL0/ring-3, cache-shared/refcounted, COW-on-write).
- aarch64: wrap `pmap_map_user_page_shared`.
- x86_64: map with `PAGE_SHARED` (its existing shared-page attrs).

### 2. MI demand resolver — `sys/vmm/vmm_demand.c` (new)
`int vmm_demand_fault(uintptr_t far)` → 0 if a page was mapped (retry), -1 if no VMA
covers `far` (real SIGSEGV). Logic (mirrors the per-page body aarch64 `sc_mmap` already
uses, but for ONE page, via `md_*` hooks):
- `vm_map_lookup(&_current->vm_map, far)`; NULL → -1.
- **VM_MAP_FILE**: `foff = vm_offset + (pg - vm_start)`. RO+exec & `ino!=0` →
  `vm_filecache_lookup_ref`; hit → `md_map_user_page_shared`. Miss → alloc frame,
  `md_phys_to_virt` + `memset` + `vfs_pread_locked(PAGE_SIZE)`; publish to filecache +
  map shared (RO/exec) or map private (writable data); `md_sync_icache` if exec.
- **VM_MAP_ANON**: demand-zero — alloc frame, zero, `md_map_user_page` private.

### 3. Per-arch fault-handler hook
On an EL0 **translation** fault (aarch64 DFSC 0x04–0x07) / ring-3 **#PF not-present**
(x86_64 error bit P=0), call `vmm_demand_fault(far)` *before* delivering SIGSEGV; on 0,
ERET/IRETQ and retry. Also the EL1/ring-0 on-behalf-of-user case (kernel touching an
un-faulted user page mid-syscall, e.g. argv copy) — same resolve, like the existing COW.

### 4. MI exec loader — `elf64_load_demand()` in `elf64_load.c`
Replaces eager `load_segment` for file execs. Per PT_LOAD:
- Full file pages `[vaddr, PAGE_DOWN(vaddr+filesz))` → `vm_map_insert_file` (prot from
  p_flags), backed by an `fopen` of the binary this VMA owns. **No population.**
- **Boundary page** (if `filesz` not page-multiple): one eager page — alloc, read the
  partial file bytes, zero the tail (it straddles file data and BSS). Keeps the resolver
  reading only whole pages (no clamp logic).
- BSS pages `[PAGE_UP(vaddr+filesz), PAGE_UP(vaddr+memsz))` → `vm_map_insert` anon
  (demand-zero).
Records entry/phdr info for auxv exactly as today.

### 5. Lifecycle
- **exec-replace / exit**: `vm_map_free(&_current->vm_map)` (closes every file VMA's
  fd) when tearing down the old/again address space.
- **fork**: `vm_map_copy(dst, src)` **with a per-file-VMA fd dup** so the child can
  fault its own un-touched pages from its own fd. *Required* — without it, a child that
  touches an un-faulted page (e.g. shell code before its own execve) SIGSEGVs. Already-
  faulted pages are COW-copied by the existing fork path.

## Boundary / correctness notes
- PT_LOAD `p_vaddr`/`p_offset` are congruent mod page size and `p_vaddr` page-aligned
  for clang (`p_align` 64 KB) — assume page-aligned start; if not, the first partial
  page is handled like the boundary page (eager).
- Writable file pages are **private** (never cached) → fork COW works unchanged.
- Read-only/exec pages are **shared** via `vm_filecache` → concurrent clang invocations
  share one physical text copy; a write takes a COW fault → private copy.
- i-cache sync moves from up-front to **first fault** of each exec page.

## Phases & progress

**Phasing decision (2026-06-26):** the **first cut is private-page only** — every
demand-faulted page (file or anon) is a *private* copy mapped via the existing
`md_map_user_page` hook. No new MD hook, no `vm_filecache` dependency, so it's
multi-arch-safe immediately (x86_64's shared-page/refcount machinery is incomplete).
This demand-pages correctly (clang faults only touched pages); cross-process *sharing*
of read-only text/rodata is a later optimization (Phase H) once both arches have the
shared-page COW + refcounted teardown confirmed.

| Phase | Item | aarch64 | x86_64 | Status |
|---|---|---|---|---|
| B | `vmm_demand_fault` MI resolver (private pages) | ☐ (shared) | ☐ (shared) | in progress |
| C | Fault-handler hook (translation/#PF) | ☐ `exceptions.c` | ☐ `idt.c` | not started |
| D | `elf64_load_demand` MI loader | ☐ (shared) | ☐ (shared) | not started |
| E | Exec wiring (`execfile.c`) + fd lifecycle | ☐ | ☐ | not started |
| F | fork `vm_map_copy` + fd dup; `vm_map_free` on exec/exit | ☐ | ☐ | not started |
| G | Test: clang loads instantly (aarch64); both kernels green | ☐ | ☐ | not started |
| H | *Optimization*: shared RO pages via `vm_filecache` + `md_map_user_page_shared` | ☐ | ☐ | deferred |

## CRITICAL FINDING (2026-06-26) — clang exposes a kernel-layout blocker

Demand paging is implemented (Phases B–E build green on both arches; dynamic binaries
correctly fall back to the eager path). But **clang cannot run on-device for a reason
unrelated to demand paging**: it is a **static ET_EXEC linked at `0x200000`**, which is
**block 0 — the kernel-identity region**. `pmap_create_user_space()` memcpy's the kernel
identity L1 and **shares blocks 0–3 by pointer across every address space**
(`USER_L1_MIN = 4`; user VAs are meant to start at block 4 = 4 GB). When the loader maps
clang's pages in block 0, `table_next` replaces the kernel's shared 2 MB identity *blocks*
with private L3 tables **in the shared kernel L1**, corrupting the global kernel identity;
the kernel then dies at the `pmap_switch` into clang's space (or the next access).

This is **not** a demand-paging bug — the daemons (PIE, loaded high in block 4+)
demand/eager-load fine. The eager loader would corrupt block 0 identically; it just never
finished (too slow) to reach the switch. The demand path is fast enough to expose it.

**Unblocking clang needs one of (separate, larger work):**
1. **Relocate the kernel to TTBR1 (high VAs)** — frees all of TTBR0 (low) for user, so a
   static binary at `0x200000` no longer collides. This is the long-noted
   "no kernel-in-TTBR1 relocation yet" item (pmap.c:227). The proper fix.
2. **Build the toolchain as static-PIE** (loads at a high base in the user region) — then
   `elf64_load_demand` must also handle ET_DYN at a chosen load base.
3. **Private low-VA sub-tables** — COW the kernel-identity L2 when a process adds a user
   page in blocks 0–3, so its mappings don't corrupt the shared kernel identity (a
   workaround, not the clean fix).

**Status of the demand-paging work itself:** infrastructure complete + green; the
demand-fault *path* (vmm_demand_fault) is still **unexercised at runtime** because every
current binary is PIE→eager and the one static binary (clang) hits the block-0 collision
before faulting. Validating the demand path needs a binary loaded in the user region
(option 1 or 2 above).

## Testing
1. `bmake kernel TARGET=aarch64` **and** `TARGET=x86_64` green (regression gate, every phase).
2. aarch64 boot-time `01-clangtest` init.d entry: clang faults in only touched pages →
   "loaded, jumping to EL0" prints in well under a second; `clang --version` runs.
3. fork-after-exec smoke: shell forks a child that execs — no SIGSEGV on un-faulted pages.
4. Existing apps (views/tcsh/vDoom on aarch64) still run (no demand-paging regression).
5. x86_64: builds + boots; exercised once file-execve lands (pre-FAT-root today).

## Remove after
The eager `load_segment` stays for the **embedded** bring-up binaries (`aarch64_run_elf_image`,
elfdemo) which have no fd; only the *file* exec path switches to demand. Drop the temporary
`read_elf_file`/`load_segment` debug `kprintf`s.
