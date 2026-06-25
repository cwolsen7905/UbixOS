# In-Kernel Preemption — Implementation Plan

**Status:** drafted 2026-06-25. Implementation drill-down for `smp-plan.md`'s
Phase 3 ("the shared `preempt_count` + true-spinlock primitive"). Read the
*why* there; this is the *how* — concrete, ordered, each step independently
testable, single-CPU correctness never regresses.

## Goal

One primitive that simultaneously (a) makes the kernel **safely preemptible** on
both arches and (b) gives SMP **preempt-disabling critical sections**. Today:

| Arch | EL1/ring-0 preemption | Lock |
|---|---|---|
| x86_64 | preemptible (unconditional `sti` ends a dispatch) — uniprocessor-correct | `spinLock()` **yields** → deadlock-prone under true SMP |
| aarch64 | **non-preemptible** (EL1 tick only runs callouts; cooperative) | same yielding `spinLock()` |

Target: a timer tick reschedules **iff `preempt_count == 0`**, and a held lock
raises `preempt_count` — so "may this tick preempt?" and "is this critical
section safe?" derive from the **same per-CPU counter**.

## The primitive

- **`preempt_count`** — per-CPU, in `struct pcpu`. `0` = preemptible.
- **`resched_pending`** — per-CPU flag: a tick that wanted to switch but found
  `preempt_count > 0` sets this instead of switching.
- **`preempt_disable()` / `preempt_enable()`** — `++` / `--`; `preempt_enable()`
  runs a deferred `sched()` if `resched_pending && preempt_count == 0`.
- **`spin_lock()` / `spin_unlock()`** — spin (`pause` / `wfe`), **IRQ-save +
  `preempt_disable()`** on acquire; **`preempt_enable()` + IRQ-restore** on
  release. **No `sched_yield()`** — a lock holder is never descheduled.

## Steps (x86_64 first — it already runs real SMP, so the locking change is
validated where it matters; aarch64 inherits the MI primitive)

1. **Scaffolding (no behavior change).** Add `preempt_count` + `resched_pending`
   to `struct pcpu` (both arches); add `preempt_disable/enable[_no_resched]()` and
   `should_resched()` in an MI header. Count stays 0 everywhere → identical
   behavior. **Gate:** both arches boot unchanged; `-smp 2/4` unchanged.
2. **Defer reschedule under count.** The timer-tick reschedule path checks
   `preempt_count == 0`; if held, set `resched_pending` instead of switching.
   `preempt_enable()` drains it. Still no caller raises the count, so still a
   no-op — but the machinery is now load-bearing. **Gate:** boot + desktop both
   arches.
3. **True spinlock — scheduler lock first.** Implement the spin+IRQ-save+
   preempt-disable lock; convert the **single hottest, most-critical lock**
   (`schedulerSpinLock`) to it. **Gate:** `-smp 2` then `-smp 4` desktop + a
   fork/exec stress stay fault-free.
4. **Migrate remaining `spinLock()` callers.** Sweep the tree; convert to the
   true lock (or document why a given site stays cooperative). **Gate:** SMP
   stress, both arches.
5. **aarch64: turn on EL1 preemption.** The aarch64 EL1 timer tick currently only
   runs expired callouts (`exceptions.c`); now it also reschedules when
   `preempt_count == 0`. This is the point the aarch64 kernel becomes
   preemptible. **Gate:** `-smp 4` stress; long in-kernel operations (FAT/ubixfs
   I/O) no longer block peers on the same core.

## Risk discipline

- Every step boots **both** 64-bit arches and `-smp 1/2/4`. Single-CPU must never
  regress (it's the cheapest signal that the counter accounting is balanced).
- x86_64 is the proving ground (Phase-4 SMP already verified); aarch64 step 5
  lands only after steps 1–4 are stable on x86_64.
- Balanced-count is the invariant: an unbalanced `disable`/`enable` either wedges
  preemption (count stuck > 0) or under-protects (count goes negative) — assert
  `preempt_count >= 0` in debug builds.
