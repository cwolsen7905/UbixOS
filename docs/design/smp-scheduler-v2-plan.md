# SMP Scheduler v2 — per-CPU run queues (design)

**Status:** design / for review (2026-06-20). No code yet.
**Owner decision pending:** approve the shape before implementation.

## Why a v2 (not more patches)

The current scheduler (`sys/kern/sched_core.c` + `sched_dispatch.c`) is the 2002
uniprocessor design: **one global run queue** protected by **one global
`schedulerSpinLock` held across the context switch**. Cooperative SMP was brought
up on it far enough to boot the aarch64 desktop and run vDoom, and the genuinely
dangerous bug it exposed — the sleep/wakeup context-switch **corruption** — is fixed
(`sched_block()`, the lock now held continuously across the sleep switch). But
real-display testing showed the *design* is too fragile for daily use:

- **Busy-spin vs. missed wakeup.** The cooperative AP idle loop (`sched_yield(); wfi`)
  either spins a core (~200k `sched()`/s observed) or sleeps and intermittently
  misses a wakeup → frozen desktop. There is no race-free "sleep until work" on a
  single global queue without heavy global-lock traffic.
- **Global-lock serialisation.** Every scheduling decision on every core serialises
  on one lock; the "hold it across the switch, release from the resumer" dance is
  subtle and was itself the source of the corruption.
- **Whack-a-mole.** Each incremental fix exposed the next facet of the same
  fragility; one idle-loop fix even turned an intermittent freeze into a hard hang.

The remaining issues are properties of the *shape*, so the fix is the shape:
**per-CPU run queues**, the standard modern SMP design.

## Goals / non-goals

**Goals**
- Per-CPU run queues + per-CPU locks; no global lock in the scheduling hot path.
- Race-free idle: a core sleeps when its queue is empty and is woken deterministically.
- Correct, cheap **per-process + per-CPU accounting** (user / kernel / IO / idle) — see below.
- Selectable at compile time; the existing global scheduler stays the default and the
  UP path stays byte-for-byte unchanged.
- Both arches (aarch64 primary, x86_64 anchor); MI core + thin MD seams.

