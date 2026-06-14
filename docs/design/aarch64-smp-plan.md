# AArch64 SMP Bring-Up Plan

Status: **design** (2026-06-14). Target series: **3.x**. Owner: SMP track.

Brings the aarch64 port from *single-core with CPU enumeration* to *application
processors running real tasks*, reaching **parity with the i386 SMP Phase 3** that
shipped (opt-in) in 2.4.0-BETA. After this lands, the **x86_64 port** is brought up
to parity with aarch64 (both LP64, both SMP) — see `cross-arch-plan.md`.

---

## 1. Where aarch64 is today

Done (Phase 1 — enumeration only):

- `aarch64_enum_cpus()` (`vmm/vmm_machdep.c`) walks the DTB `/cpus` node into the MI
  `cpu_enum` table: each core's MPIDR (hwid), `enable-method` (`psci` / `spin-table`),
  and the boot CPU. **Discovery only — it starts nothing.**
- Shared MI foundations that already help: `spinLockIrq`/`spinUnlockIrq` (true
  spinlock), the run-queue critical section (already SMP-safe — `sched_dispatch.c`
  guards it under `schedulerSpinLock`), and the per-CPU idle *dispatch* (added for
  i386 in `6755e79d1`, currently `#if __i386__`-guarded).

Missing — **everything else**. aarch64 has no per-CPU machinery at all:

| Concern | i386 (shipped) | aarch64 (today) |
|---|---|---|
| Per-CPU "current"/block | per-CPU `%gs` → `g_pcpu[]` | **`_current` is one global**; `tpidr_el1` unused |
| Start a secondary core | LAPIC INIT-SIPI + trampoline | **nothing** (PSCI only *parsed*) |
| Per-CPU interrupt ctrl | per-CPU LAPIC | GICv2 `gic_init()` inits the calling CPU's GICC, but is **only called on the BSP** |
| Per-CPU scheduler tick | per-CPU LAPIC timer | CNTV virtual timer (PPI 27) — **BSP only** |
| AP scheduler entry | `c_ap_boot` | **nothing** |
| Per-CPU idle | `g_pcpu[id].idle`, dispatched OOB | MI dispatch exists but aarch64 keeps one enqueued idle |

Useful aarch64 specifics confirmed in-tree:

- **GICv2**: `GICD@0x08000000`, `GICC@0x08010000` (`dev/gic.c`). The **distributor
  (GICD) is global**; the **CPU interface (GICC) + the SGI/PPI enables are banked
  per-CPU** — each core programs its own GICC. No GICv3 redistributor to worry about.
- **Virtual timer (CNTV)**: PPI **INTID 27**, periodic via `cntv_tval_el0`
  (`dev/timer.c`). PPIs are banked, so each core arms its own timer.
- **`tpidr_el0` is taken** (user TLS, `syscall_md.c`), so **`tpidr_el1` is free** for
  the per-CPU kernel block — the clean analog of i386's per-CPU `%gs`.
- **PSCI** is the QEMU-virt enable-method; a `CPU_ON` needs an **SMC/HVC** call, which
  does not exist yet (only the `enable-method` *string* is parsed).
- The kernel is **non-preemptible**: `exceptions.c` calls `sched()` only from the
  **EL0** IRQ path (EL1 handlers run with IRQs masked). True SMP keeps this model —
  each core preempts only its own EL0 — so no new preemption semantics are needed.

---

## 2. Goal & parity definition

Match i386 SMP Phase 3:

- Secondary cores leave firmware, run kernel C, and **pull READY tasks off the shared
  run queue** driven by their own timer.
- Each core has a **per-CPU idle** that is never enqueued (dispatched when its run
  queue is empty), reusing the MI dispatcher path.
- Shipped **opt-in, default off** (`SMP_ENABLE_APS` analog) until the true-SMP
  hardening (per-CPU FPU/NEON + TLB shootdown) lands, because the same uniprocessor
  assumptions that gate i386 (one global lazy-FP owner; no cross-CPU TLB shootdown)
  apply to aarch64.

---

## 3. Milestones (each independently testable)

### M0 — per-CPU foundation (`tpidr_el1` → `struct pcpu`)
The big MD change, the analog of i386's `9efd904de`. Do this **first and verify the
BSP still boots** before starting any AP.

- Add an aarch64 `struct pcpu { cpuid; mpidr; struct taskStruct *current; *idle; … }`
  and `g_pcpu[MAXCPU]`, mirroring `i386/pcpu.h`.
- On the BSP (and later each AP): `msr tpidr_el1, &g_pcpu[id]`. Add `curcpu()` =
  read `tpidr_el1`.
