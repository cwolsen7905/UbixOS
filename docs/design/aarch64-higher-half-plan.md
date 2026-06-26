# AArch64 Higher-Half Kernel (relocate kernel + identity to TTBR1)

## Why

The kernel currently runs **entirely in TTBR0 (low VAs)** with a flat identity map, so
the low VA space is shared between the kernel and every process. A **static `ET_EXEC`
linked at `0x200000` (clang) collides with the kernel-identity region** and corrupts the
shared kernel tables (see `demand-paged-exec-plan.md` "CRITICAL FINDING"). Moving the
kernel + its identity/physmap to **TTBR1 (high VAs)** frees **all of TTBR0 for user**, so:
- static low-linked binaries (clang, the whole self-hosting toolchain) just work;
- per-process TTBR0 spaces no longer carry a copy of the kernel identity (no shared-L1
  corruption, cleaner fork/exec);
- the demand-fault path finally gets exercised by a real binary.

This is the long-noted "no kernel-in-TTBR1 relocation yet" item (`pmap.c:227`, `mmu.c:11`).

## Current state (recon)

- `mmu.c` — one static `l1_table[512]` of **1 GB block descriptors**, **VA==PA identity**,
  `TCR_EL1`: T0SZ=25 (39-bit VA), **EPD1=1 (TTBR1 disabled)**, TTBR0 = `l1_table`.
- Kernel linked at **`0x40200000`** (`ldscript.aarch64`), QEMU `-kernel` loads it there;
  `start.S` `_start` zeroes BSS → C → `aarch64_mmu_init`.
- **phys==virt is assumed everywhere**: `md_phys_to_virt` is identity (pmap.c), page-table
  walks cast a physical address straight to a pointer (`table_next`, `pmap_extract`,
  `pmap_free_user_space`, the demand resolver, `vmm_find_free_page` returns a phys used as
  a kernel pointer). **This is the hard part to unwind.**
- `pmap_create_user_space` memcpy's `l1_table` so every space maps the kernel; user pages
  are added in blocks the kernel "doesn't use" (`USER_L1_MIN=4`) — but a static ET_EXEC
  ignores that and lands in block 0.

## The core challenge

A true higher-half kernel breaks `phys == virt`. Once the kernel runs at a high VA, a
physical frame's kernel pointer is `PHYSMAP_BASE + phys`, **not** `phys`. Every site that
treats a physical address as a pointer must go through a **physmap** translation. So the
migration is mostly: introduce `PHYSMAP_BASE`, route all phys→ptr through it, then drop the
low identity.

## Phased plan (each phase must boot — test `run-debug-aarch64` under tcg *and* HVF)

### Phase 1 — Add a TTBR1 physmap, dual-mapped (no behaviour change)
- Build a TTBR1 table mapping **all RAM + peripherals at `PHYSMAP_BASE`** (e.g.
  `0xFFFF_8000_0000_0000`), same attrs as today. Program `TCR_EL1.T1SZ`, clear `EPD1`,
  set `TTBR1_EL1`. **Keep** the TTBR0 identity intact. Kernel still executes low.
- Verify: boots identically; `PHYSMAP_BASE+phys` reads match `phys` reads.

### Phase 2 — Relocate kernel execution to high VAs
- Relink the kernel at a **high VA** (`KERNEL_VA_BASE = PHYSMAP_BASE + 0x40200000`, or a
  dedicated high text base) — `ldscript.aarch64` `. = KERNEL_VA_BASE`, with `AT()` load at
  the physical `0x40200000` so QEMU still loads it there.
- `start.S`: after enabling the MMU (with both TTBRs up from Phase 1), **trampoline**: load
  the high-VA address of the next step and `br` to it; set `VBAR_EL1` to the high vectors;
  switch SP to a high-VA stack.
- Verify: boots, runs at high VA, exceptions work (VBAR high).

