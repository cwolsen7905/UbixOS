# SMP Hardening Plan (64-bit cooperative SMP)

**Status:** scoping / design (2026-06-20). No code yet.
**Goal:** make cooperative parallel SMP safe for daily use on aarch64 (the daily
driver) and x86_64 (the anchor) — both CPUs running user tasks off the shared run
queue, no preemption on the secondary (the proven-stable model; AP *preemption*
stays deferred, see [[project_smp_64bit_activation]]).

## Why this plan exists

Enabling cooperative SMP on aarch64 boots the full graphical desktop with both
CPUs scheduling in parallel, but real-display testing exposed two reproducible
failures a headless fork/exec stress did **not**:

1. **First login fails, second succeeds.** Auth reads `/etc/userdb` off disk; a
   concurrent two-CPU disk read returns stale/garbage data once.
2. **Mouse dead after login.** Input/IPC wakeups to a task on the secondary CPU
   stall — the AP only notices new work on its 10 ms timer poll.

Both trace to specific, fixable code. The headless test missed them because it
never exercised concurrent *disk reads* or *interactive input* — only fork/exec.

**Lesson driving this plan:** SMP activation is a hardening project, not a flag
flip. Headless stress is necessary but not sufficient; the interactive paths (FS
auth, input, IPC wakeup) are where uniprocessor assumptions bite.

---

## Phase 0 — the two root causes (unblocks the daily driver)

### 0A. Make the virtio-blk driver SMP-safe  *(fixes "first login fails")*

`sys/arch/aarch64/dev/virtio_blk.c` uses a **single set of shared static buffers**
for every request — `g_hdr`, `g_data` (one 512 B bounce buffer), `g_status`, and
the virtqueue rings `g_desc`/`g_avail`/`g_used`/`g_last_used` — with **no lock**
across the submit→poll-for-completion window. Two CPUs reading concurrently clobber
each other's header/bounce-buffer and race the avail/used rings, so a read returns
another sector's data (or garbage). That is exactly the failed-first-userdb-read.

- Add a driver-internal spinlock held across the entire `read()`/`write()`
  critical section (request setup → ring submit → poll completion → copy out).
  Driver-internal (not caller-side) so it protects **every** path, including the
  `fat_file_read()` fast path (`sys/fs/fat/fat_file.c:319`) that calls
  `dev_blk_ops->read()` directly, bypassing `bcache`.
- The poll-for-completion busy-loop must **yield** while holding the lock would be
  wrong on a non-preemptive kernel — instead keep the section short (the device
  completes synchronously in QEMU) or use `spinLockIrq`/short spin. Decide: spin
  vs. block. Given the driver polls (no completion IRQ wired), a plain spinlock
  around the short synchronous request is correct.
- **Do the same on x86_64** (`sys/arch/x86_64/dev/virtio_blk.c`). Its cooperative
  SMP is "verified" only against fork/exec churn, which doesn't read files
  concurrently — the same single-buffer race is latent there.
- Longer term: a small ring of in-flight request slots instead of one, but a lock
  is the correct, minimal first fix.

### 0B. aarch64 reschedule IPI via GICv2 SGI  *(fixes "dead mouse")*

`arch_smp_reschedule()` is the weak no-op (`sys/kern/sched_core.c`) on aarch64;
x86_64 has a strong override (`sys/arch/x86_64/kern/smp.c`) that IPIs its APs.
Without it the AP only polls the run queue on its 100 Hz CNTV tick, so a wakeup
(`sched_wakeup_chan`/`sched_io_wakeup` → `rq_enqueue_locked` → `arch_smp_reschedule`)
to a task parked on the AP waits up to 10 ms — input and MPI delivery stall.

Mirror the x86_64 mechanism with a GICv2 Software-Generated Interrupt:
- Add `GICD_SGIR` (distributor offset 0x0F0) to `sys/arch/aarch64/dev/gic.c` and a
  `aarch64_gic_send_sgi(target, sgi)` helper. Pick a free SGI INTID (0–15; use 0).
- `gic_secondary_init()` must **enable** the reschedule SGI on each AP (GICD_ISENABLER0
  for INTID 0) so the AP receives it.
