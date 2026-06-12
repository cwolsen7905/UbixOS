# UbixOS SMP + In-Kernel Preemption Design Plan

> **Scope (decided 2026-06-10):** SMP and in-kernel preemption are **one
> problem, one plan**. The bring-up phases below are i386-first (that is where
> the trampoline + per-CPU `%gs` work already lives); the **preemption** thread
> is cross-arch, because making aarch64 SMP-capable *requires* first making its
> (deliberately non-preemptible) kernel safely preemptible — and the primitive
> that does that is the same true-spinlock + preempt-count this plan builds for
> SMP. See **"In-Kernel Preemption (interlocked with SMP)"** below.

## Goal

Boot the secondary CPU cores and run the kernel scheduler across all of them
safely — going from today's single-CPU kernel to genuine symmetric
multiprocessing, where any runnable thread can execute on any idle core — **and,
as the same body of work, give both architectures a uniform, safe in-kernel
preemption model.**

This is a **large, staged** effort. The danger is not the hardware bring-up
(the trampoline already works) — it is that essentially every shared kernel
structure today assumes exactly one CPU. The plan below sequences the work so
that each phase is independently testable and the kernel stays bootable
throughout. Single-CPU correctness must never regress.

---

## Status Matrix

Legend: ✅ done & verified · 🟡 partial · ⬜ not started

| Phase | Item | Status | Commit / Notes |
|-------|------|--------|----------------|
| 0  | Build SMP objects + relocate `smp.h` to `i386/` | ✅ | boots `-smp 1/2/4` |
| 0  | Implement missing `spinLockLocked()` | ✅ | |
| 1a | Wire `smpInit()` into boot; map LAPIC; register BSP | ✅ | logs BSP apic id/brand |
| 1a | Drop broken EFLAGS.ID CPUID-presence panic | ✅ | |
| 1b | INIT-SIPI the APs; trampoline at `0x0` + IVT save/restore | ✅ | |
| 1b | Enumerate cores (`ap_online`) | ✅ | `-smp N` → N cores |
| 2.1| `struct pcpu` + `g_pcpu[]` + `curcpu()`/`smp_processor_id()` | ✅ | each core self-registers |
| 2.2| `_current` → per-CPU (`%gs:8`, SEL_PCPU) | ✅ | DONE via the software-task-switch work (Milestone B — see `completed/software-task-switch-plan.md`). The original *macro* attempt was reverted; the working approach is a bare `%gs`-relative load through `get_current()`/`set_current()` in `sched.h`, with `cpu_switch` (and every kernel entry stub via `PCPU_LOAD_GS`) forcing `%gs = SEL_PCPU` so `%gs:8` is valid in all kernel contexts |
| 2.2| Repoint `_int7` FPU asm off `_current` symbol | ✅ | superseded by the above — exception stubs reload `%gs = SEL_PCPU`, so the lazy-FPU path reaches per-CPU current via `%gs:8` like everything else |
| 2.3| APs adopt kernel CR3 + enable paging | ✅ | APs in kernel address space |
| 2.3| APs run a per-CPU idle loop in parallel | 🟡 | busy `heartbeat++;pause` loop; verified advancing on all cores.  Real `sti/hlt` idle + scheduler-dispatched threads pending Phase 3 |
| 2.3| Per-CPU idle *threads* (scheduler-dispatched) | ⬜ | needs per-CPU identity + SMP-safe IRQs |
| 3  | Real per-CPU identity (`%gs`-base / LAPIC lookup) | 🟡 | `%gs`-base half DONE (task-switch work: `g_pcpu[]`, `%gs = SEL_PCPU` with base `&g_pcpu[cpu]`, `pcpu_install_gs()` at boot). LAPIC-id lookup for AP self-identification still pending |
| 3  | LAPIC remap into shared kernel PD range | ⬜ | prereq for LAPIC-id lookup |
| 3  | True spinlock type (spin, not yield; IRQs off) | ⬜ | current `spinLock` yields |
| 3  | `preempt_count` (per-CPU/thread) gating in-kernel preemption | ⬜ | the shared SMP-locking + preemptible-kernel primitive (see "In-Kernel Preemption") |
| 4+ | **aarch64 SMP track** — PSCI `CPU_ON` + GICv2/SGI + TTBR1 + preemptible aarch64 kernel | ⬜ | reuses the `pcpu`/`preempt_count` contract; gated on i386 Ph 3-4 |
| 3  | Per-CPU LAPIC timer + reschedule IPI | ⬜ | |
| 3  | LAPIC EOI path | ⬜ | still 8259-only |
| 3.5| Scheduler accounting — per-task `run_ticks`, per-CPU `busy/idle_ticks` | ✅ | done 2026-06-11, both arches boot-verified.  `sched_account_tick()` (MI, `sched_core.c`) charged once per timer IRQ from i386 `timerInt` + aarch64 `timer_tick`; idle tagged via `g_idle_task`; surfaced as `/proc/<pid>/stat` utime + new `/proc/stat`.  Uniprocessor (single busy/idle pair); per-CPU split deferred to Phase 4 |
| 4  | SMP scheduling — global run queue under one lock | ⬜ | two cores run threads |
| 5  | TLB shootdown IPIs | ⬜ | |
| 6  | Per-CPU run queues + load balancing + affinity | ⬜ | optimization layer |

