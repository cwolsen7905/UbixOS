# UbixOS SMP Design Plan (i386)

## Goal

Boot the secondary CPU cores and run the kernel scheduler across all of them
safely — going from today's single-CPU kernel to genuine symmetric
multiprocessing, where any runnable thread can execute on any idle core.

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
| 2.2| `_current` → per-CPU macro (`curcpu()->current`) | ✅ | uniprocessor no-op |
| 2.2| Repoint `_int7` FPU asm off `_current` symbol | ✅ | offset static-assert |
| 2.3| APs adopt kernel CR3 + enable paging | ✅ | APs in kernel address space |
| 2.3| Per-CPU idle threads (created, pinned) | ⬜ | needs scheduler entry |
| 3  | Real per-CPU identity (`%gs`-base / LAPIC lookup) | ⬜ | see Phase 3 prerequisites |
| 3  | LAPIC remap into shared kernel PD range | ⬜ | prereq for LAPIC-id lookup |
| 3  | True spinlock type (spin, not yield; IRQs off) | ⬜ | current `spinLock` yields |
| 3  | Per-CPU LAPIC timer + reschedule IPI | ⬜ | |
| 3  | LAPIC EOI path | ⬜ | still 8259-only |
| 4  | SMP scheduling — global run queue under one lock | ⬜ | two cores run threads |
| 5  | TLB shootdown IPIs | ⬜ | |
| 6  | Per-CPU run queues + load balancing + affinity | ⬜ | optimization layer |

Backstops: tag `smp-scaffold-safe` (pre-`_current` refactor) and the per-phase
commits.  Phases 0–2.3 landed as independently-bootable commits; Phase 3+ must
land as a unit (see "Phase 3 prerequisites discovered").

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
| Per-CPU data / `_current` | **Global** (`sys/kern/sched_core.c`: `kTask_t *_current`). A DR3-register hack marks the cpu struct but nothing uses it. |
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

### Phase 2 — Per-CPU infrastructure
- Define `struct pcpu { kTask_t *current; u_int32_t cpuid, apicid; kTask_t *idle;
  u_int32_t flags; ... }`. Allocate one per CPU.
- Establish `smp_processor_id()` and `curcpu()` — set each CPU's `%gs` base (or
  TR) to its `pcpu` at AP start; BSP too.
- Mechanically replace global `_current` with `curcpu()->current` everywhere
  (compat macro first, then migrate sites).
- Give each CPU an **idle thread**.
- **Test:** single-CPU boot routes through `pcpu[0]` with zero behavior change;
  APs still parked.
- **Risk:** high churn (pervasive `_current` edit), low conceptual risk.

### Phase 3 — SMP-safe locking + LAPIC interrupts
- Rewrite `spinLock()` to spin with `pause`; add `spin_lock_irqsave`-style
  preemption/interrupt disable while held. Audit every existing lock holder for
  "sleeps while holding" bugs.
- LAPIC EOI; per-CPU LAPIC timer for the scheduler tick (replace PIT-drives-
  scheduler on APs; BSP can keep PIT for timekeeping).
- Reschedule IPI (a CPU can poke another to re-run the scheduler).
- **Test:** APs take LAPIC timer ticks and idle-loop without corrupting state;
  contended lock between BSP and a still-idle AP makes progress.
- **Risk:** high — this is where deadlocks/races first appear.

### Phase 4 — SMP scheduling (single global run queue under lock)
- One global run queue + one scheduler lock (big-scheduler-lock). Every CPU, in
  its idle loop, takes the lock, dequeues a runnable thread, runs it, re-enqueues
  on quantum expiry.
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
- Replace the global run queue with per-CPU queues + a load balancer; CPU
  affinity. Drop the big-scheduler-lock to per-queue locks.
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