- Route the SGI in `aarch64_irq_dispatch()`: EOI and return — the *wake* is the
  point (the AP's idle loop re-runs `sched_yield()` after the IRQ). Do **not**
  `sched()` from the SGI handler on the AP (keep the AP cooperative / no async
  preemption — that is the deferred-x86_64 corruption path).
- Add `sys/arch/aarch64/kern/smp.c` with the strong `arch_smp_reschedule()` that
  SGIs every online AP except self (gate on `g_pcpu[c].heartbeat`).
- aarch64 TLBI is hardware-broadcast, so still **no** TLB-shootdown IPI needed.

---

## Phase 1 — remaining CRITICAL unlocked globals (correctness under load)

### 1C. `signal_post_tty` / `signal_post_pgrp` walk `taskList` with no lock
`sys/posix/signal.c` (~178–241) iterates the global `taskList` and mutates
`sig_pending`/`sig_code[]` with **no** `schedulerSpinLock`. Called from
`tty_inject` on Ctrl-C/Ctrl-Z/SIGWINCH. Concurrent `sched()` task-list mutation on
another CPU → stale `->next` (use-after-free) or missed task. Fix: hold
`schedulerSpinLock` (or a dedicated signal lock with consistent ordering) around the
walk. Watch lock ordering vs. the tty lock.

### 1D. TTY stdin ring buffer relies on per-CPU IRQ disable
`sys/posix/tty.c` protects `stdin[]`/`stdinSize` with `irq_save_disable()`, which is
**per-CPU** — it does not serialize two CPUs. `tty_read` (shift) racing `tty_inject`
(append) corrupts the ring / size. Fix: a real tty spinlock (`spinLockIrq` to keep
ISR-safety) around stdin ring mutation; audit the other tty ring buffers too.

### 1E. `devfs_len` updated without the devfs lock
`sys/fs/devfs/devfs.c`: `devfs_makeNode` does `devfs_len += ...` unlocked while
`devfs_open` reads it under `devfsSpinLock`. Mostly boot-time, but make
`makeNode`'s counter + list mutation take `devfsSpinLock`. Low blast radius, cheap.

---

## Phase 2 — pmap / VMM concurrency (needed once apps use threads on SMP)

Single-threaded processes have a per-process page table touched by one CPU at a
time (safe). The exposure is (a) the **shared kernel L1** and (b) **threads** of one
process running on two CPUs.

### 2F. Lock kernel-L1 fine mapping + fork-vs-COW
`sys/arch/aarch64/vmm/pmap.c`: `table_next()` allocates+links L2/L3 tables with no
lock (TOCTOU: two CPUs faulting the same kernel-L1 slot both allocate, one L2 is
lost). `pmap_fork_copy()` reads the parent's tables while the parent may
`pmap_cow_fault()` on another CPU (use-after-free of an L2/L3 the parent frees).
Fix: a pmap spinlock for shared-kernel-L1 mutation; for fork, serialize against the
parent's COW faults (per-process pmap lock held across `fork_copy` and `cow_fault`).
Until then, **document that multi-threaded processes on SMP are unsupported** and
keep the desktop (single-threaded apps) working. Verify the same on x86_64 pmap.

---

## Phase 3 — lower-severity hardening + validation

- **3G. Callouts:** `callout_run_expired` invokes callbacks while holding
  `schedulerSpinLock` (`sys/kern/callout.c` + `sched_dispatch.c`). Works on UP;
  on SMP it blocks the other CPU for the callback's duration and would deadlock if
  a callback re-enters the scheduler lock. Verify callbacks never re-enter; if any
  do, drop the lock around the callback. (Likely fine today — verify, don't assume.)
- **3H. CSPRNG `krandom_stir`:** lock-free RMW on `g_key`/`g_ctr`/`g_stir_idx`,
  called every tick from both CPUs (`sys/kern/random.c`). Per its own comment this
  is entropy-only (a dropped sample is harmless), but a concurrent stir during
  `krandom_bytes` extraction is worth a lightweight guard or a documented
  acceptance. Low priority.
- **3I. Real-display SMP validation harness.** The gap that let these ship: no
  automated test drove the *interactive* desktop. Build a QMP+VNC harness (HVF,
  `-smp 2`) that: boots, screendumps the login, injects `root`/`user` keystrokes,
  screendumps the composited desktop, injects mouse moves/clicks (needs an
  **absolute** pointer — add `virtio-tablet-device` to the run target, since
  `virtio-mouse` is relative and QMP `input-send-event` abs coords don't map),
  launches apps, and asserts no fault over a soak. Add a concurrent-FS-read stress
  (many processes reading distinct files) to exercise 0A. This becomes the gate for
  re-enabling `AARCH64_SMP_ENABLE_APS`.

---

## Sequencing & re-enable gate

1. Phase 0 (0A + 0B) — then re-test the real desktop: login on first try + working
   mouse are the pass criteria.
2. Phase 1 (1C–1E) — re-run the desktop + Ctrl-C/resize/tty stress.
3. Phase 3I harness in parallel (it gates everything).
4. Phase 2 only when an SMP multi-threaded workload is in scope; until then document
   the single-threaded-only constraint.

`AARCH64_SMP_ENABLE_APS` stays **0** (and x86_64 SMP stays as-is) until Phases 0–1
pass the real-display harness. Each phase keeps **both** arches green.

## Notes on confidence

0A and 0B are high-confidence: they map directly to the two observed symptoms and to
specific shared-state-without-lock in named files. 1C/1D/1E are real unlocked
globals but lower observed-frequency. 2F matters for threads/kernel-dynamic-map; the
single-threaded desktop is safe without it. 3G/3H need verification before action —
treat as "confirm, then fix if needed," not assumed bugs.