**Non-goals (for v2.0)**
- Full CFS/EEVDF fairness math — keep the existing priority bands + aging first.
- Tickless / high-resolution timers — separate, larger modernisation (the 100 Hz
  tick stays; v2 doesn't depend on changing it).
- NUMA awareness — single memory domain on our targets.

## Design

### Per-CPU run queue
Each CPU owns a `struct runqueue`: the priority buckets + `ready_mask` (the existing
O(1) pick), a per-CPU `nr_running`, the per-CPU **idle** task, and a per-CPU
`rq_lock` (a real spinlock). A task lives on **exactly one** CPU's run queue at a
time (`task->cpu`). This is what kills the double-dispatch/torn-context class: no two
CPUs can ever pick the same task, because it's only in one queue.

### Scheduling (hot path, no global lock)
`schedule(rq)` runs with `rq->rq_lock` held: dead-task reap + aging are per-queue,
pick highest-priority from `rq->ready_mask`, `switch_to`, release `rq_lock` from the
resumed side (same proven `sched_resume_unlock` discipline, but per-CPU). The timer
tick preempts the running EL0/ring3 task via its **own** CPU's `rq_lock` — never a
cross-CPU lock — which also removes the async-preemption-on-a-secondary hazard that
was deferred (preemption is now always local).

### Wakeup + migration (the only cross-CPU step)
Waking a task picks a **target CPU** — prefer `task->cpu` (cache affinity); if that
CPU is overloaded and another is idle, pick the idle one — then enqueues on that
CPU's queue under *its* `rq_lock` and, if the target is idle/lower-priority, sends it
a **reschedule IPI** (x86_64 fixed IPI 0xFD already exists; aarch64 GICv2 SGI already
added this session). Migration = dequeue(source rq) → enqueue(target rq), ordered
locks (lower CPU id first) to avoid deadlock. This is the one place two rq_locks meet,
and it's rare (wakeups/balancing), not the hot path.

### Idle — race-free, no spin
Each CPU's idle loop:
```
disable local IRQ
if (rq->ready_mask != 0) { enable IRQ; schedule(rq); continue; }
arm "idle" state                      // visible to wakers
wait-for-interrupt (wfi / hlt / mwait)
enable IRQ                            // take the reschedule IPI / timer
```
The waker sets `ready_mask` (under `rq_lock`) **then** sends the IPI; the idle core
re-checks `ready_mask` with IRQ disabled before sleeping, so a wakeup in the window is
never lost. (The aarch64 attempt that hard-hung did this on the *global* queue with
masked-`wfi`; on a per-CPU queue the check is local and cheap, and we validate the
masked-`wfi` wake on HVF explicitly before relying on it — with the per-CPU timer as a
bounded backstop.)

### Load balancing
Cheap to start: on wakeup, place on the least-loaded of {last CPU, idlest CPU}. Add a
periodic balancer (every N ticks, pull from the busiest queue to an idle one) only if
measurements show imbalance. Idle cores may **work-steal** from a busier queue before
sleeping.

## Accounting & profiling (answering "can we track per-PID user/kernel/IO time?")

**Yes — and it's small.** We already have the foundation: `sched_account_tick()`
charges a tick to `_current` (per-task `run_ticks`) and to per-CPU busy/idle counters
(this drives the Activity Monitor app + procfs today). v2 extends it into a proper
breakdown, because the per-CPU tick already knows everything it needs:

Per **process/thread** (`kTask_t`), charged once per timer tick based on the
**interrupted context** (the trapframe's EL/CPL is already in hand at the tick):
- `utime`  — tick landed in **EL0 / ring 3** (user code).
- `stime`  — tick landed in **EL1 / ring 0** (in a syscall / kernel on its behalf).
- `iowait` (per-task "blocked" time) — wall time spent in WAIT blocked on I/O:
  stamp `block_start = uptime()` when it sleeps on an I/O channel, accumulate on wake.
  (Pure book-keeping at the two transitions — no hot-path cost.)

Per **CPU** (`struct pcpu`), for `/proc/stat` + `top`/Activity Monitor:
- `user`, `sys`, `idle`, `iowait`, `irq` tick counters (same tick, charged by mode;
  `iowait` = idle ticks while any I/O is outstanding on that CPU).

This makes `/proc/<pid>/stat` report real `utime`/`stime`, `/proc/stat` report
per-core user/sys/idle/iowait, and the Activity Monitor show a true user-vs-kernel
split per process and per core — at the cost of a couple of branches in the tick
handler plus two timestamps around I/O sleeps. It is **not** too much to do up front;
it's the natural shape of a modern scheduler's tick and is far easier to build in now
than to retrofit. (Finer-grained — e.g. precise EL0↔EL1 cycle accounting via the
virtual counter at every trap — is a later refinement, gated behind the same flag.)

## Selectability + coexistence

A compile-time switch (e.g. `CONFIG_SCHED_PERCPU`):
- **Off (default):** the current global scheduler, untouched. UP stays exactly as
  today; nothing regresses.
- **On:** the per-CPU scheduler. The MI run-queue ops (`rq_enqueue`/`rq_dequeue`/the
  O(1) pick) are factored so both schedulers share the bucket/`ready_mask` mechanics;
  only the *ownership* (one global vs. per-CPU) and locking differ.

MD seams (already mostly present from this session's work): AP entry, reschedule IPI
(x86_64 0xFD / aarch64 GICv2 SGI), per-CPU timer, `switch_to` + per-task FPU save.
Per-CPU `current` is already done (`%gs` / `TPIDR_EL1`).

## Phasing

1. **Factor** the run-queue mechanics out of `sched_core.c` so a queue is a value
   (`struct runqueue`), not file-scope globals. UP builds identically (one instance).
2. **Per-CPU instances** + per-CPU `rq_lock`; `schedule(rq)`; idle-per-CPU. Validate
   on **x86_64 first** (the anchor; TCG is deterministic and easy to stress), 0 global
   lock in the hot path.
3. **Wakeup/migration + reschedule IPI** + the race-free idle; the multi-CPU stress
   harness (fork/exec storm + the input/desktop interactive paths that broke v1) is
   the gate.
4. **Accounting** (utime/stime/iowait) wired to procfs + Activity Monitor.
5. **aarch64** bring-up of the same, on real-display test. Re-enable
   `AARCH64_SMP_ENABLE_APS` only when the harness + a real desktop session pass.
6. **ubixfs SMP-safety** is a *parallel, independent* workstream (the "not loadable"
   flaky boot) — a scheduler rewrite does not address it; it needs the pool reader
   made re-entrant / fully locked regardless.

## Phase 2 implementation notes (the per-CPU core — concrete steps)

Phase 1 is **done** (`struct runqueue` + a single global `g_rq`; both arches build,
UP boots byte-identically). Phase 2 turns `g_rq` per-CPU. The concrete decisions:

- **Storage.** `struct runqueue g_rq[SCHED_MAX_CPUS]` (MI array, `SCHED_MAX_CPUS`
  defined in `sched_internal.h`, `_Static_assert(SCHED_MAX_CPUS >= MAXCPU)` where
  MAXCPU is visible). Chosen over embedding in `struct pcpu` to avoid the per-arch
  pcpu offset asserts + an include cycle (`pcpu.h` ↔ `sched_internal.h`). Add
  `struct runqueue *this_rq(void)` (= `&g_rq[curcpu()->cpuid]`) and
  `cpu_rq(n)`, implemented in `sched_core.c` behind the existing per-file arch
  pcpu-include dance.
- **Per-task home.** Add `kTask_t.rq_cpu` (which CPU's queue the task is enqueued
  on). `rq_enqueue_locked` sets it; `rq_dequeue_locked` dequeues from
  `g_rq[t->rq_cpu]` (a task can be dequeued by a remote waker, so it must not assume
  `this_rq()`). On UP, `rq_cpu` is always 0 → identical to today.
- **Per-CPU lock.** Add `runqueue.rq_lock`. The hard part is *untangling*
  `schedulerSpinLock`, which today guards the run queue **and** `taskList`/pid-hash
  **and** the lock-across-switch. Split into: (a) per-CPU `rq_lock` for the
  enqueue/dequeue/pick + the switch on that CPU, and (b) a separate lock (or RCU-ish
  scheme) for `taskList`/pid-hash global walks (aging, reap, signal/pgrp delivery).
  This is the riskiest step — do it on x86_64 under TCG with the stress harness, one
  sub-step at a time, UP green at each.
- **schedule(rq).** `sched_common` becomes `schedule(this_rq())`: lock `rq->rq_lock`,
  local aging/reap on that rq, pick from `rq->ready_mask`, `switch_to`, release from
  the resumed side (same `sched_resume_unlock` discipline, now per-rq). Preemption is
  local (the per-CPU timer preempts the local EL0/ring-3 task under `rq_lock`).
- **Wakeup/migration.** `wake(t)` picks target = `t->rq_cpu` (strict affinity) unless
  that CPU is busy and another idle; enqueue under the target's `rq_lock`; reschedule
  IPI if the target is idle/lower-pri. Two rq_locks only meet on migration — order by
  CPU id.
- **Idle.** Per-CPU idle (already `curcpu()->idle`); the race-free
  check-ready_mask-then-wfi/hlt loop, validated for masked-`wfi` wake on HVF with the
  per-CPU timer as the floor.

Order: (2a) array + `this_rq`/`cpu_rq` + `rq_cpu`, enqueue/dequeue/pick routed
through them, **global lock unchanged** (UP-identical; correct-but-serialised SMP).
(2b) split the lock → per-CPU `rq_lock` + a `taskList` lock. (2c) wakeup/migration +
race-free idle. x86_64 validates each before aarch64. This sequencing keeps every
intermediate buildable and UP-green.

## Decisions (approved 2026-06-20)

- **Affinity v0:** strict — a woken task stays on its last CPU unless that CPU is
  busy *and* another is idle. Cache locality + deterministic behavior; revisit if
  measurements show imbalance.
- **Idle backstop:** keep the per-CPU timer as the floor; the reschedule IPI is the
  fast path. Validate masked-`wfi` wake on HVF before relying on it.
- **AP preemption:** enable **local** preemption (the per-CPU timer preempts the
  local EL0/ring-3 task under the local `rq_lock`). Safe with per-CPU queues — a task
  is only ever in one queue, so the double-dispatch/torn-context class can't occur —
  but validate under the stress harness.
- **ubixfs SMP-safety:** sequenced as an **independent** workstream (Phase 6 / can run
  in parallel); the scheduler rewrite does not address the "not loadable" race.
