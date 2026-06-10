# Backlog Roadmap (2026-06-10)

Synthesized from a full review of the active `docs/design/*.md` plans against the
current code.  Several plans turned out done / superseded; the review also
surfaced two real leaks.  Work is ordered by value × (1/risk) × product-identity
alignment.  Check items off as they land.

## Review outcome (plan status)

| Plan | Status | Note |
|------|--------|------|
| console-and-arch-convergence | ✅ core done | remaining = fbcon (3.5) + image profiles (4) |
| cross-arch | ✅ features done | remaining = COW (reverted), TTBR1 (deferred) |
| vmm | 🟢 P1–3 done both arches | RB-tree VMAs, lazy/file mmap, swap, pageout, madvise |
| threads-refactor | ✅ v1 done | rest deferred post-v1 (64-bit signals, kTask split) |
| network-modernization | 🟢 P0/3/5 done | rest cosmetic / a reverted NAPI experiment |
| ubixfs-pool | 🟢 host core 100% | kernel VFS driver not started |
| ubixfs2 | ⬜ SUPERSEDED | dead code, delete |
| views-polish | 🟢 ~70% | small polish items left |
| sound-server (aural) | 🟢 ~50% | mixer server not built; /dev/audio works single-client |
| activity-monitor | ⬜ not started | needs kernel tick accounting first |
| smp (i386) | 🟢 P0–2.3 | APs built but never launched; P3 interlocked |
| clang-selfhost | 🟢 P1–4 | P5–8 (disk grow, cross-compile, native build) ahead |
| session | 🟢 P0 done | session_id field + SessionManager + lock screen left |
| fbcon | ⬜ not started | the console-first sink |

## Real bugs found (not features)
- **aarch64 exec leaks all old user pages** (`sys/arch/aarch64/kern/execfile.c`,
  "leak for now") — no `pmap_free_user_space()`.  Also a prerequisite for COW.
- **vmm anon-teardown leak** (~51 pages/process-cycle) + **shared-region leak**
  (~3 MB/logout) — need a per-frame refcount.

---

## Tier 1 — quick wins / hygiene (low risk)
- [ ] Delete superseded **ubixfs2** dead code (`sys/fs/ubixfsv2/`, `lib/ubixfs2_core/`, `tools/ubixfs2/`, `include/fs/ubixfs2/`) — verify unbuilt first
- [ ] **aarch64 `pmap_free_user_space()`** — free old user pages on exec/exit (fixes per-exec leak; unblocks COW)
- [ ] **chmod / symlink / mprotect** syscalls (currently `sys_invalid`) — POSIX completeness
- [ ] **views-polish** leftovers (network tray, login caret/clock, submenu hover)

## Tier 2 — console-first product identity (closes the convergence plan)
- [ ] **fbcon (Phase 3.5)** — on-screen text console sink (VESA LFB + virtio-gpu), built-in 8×16 font, KC_PRIMARY|KC_SUSPENDABLE; boot log / safe-mode / panic without `views`
- [ ] **Image profiles (Phase 4)** — base (headless/console) vs desktop, one tree, `mkimage*` profile knob + `init` launch selection

## Tier 3 — one bigger feature (pick by goal)
- [ ] **ubixfs-pool kernel driver** — wire the finished host lite-ZFS core into the VFS (native CoW root, path off FAT)
- [ ] **aarch64 COW fork (landed safely)** — exclude `sys_mapfb`/`vmm_share_region` pages from the COW walk; + **TTBR1 kernel/user split**
- [ ] **SMP (i386)** — launch APs, per-CPU %gs + real spinlock + LAPIC timer + IPIs

## Tier 4 — defer
- sound-server (`aural` mixer), activity-monitor (after tick accounting),
  clang self-host (long horizon), session formalization, vmm anon-leak hunt,
  network Phase 1/2 cosmetics, threads kTask split (with SMP).