Backstops: tag `smp-scaffold-safe` (pre-`_current` refactor) and the per-phase
commits.  Phases 0–2.3 landed as independently-bootable commits; Phase 3+ must
land as a unit (see "Phase 3 prerequisites discovered").

> **Parity note (2026-06-03):** the per-CPU `_current` blocker that Phase 2.2
> reverted has since been **solved** by the software-task-switch work
> (`completed/software-task-switch-plan.md`, Milestone B): `_current` is now a
> per-CPU `%gs:8` access, not a global. This also delivered the `%gs`-base half
> of Phase 3's per-CPU identity. The remaining SMP work is therefore the
> *multi-core* pieces — APs entering the scheduler, a true (non-yielding)
> spinlock, LAPIC timer/EOI/IPI, and SMP run-queue locking (Phase 4) — **not**
> the per-CPU-current foundation, which is done. The shared run queue Phase 4
> locks is scheduler-plan's existing `run_queue[32]` + `ready_mask`, so the two
> plans are complementary, not conflicting. The "Current State (honest
> inventory)" section below predates Phases 0–2.3 and this work; the status
> matrix above is authoritative.

---

## In-Kernel Preemption (interlocked with SMP)

This plan absorbs the "preemption" question raised in the 2026-06-10 completeness
review. The conclusion there: **a standalone "make the kernel preemptible" plan
is the wrong shape — preemption and SMP are interlocked, so the work lives
here.** The reasoning:

### The asymmetry today

| Arch | Kernel preemption | Mechanism |
|---|---|---|
| **i386** | **Preemptible** | `sched()` ends a dispatch with an unconditional `sti()`; the PIT timer can reschedule while in kernel code. |
| **aarch64** | **Non-preemptible by design** | Syscalls run IRQ-masked; only EL0 is preempted by the 100 Hz virtual timer. Kernel threads must cooperatively `sched_yield()`. (See `cross-arch-plan.md` "Preemptible EL0"; `project_aarch64_sched_jobcontrol`.) |

On a **uniprocessor**, aarch64's cooperative kernel is a legitimate, simpler
choice and is *not* a bug — it is a deliberate design decision. The reason it
becomes a gap is **SMP**: two CPUs executing kernel code at the same instant is
concurrency whether or not you call it "preemption," and a cooperative,
single-lock-free kernel cannot survive it.

### Why one mechanism serves both

The primitive that makes a kernel *safely preemptible* and the primitive SMP
needs for *shared-state safety* are the same two things:

1. **A true spinlock** (spin with `pause`, do **not** `sched_yield()`) that
   **disables preemption and local IRQs while held** — already called out as
   Phase 3's "spinlocks that actually spin."
2. **A per-CPU/per-thread `preempt_count`.** A timer tick may reschedule *only*
   when `preempt_count == 0`. `spin_lock_irqsave` raises it; `spin_unlock`
   lowers it and runs a deferred reschedule if one is pending.