### Phase 3 — Unwind phys==virt through the physmap (the big one)
- `md_phys_to_virt(phys)` → `(void*)(PHYSMAP_BASE + phys)`.
- Every page-table walk / phys-as-pointer: `table_next`, `pmap_extract`, `pmap_map_page`,
  `pmap_free_user_space`, `pmap_fork_copy`, `pmap_cow_fault`, the demand resolver, the ELF
  loaders' `kframe`, `vmm_find_free_page` consumers. Audit `(u_int64_t *)(uintptr_t)(... &
  PTE_ADDR_MASK)` casts — each becomes `md_phys_to_virt(...)`.
- Verify after each cluster: boots + the desktop/daemons still run.

### Phase 4 — TTBR0 = user only; drop the kernel-identity copy
- `pmap_create_user_space` no longer copies `l1_table`; a fresh user L1 is **empty** (kernel
  lives in TTBR1, shared, set once). Faults below the user split resolve via the demand/COW
  paths or SIGSEGV.
- `mmu.c`: drop the TTBR0 identity blocks (or keep a tiny low map only if early boot needs
  it). TTBR0 now starts empty per process.
- **Verify the payoff:** clang (static ET_EXEC @0x200000) loads + demand-pages + runs;
  existing PIE apps (views/tcsh/vDoom) still run.

### Phase 5 — Cleanup
- Remove dual-mapping scaffolding, stale identity assumptions, and the demand-paging debug
  kprintfs. Update `pmap.c:227` / `mmu.c` comments. Re-baseline both arches green.

## Multi-arch (see `feedback_multiarch_first`)

x86_64 is **also kernel-low** today ("kernel LOW not higher-half, user VA ≥1GB" —
`project_x86_64_port_progress`), so it has the same latent collision. The higher-half move
applies there too (PML4 higher-half + physmap, `P2V` already exists as the seam). It is a
**separate x86_64 effort** — do aarch64 first (primary target, where clang must run), keep
x86_64 building green throughout, and mirror the design so x86_64's later move is mechanical.
The MI demand-paging layer already works for both; only this MD relocation differs.

## Progress

| Phase | Item | Status |
|---|---|---|
| 1 | TTBR1 physmap, dual-mapped | **DONE + verified 2026-06-26** — `physmap_l1` (1 GB identity blocks) at `PHYSMAP_BASE=0xFFFFFF8000000000`; TCR T1SZ=25/TG1=4KB/EPD1=0; `aarch64_physmap_verify` prints `MATCH` (lo & hi reads of phys 0x40200000 agree); boot unregressed |
| 2a | Boot restructure + trampoline machinery (`VIRT_OFFSET=0`, stays low) | **DONE + verified 2026-06-26** — `ldscript.aarch64` parameterized on `VIRT_OFFSET` (`.text.boot` low stub + high `.text` via `AT()`); `start.S` rewritten: low stub sets temp phys stack, zeroes BSS at phys, calls `aarch64_mmu_init` at its phys address, branches to `_start_high`; `mmu_init` call removed from `kmain`. Boots clean, no regression. |
| 2b | Flip `VIRT_OFFSET` to high; kernel executes high | **DONE — kernel boots high through userland under tcg, 2026-06-26.** Fixes that got it there: **SMP AP path** — `apsmp.c` takes `&secondary_entry` through a `volatile` absolute-relocated pointer (`const` let the optimizer fold it back to an out-of-range `adrp`); `apentry.S` rewritten as a low stub (phys stack → enable MMU at phys → branch to `secondary_entry_high`), MMU-enable removed from `c_ap_boot_arm`. **`ptrdiff_t` was hard-coded `int32_t`** (sys/types.h) → truncated 64-bit pointer casts (lwIP `LWIP_CONST_CAST` asserted on a high stack VA); fixed to `__PTRDIFF_TYPE__` — a real latent 64-bit bug. **`_end` bitmap base** → `AARCH64_PHYS_OF(_end)` (Phase-3 site #1, `PHYSMAP_BASE`/`AARCH64_PHYS_OF`/`AARCH64_VIRT_OF` now in `bringup.h`). Kernel runs from TTBR1, daemons launch + DHCP binds + automountd serves mounts. HVF still hits the *separate* clang-MMIO `(isv)` codegen issue — test the high kernel under **tcg**. |
| 3 | Route remaining phys→ptr through the physmap (unwind identity) | in progress — `_end`/bitmap done; the kernel still works only because TTBR0 keeps the kernel-identity copy (pmap_create_user_space). Phase 4 removes that, so first ALL kernel phys-as-pointer access (`md_phys_to_virt`, the pmap table walks, the bitmap access, ELF `kframe`) must route through the TTBR1 physmap. |
| 4 | TTBR0 user-only; drop kernel-identity copy; **clang runs** | not started — **this is the clang unblock.** Today the high kernel still copies the kernel identity into every TTBR0 (so a static low binary still collides); only after Phase 3 lets the kernel reach physical memory via the TTBR1 physmap (not the TTBR0 low identity) can `pmap_create_user_space` hand out an empty TTBR0, freeing the low VAs for clang. |
| 5 | Cleanup + both arches green | not started |

## Risk

Every phase can brick the boot (MMU/VBAR/stack on the wrong VA). Mitigate: keep the low
identity dual-mapped until Phase 4, advance one cluster at a time, boot-test each under tcg
(addresses visible) before HVF. Keep a known-good kernel to bisect against.