- Make `_current` resolve to `curcpu()->current` on aarch64 (today it's a global) —
  the analog of i386's `get_current()`/`set_current()`. Audit the ~dozen
  `_current` sites in `kern/syscall*.c` (they keep working through the macro).
- **Test:** BSP boots to desktop on `-smp 2`, identical behaviour. No AP yet.

### M1 — PSCI `CPU_ON` + secondary entry (liveness)
- Add a PSCI call wrapper: `smc`/`hvc` with function id `CPU_ON` (`0xC4000003`),
  passing the target MPIDR and a physical entry point. (QEMU-virt uses HVC by default;
  honour the DTB `psci` node's `method`.)
- Write the **secondary entry**: a small routine that lands at EL1 with the MMU off,
  enables the MMU with the shared kernel page tables (reuse `mmu.c`/`TTBR1`), sets
  `tpidr_el1 = &g_pcpu[id]`, then calls a C `c_ap_boot_arm()`.
- First version of `c_ap_boot_arm()` just **bumps `g_pcpu[id].heartbeat`** so the BSP
  can prove the AP executes kernel C in parallel — the exact liveness pattern i386
  used before wiring the scheduler.
- **Test:** BSP observes each AP's heartbeat advance; APs then park (`wfi` loop).

### M2 — per-CPU GIC + timer
- Each AP calls `gic_init()` (its own GICC: PMR + CTLR) and enables the **banked**
  timer PPI (INTID 27) + any SGIs.
- Each AP arms its own CNTV (`timer_init()` per-CPU).
- Distributor stays BSP-only; confirm SPI routing/affinity is sane for >1 core.
- **Test:** an AP takes its own timer IRQ (instrument `timer_tick`/the EL1 IRQ path on
  the AP) without disturbing the BSP.

### M3 — AP scheduler entry (true SMP)
- Extend the MI per-CPU idle dispatch (`sched_dispatch.c`) to aarch64: define
  `cpu_idle = curcpu()->idle` for aarch64 too (drop the `#if __i386__` to include
  `__aarch64__`), and create one non-enqueued idle per CPU in `boot.c` (the analog of
  the i386 `main.c` loop). aarch64 currently sets one enqueued `g_idle_task` in
  `execfile.c` — make it the BSP's `g_pcpu[0].idle`, un-enqueued.
- `c_ap_boot_arm()`: after M1+M2, enable IRQs and idle; the per-CPU CNTV IRQ drives
  `sched()` on that core, which pulls READY tasks from the shared queue. `_current`
  starts NULL → first `sched()` discards the boot context (the `prev==NULL` path) like
  the BSP discards kmain.
- Gate behind the `SMP_ENABLE_APS` analog + a deferred release thread (don't let APs
  race the not-yet-SMP-safe early boot), exactly as i386.
- **Test:** under `-accel tcg` + gdb, AP runs a CPU-bound task while the BSP runs the
  desktop; watch for the latent single-CPU bugs below.

### M4 — true-SMP hardening (shared with i386; flips SMP on by default)
This is what lets **both** arches drop the opt-in gate:

- **Per-CPU FP/SIMD ownership.** i386's lazy-FPU owner `_usedMath` is one global;
  aarch64 has the analogous NEON/FPU lazy-save. Make the "who owns the vector unit"
  state per-CPU (`g_pcpu[].fpowner`), or switch to eager save/restore.
- **TLB shootdown.** Neither arch has it. When one core changes shared/another core's
  page tables, IPI the others to invalidate. aarch64 can broadcast `TLBI *IS`
  (inner-shareable) for many cases — often cheaper than i386, which needs an SGI/IPI +
  `invlpg`. Use a GIC **SGI** for the cross-core flush where broadcast TLBI isn't
  enough.
- Audit remaining unlocked global kernel state surfaced by M3 testing.

---

## 4. After aarch64 SMP: x86_64 to parity

Once aarch64 runs APs, bring up **x86_64** (`cross-arch-plan.md` / the x86_64
migration plan) and take it to **parity with aarch64**: both LP64, both booting the
desktop, both running APs. x86_64 inherits the i386 SMP design almost directly (LAPIC,
per-CPU via `%gs`/`GS.base` MSR, AP entry) — the main new MD work is long-mode boot,
4-level paging, and SYSCALL/SYSRET, not the SMP logic. The M4 hardening (per-CPU FP +
TLB shootdown) is shared across all three arches.

---

## 5. Risks & method

- **M0 is the riskiest single step** (touches every `_current` read); land + verify it
  in isolation before any AP, like i386.
- Each milestone is a triple-fault/hang risk on the BSP — use the headless fault-catch
  harness (serial + `-no-reboot`, `-accel tcg` for deterministic faults).
- Keep i386 + the rest of aarch64 green throughout; the only MI file touched is
  `sched_dispatch.c` (extend the existing per-CPU idle guard to `__aarch64__`).
- **Opt-in until M4.** Default-off SMP ships safely; flipping the default is gated on
  the per-CPU-FP + TLB-shootdown hardening, for both arches at once.