With those, "should this timer tick preempt the current kernel thread?" and
"is it safe for another CPU to touch this structure?" have the **same answer**,
derived from the **same counter**. Build it once:

- **i386** is already preemptible but its `spinLock()` *yields* — which is
  correct on a uniprocessor and a **deadlock under true SMP**. Phase 3 replaces
  it with the spin+preempt-count lock; i386 in-kernel preemption becomes
  *governed by `preempt_count`* instead of unconditional `sti()`.
- **aarch64** moves from cooperative to preemptible-with-locks **only as a
  prerequisite to its own SMP** — it is not worth doing for a uniprocessor. So
  aarch64 SMP is explicitly gated on (a) this `preempt_count` + true-spinlock
  work, **plus** (b) the aarch64-specific multi-core bring-up that has no i386
  analogue: **PSCI `CPU_ON`** secondary start (vs. INIT-SIPI), **GICv2
  per-CPU/SGI** routing (vs. LAPIC/IPI), the **TTBR1 kernel/user split**
  (`cross-arch-plan.md`, deferred), and TLB shootdown via **`TLBI` broadcast**
  (mostly hardware-coherent on ARM, unlike the i386 shootdown IPI).

### Where it lands in the phases

- **Phase 3** is where the shared primitive is built: the true spinlock now
  *explicitly* carries a `preempt_count` (per-CPU, in `struct pcpu`, and/or
  per-thread). On i386 this re-bases in-kernel preemption onto the counter; it
  is the foundation aarch64 later reuses.
- **Phase 4** (single global run-queue lock) is the first consumer that *needs*
  preemption disabled while the lock is held.
- **aarch64 SMP track** runs *after* i386 Phases 3-4 prove the model — it is the
  same `pcpu`/`smp_*`/`preempt_count` contract (the MI scheduler is already
  arch-neutral) re-implemented over PSCI + GICv2, and is the point at which the
  aarch64 kernel is made preemptible. Tracked as a sibling of Phase 4, not a
  renumber of the i386 sequence.

**Net:** there is no separate preemption plan because there is no separate
preemption mechanism — `preempt_count` + a spinning, preemption-disabling lock
is the single thing that delivers SMP-safe critical sections on both arches
*and* makes the aarch64 kernel preemptible. The i386 SMP phases build it; the
aarch64 SMP track is what cashes it in for the second architecture.

---

## How Other Systems Did It

| System | Per-CPU "current" | Run queue | Cross-CPU invalidation | Bring-up |
|--------|-------------------|-----------|------------------------|----------|
| FreeBSD | `pcpu` via `%fs`/`%gs` base | per-CPU run queues + load balancer | TLB shootdown IPI | MD `mp_machdep.c` + MI `subr_smp.c` |
| Linux | `this_cpu` via `%gs` segment | per-CPU rq, CFS load balance | `flush_tlb` IPI / INVLPGB | `smpboot.c`, ACPI MADT |
| Solaris | per-CPU `cpu_t` | per-CPU dispatch queues | `xc_call` cross-calls | PSM modules |
| **UbixOS (plan)** | per-CPU struct via `%gs` base or LAPIC-id index | **single global rq under a lock first**, per-CPU rq later | TLB shootdown IPI | existing `ap-boot.S` + MADT/MP enumeration |

The universal lesson: **the machine-independent scheduler talks to a per-CPU
structure, never a global "current".** Getting that abstraction right is the
load-bearing change; everything else (locking, IPIs, balancing) builds on it.

We deliberately start with a **single global run queue protected by one lock**
(a "big scheduler lock"). It is slower than per-CPU queues but *correct* and far
simpler to get right; per-CPU queues are a later optimization (Phase 6), not a
prerequisite for "two cores running threads."

---

## Current State (honest inventory)

**Present but NOT compiled** — `sys/arch/i386/Makefile` has `ap-boot.o smp.o`
commented out ("SMP not yet wired up — smp.c needs `<string.h>` fix and AP
trampoline integration"). `smpInit()` is not in the boot `init_tasks[]` and is
never called.

| Piece | State |
|-------|-------|
| AP trampoline (`sys/arch/i386/ap-boot.S`) | Complete real-mode → pmode → C entry. Not built. |
| INIT-SIPI-SIPI (`smp.c apicMagic()`) | Written. Not built/called. |
| LAPIC access (`apicRead/apicWrite` @ `0xFEE00000`) | Read for CPU id/ver only. No timer, no EOI, no IPI dispatch wired. |
| CPU enumeration (ACPI MADT / Intel MP tables) | **Absent.** `cpuinfo[8]` is a static array; APs self-register. |
| Per-CPU data / `_current` | **DONE — per-CPU** via `%gs:8` (SEL_PCPU base `&g_pcpu[cpu]`), through `get_current()`/`set_current()` in `sched.h`; `cpu_switch` + entry stubs force `%gs = SEL_PCPU`. (Was global with an unused DR3 hack; superseded by the software-task-switch work.) |
| Spinlocks (`sys/arch/i386/spinlock.c`) | Real atomics (`__sync_*`, `xchg`), **but `spinLock()` calls `sched_yield()`** → deadlocks under true contention. |
| Run queue (`sched_core.c`) | Single global `run_queue[32]` + `ready_mask`. O(1), good — but one copy. |
| TLB shootdown | **Absent.** `invlpg` is local-only; cross-CPU staleness would corrupt memory. |
| Interrupts | Pure legacy 8259 PIC + PIT. EOI to master PIC only (`timer.S`). |
| Header placement | `sys/include/ubixos/smp.h` is x86-specific (apic_id, `apicMagic`, `cpuid`, EFLAGS) but lives in the MI tree — should be `sys/include/i386/smp.h`. |

**Bottom line:** ~30% of the parts exist in isolation, 0% integrated. The
trampoline and atomics are the easy 90%; the per-CPU refactor + SMP-safe locking
+ TLB shootdown are the 10% that "breaks everything."

---

## The Hard Problems (must each be solved)

1. **Per-CPU current-thread.** `_current` must become `pcpu[cpuid].current`.
   Reached from interrupt handlers, the scheduler, syscalls, and `fork`/`exec` —
   dozens of sites. Access path must be cheap (a `%gs`-relative load or an index
   by LAPIC id).
2. **Spinlocks that actually spin.** Remove `sched_yield()` from `spinLock()`;
   busy-wait with `pause`. Holding a spinlock must disable preemption (and
   usually local interrupts) so a lock holder can't be descheduled while a peer
   spins on it.
3. **CPU enumeration.** Parse the ACPI MADT (preferred) or the Intel MP
   FloatingPointer/MP table to learn how many CPUs exist and their LAPIC ids,
   instead of firing SIPIs blindly.
4. **LAPIC as the interrupt controller.** Per-CPU LAPIC timer for preemption,
   LAPIC EOI, and an IPI path for reschedule + TLB shootdown. (IO-APIC for
   device IRQ routing can come later; PIC can stay for devices initially.)
5. **TLB shootdown.** A page-table change on one CPU must invalidate the stale
   TLB entry on every other CPU via an IPI + handshake before the change is
   considered complete.
6. **Boot synchronization.** APs must spin in a holding pen until the BSP has
   built per-CPU state for them; a memory barrier / `mwait`-free handshake.

---

## Implementation status (as built)

Done and verified booting on `qemu -smp 1/2/4` (tag `smp-scaffold-safe` marks
the pre-`_current`-refactor point):

- **Phase 0** — `smp.c`/`ap-boot.S` compile and link again; `smp.h` relocated to
  `sys/include/i386/`; the declared-but-missing `spinLockLocked()` implemented.
- **Phase 1a** — `smpInit()` wired into the boot task list; LAPIC identity-mapped
  via `vmm_remap_io_page`; BSP self-registers (APIC id/ver, CPUID brand).  The
  legacy EFLAGS.ID "supports CPUID?" panic was removed (always true here).
- **Phase 1b** — `apicMagic()` INIT-SIPI-SIPIs the APs; each lands in
  `c_ap_boot()` (the trampoline MUST load at phys `0x0` — its 32-bit jump uses
  flat base-0 offsets — so we save/restore the clobbered real-mode IVT around AP
  startup).  Cores enumerate (`ap_online`).
- **Phase 2.1/2.2** — `struct pcpu` + `g_pcpu[]` + `curcpu()`/`smp_processor_id()`;
  each core self-registers its identity.  `_current` is now a per-CPU macro
  (`curcpu()->current`) on i386, a perfect uniprocessor no-op while
  `g_smp_active == 0`.  The `_int7` FPU stub's asm `_current` reference was
  repointed at `g_pcpu[0].current` with an offset static-assert.
- **Phase 2.3 (partial)** — each AP adopts the BSP's CR3 and enables paging in
  `c_ap_boot()`, so the APs now run in the kernel's virtual address space.  They
  remain parked in `cli; hlt`.

## Phase 3 prerequisites discovered (i386 specifics)

Making `curcpu()` resolve to the *running* CPU (not always `pcpu[0]`) is the gate
for everything below, and on i386 it is genuinely interlocked — there is no
standalone, boot-verifiable slice:

- **`%gs` is unavailable for free per-CPU.** Userland uses `%gs` for TLS (musl
  i386) and the kernel saves/restores it on every entry (`push %gs`/`pop %gs` in
  `sys_call.S`, `sys_call_posix.S`, `timer.S`, and the IDT stubs).  A `%gs`-base
  per-CPU pointer requires swapping in a kernel `%gs` after each save and is
  fragile: one missed entry/exit path corrupts user TLS.  Needs per-CPU GDT
  descriptors (one `%gs` descriptor per CPU, base = `&g_pcpu[i]`) plus a `self`
  pointer in `struct pcpu`.
- **The LAPIC-id lookup fallback faults outside the BSP's PD.** The LAPIC is
  identity-mapped at `0xFEE00000` = PD index 1019, which is **outside** the
  globally-synced kernel PD range (770–1015).  Reading it from an arbitrary
  process context (as `_current` would, everywhere) page-faults.  Activating
  `g_smp_active` therefore requires first remapping the LAPIC into shared kernel
  space, and it puts a slow MMIO read on every `_current` access.
- **`spinLock()` yields by design.** It calls `sched_yield()` while waiting,
  which is correct on a uniprocessor (the lock holder is another task that must
  be scheduled to release).  Pure spinning would deadlock single-CPU.  SMP needs
  either a second, true-spinlock type for short cross-CPU critical sections
  (scheduler run queue) with preemption/IRQs disabled while held, or an audit of
  every holder to guarantee it never sleeps.
- **No per-CPU LAPIC timer.** An idle AP can only notice new work via a tick or
  a reschedule IPI; the scheduler is still driven by the PIT on the BSP only.

Consequence: Phase 3 must be developed and tested as a **unit** (per-CPU
identity + true spinlock + LAPIC timer + a minimal AP idle/scheduler entry),
not as the independently-bootable commits Phases 0–2 allowed.  `smp-scaffold-safe`
and the per-phase commits are the backstops.

## Lessons learned (the reverted Phase 2.2)

1. **`_current` must be a memory access, never a macro.**  Making it
   `curcpu()->current` turned a global read into a token substitution that
   changed codegen across 56 files and injected a function call into the FPU
   lazy-restore asm (`_int7`/`mathStateRestore`), between a paired `fnsave`/
   `frstor` with no declared clobbers.  Result: nondeterministic FPU/state
   corruption that smashed the FPU-heavy compositor.  Per-CPU current must come
   from a `%gs`-relative *memory operand*, valid anywhere a plain global was.
2. **Nondeterministic bugs need multi-boot bisects.**  The 2.2 crash fired only
   sometimes; a single "clean" boot at a commit meant nothing.  The first fix
   looked good on one lucky boot and shipped broken.  Re-bisecting with **4
   boots per commit** found it cleanly (`smp-scaffold-safe` 4/4 clean, the macro
   commit crashes, the revert 4/4 clean).  Treat any intermittent fault this way.
3. **A headless repro beats clicking.**  A temporary vlogin auto-login +
   taskbar term-autolaunch reproduced the exact "launch a 2nd GUI app" crash
   without a human, making the bisect possible.  Keep that pattern handy.

---

## Phases

Each phase ends bootable and testable under `qemu -smp N`.

### Phase 0 — Housekeeping (no behavior change)
- Relocate `sys/include/ubixos/smp.h` → `sys/include/i386/smp.h`; fix the
  `#include` in `smp.c`. Split a tiny MI header (`smpInit()`, `smp_cpu_count()`)
  if/when MI code needs it; keep the APIC/CPUID bits MD.
- Get `smp.c` + `ap-boot.S` compiling again (fix the `<string.h>` issue) but
  **do not** launch APs yet — guard AP launch behind a runtime check.
- **Test:** kernel builds with SMP objects linked; single-CPU boot unchanged.
- **Risk:** none.

### Phase 1 — CPU enumeration + proof-of-life bring-up
- Parse ACPI MADT (fallback: MP table) to count CPUs and collect LAPIC ids.
- Enable the BSP LAPIC. Wire `smpInit()` into boot.
- Fire INIT-SIPI-SIPI; each AP lands in C, registers, logs **"CPU N online"**,
  then **parks in a `cli; hlt` loop** (touches no shared state).
- **Test:** `qemu -smp 4` logs 4 cores online; single-CPU still boots.
- **Risk:** medium (AP fault wedges the box) but contained — APs do nothing.
- **This is the bounded milestone** — multi-core detected and booted, scheduler
  untouched.

### Phase 2 — Per-CPU infrastructure  ✅ partial / ⛔ _current reverted
- ✅ Define `struct pcpu { kTask_t *current, *idle; u_int32_t cpuid, apicid; ... }`,
  the `g_pcpu[]` array, and `curcpu()`/`smp_processor_id()` (LAPIC-id lookup,
  short-circuited to cpu 0 while `g_smp_active == 0`).  Each core self-registers.
- ✅ APs adopt the kernel CR3 + paging and run a per-CPU idle loop in parallel.
- ⛔ **Do NOT make `_current` a macro.**  The attempt (`#define _current
  curcpu()->current`, even reduced to a plain `g_pcpu[0].current` access) caused
  nondeterministic memory corruption — a second GUI app launch smashed the
  compositor (EIP into a shared buffer) and triple-faulted.  It was reverted;
  `_current` stays a plain global.  **Per-CPU `current` is deferred to Phase 3
  and done via a `%gs`-relative access**, not a token-substitution macro that can
  inject a function call / different codegen into delicate asm paths (the FPU
  lazy-restore `_int7`/`mathStateRestore` was one such victim).

### Phase 3 — Real per-CPU base (%gs) + SMP-safe locking + LAPIC interrupts
This is the interlocked unit; land it together, test with **many** boots (the
2.2 bug was nondeterministic and single-boot bisects lied — see Lessons).
- **Per-CPU `%gs` base.** Add per-CPU GDT descriptors (one `%gs` descriptor per
  CPU, base = `&g_pcpu[i]`) + a `self` pointer in `struct pcpu`; load `%gs` on the
  BSP and each AP.  Swap in the kernel `%gs` after the existing `push %gs` in
  every entry path (`sys_call.S`, `sys_call_posix.S`, `timer.S`, IDT stubs) and
  restore on exit — this is the part that conflicts with user-TLS `%gs`, so do it
  carefully.  Then `_current` becomes `%gs:offsetof(current)` — a plain memory
  operand, valid in asm, never a call.
- **Spinlocks that spin + `preempt_count`.** Rewrite `spinLock()` to busy-wait
  with `pause`; add `spin_lock_irqsave`-style preemption/IRQ disable while held,
  implemented as a per-CPU/per-thread **`preempt_count`** (lock raises it,
  unlock lowers it + runs a pending reschedule). A timer tick reschedules only
  when `preempt_count == 0`. This is the single primitive that both makes
  critical sections SMP-safe *and* re-bases i386 in-kernel preemption off the
  unconditional `sti()` — and is what the aarch64 SMP track later reuses to make
  *its* kernel preemptible (see "In-Kernel Preemption (interlocked with SMP)").
  Audit every holder for "sleeps while holding".  (Today's `spinLock` *yields* —
  correct on uniprocessor, deadlock on SMP.)
- **LAPIC interrupts.** Map the LAPIC into the globally-synced kernel PD range
  first; LAPIC EOI; per-CPU LAPIC timer for the scheduler tick on APs (BSP keeps
  PIT for timekeeping); reschedule IPI.
- **Test:** APs take LAPIC ticks and idle without corrupting state; contended
  lock between BSP and a still-idle AP makes progress; the term-launch repro
  stays clean across ≥8 boots.
- **Risk:** high — deadlocks/races and the `%gs`/TLS interaction first appear here.

### Phase 3.5 — Scheduler accounting (uniprocessor-safe, SMP-ready) ✅ DONE 2026-06-11

**Status: shipped, both arches boot-verified.** Implemented ahead of the rest of
Phase 3 (it has no dependency on the per-CPU `%gs`/spinlock work).  As built:
- `kTask_t.run_ticks` (`u_int64_t`); a single MI `sched_account_tick()` in
  `sys/kern/sched_core.c` charges one tick to the running thread and to one
  busy/idle counter pair (`g_idle_task` distinguishes idle).
- Called **once per timer interrupt** — i386 `timerInt` (`timer.S`, after EOI,
  before the quantum-gated `sched`) and aarch64 `timer_tick` (`timer.c`) — *not*
  from `sched()` (which also runs on voluntary yields and would over-count).
- Surfaced in procfs: `/proc/<pid>/stat` field 14 (utime) = `run_ticks`; new
  global `/proc/stat` with `cpu`/`cpu0` busy+idle lines.  Ticks are raw timer
  ticks; userland forms HZ-independent ratios.
- Uniprocessor (one busy/idle pair, `cpu0` == aggregate).  The per-CPU array and
  extra `/proc/stat` lines arrive with Phase 4.

The original design notes below are retained for reference.

A small, low-risk slice that lands between the per-CPU timer work of Phase 3
and the multi-core dispatch of Phase 4.  Useful on its own (it unblocks an
`/proc`-driven activity monitor — see `activity-monitor-plan.md`) and it is
forced work for Phase 6's load balancer, so doing it here avoids retrofitting
the tick handler twice.

- **Per-task `run_ticks`.**  Add `u_int64_t run_ticks` (and optionally split
  `user_ticks` / `sys_ticks`) to `kTask_t`.  In the scheduler tick handler,
  bump `_current->run_ticks` (or the per-CPU equivalent — see below) before
  decrementing `quantum`.
- **Per-CPU `busy_ticks` / `idle_ticks`.**  Add to `struct pcpu`.  The tick
  handler bumps `curcpu()->busy_ticks` when `current != idle`, otherwise
  `idle_ticks`.  Until `g_smp_active == 0` this collapses to `g_pcpu[0]`,
  which is exactly what we want on uniprocessor.
- **Wall-clock anchor.**  A monotonically increasing `g_jiffies` (already
  effectively present as the PIT tick counter) plus `HZ` lets userland turn
  raw tick deltas into a percentage.
- **procfs surface.**  `/proc/<pid>/stat` already exists — extend it (or add
  `/proc/<pid>/schedstat`) with `run_ticks`.  Add a new `/proc/stat` with one
  `cpuN busy idle` line per CPU plus a `cpu` aggregate line.
- **Test (uniprocessor):** a CPU-bound spinner shows `run_ticks` climbing at
  ~`HZ`/sec while `idle_ticks` on cpu0 stalls; an `idle` loop reverses the
  ratio.  `bmake run` boot stays bit-identical.
- **Test (Phase 4 follow-up):** spawn N spinners on `-smp 2`, observe
  `busy_ticks` advancing on both `cpuN` lines.
- **Risk:** very low.  Pure additive fields + two `++`s in the tick handler.
  The one trap is making sure the tick handler reads `curcpu()` *before*
  `schedule()` swaps `current` out, so the just-finished slice is charged to
  the right task.

### Phase 4 — SMP scheduling (single global run queue under lock)
- **Reuse the existing structure as-is.**  `run_queue[32]` (32 per-priority
  *circular doubly-linked* lists via `rq_next`/`rq_prev`) + `ready_mask` + the
  `schedulerSpinLock` already *is* a single global run queue.  No restructuring:
  the doubly-linked design gives O(1) enqueue/dequeue and O(1) remove-from-middle
  (the `rq_prev` link), which SMP needs constantly (block, repriority, migrate).
  The all-tasks list `taskList` likewise stays global + lock-protected.
- Every CPU, in its idle loop, takes `schedulerSpinLock` (now a real spinlock),
  dequeues the highest-priority runnable thread, runs it, re-enqueues on quantum
  expiry.
- Migration safety: a thread's address space + FPU state must be consistent
  across CPUs before it runs elsewhere.
- **Test:** `qemu -smp 2`, spawn N CPU-bound threads, observe them progress on
  both cores (e.g. per-CPU tick counters); fork/exec stress; no panics.
- **Risk:** high — first time two cores mutate kernel state concurrently.

### Phase 5 — TLB shootdown
- TLB-shootdown IPI + handshake; hook every `invlpg`/CR3 reload site
  (`vmm_free_virtual_page.c`, `vmm_swap.c`, `vmm_page_fault.c`, COW paths).
- **Test:** mmap/munmap + COW fork stress across cores with no stale-TLB
  corruption.
- **Risk:** high — corruption here is silent; needs careful stress testing.

### Phase 6 — Per-CPU run queues + balancing (optimization)
- Replicate the same structure per CPU: `run_queue[NCPU][32]` + per-CPU
  `ready_mask` + per-CPU lock (the doubly-linked lists are unchanged, just one
  set per core).  Add a load balancer that migrates tasks between cores'
  queues — O(1) per task thanks to `rq_prev` — plus CPU affinity (which queue a
  task lives in).  Drop the big-scheduler-lock to per-queue locks.
- **Test:** throughput scales with core count; no starvation.
- **Risk:** medium; purely a performance/scalability layer over a correct base.

---

## File Organization (MD / MI split)

- **Machine-dependent (`sys/arch/i386/`):** `ap-boot.S`, `smp.c` (LAPIC, IPIs,
  INIT-SIPI, per-CPU `%gs` setup), `lapic.c`, `madt.c` (ACPI parse), TLB-shootdown
  IPI handler.
- **Machine-independent (`sys/kern/`):** the per-CPU-aware scheduler, `pcpu`
  glue, generic `smp_*` entry points.
- **Headers:** MD bits in `sys/include/i386/` (`smp.h`, `lapic.h`, `pcpu.h`); a
  thin MI `sys/include/ubixos/smp.h` exposing only `smpInit()`,
  `smp_cpu_count()`, `smp_processor_id()`.
- `armv6` keeps its own `ap-boot.S`; the MI scheduler stays arch-neutral so the
  ARM port can implement the same `pcpu`/`smp_*` contract later.

---

## Testing

- `qemu-system-i386 -smp 2` (and 4) added to a `run-smp` make target. QEMU
  enables the LAPIC automatically with `-smp`.
- Per-phase gates: Phase 1 = "N cores online" in `/var/log/messages`; Phase 4 =
  per-CPU tick counters advancing under load; Phase 5 = COW/mmap stress clean.
- Always re-run the **single-CPU** boot each phase — SMP work must not regress
  uniprocessor correctness.
- Keep an `-smp 1` and `-smp 4` smoke test in CI-equivalent scripts.

---

## Out of scope (initially)

IO-APIC device IRQ routing (legacy PIC stays for devices first); CPU hotplug;
NUMA; deep C-states / `mwait` idle; per-thread CPU affinity API for userland;
hyperthreading-aware scheduling. These layer on after Phase 6.
