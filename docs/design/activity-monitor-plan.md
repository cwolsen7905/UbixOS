# UbixOS Activity Monitor Plan

## Goal

A graphical, macOS Activity Monitor-style application that shows the live
state of every process on the system: PID, name, parent, state, CPU%,
resident memory, and elapsed time — sortable, refreshing once per second.
A v1 that just renders the table is the milestone; live CPU graphs and a
"force quit" button are stretch goals.

The work splits cleanly into **three layers**, each independently testable:

1. **Kernel accounting** — collect the numbers (per-task tick counts,
   per-CPU busy/idle, RSS).
2. **procfs surface** — expose them as text files under `/proc`.
3. **`views` app** — read `/proc`, render the table with `objGFX`.

Layer 1 is shared with the SMP work; see `smp-plan.md` Phase 3.5.

---

## Status Matrix

Legend: ✅ done · 🟡 partial · ⬜ not started

| Phase | Item | Status |
|-------|------|--------|
| 1 | `kTask_t.run_ticks` + scheduler-tick hook | ⬜ |
| 1 | `struct pcpu.busy_ticks` / `idle_ticks` | ⬜ |
| 1 | `g_jiffies` + `HZ` exported to userland | ⬜ |
| 2 | `/proc/<pid>/stat` includes `run_ticks` | ⬜ |
| 2 | `/proc/<pid>/statm` (RSS / VSZ in pages) | ⬜ |
| 2 | `/proc/stat` (per-CPU + aggregate busy/idle) | ⬜ |
| 2 | `/proc/uptime` (seconds since boot, idle seconds) | ⬜ |
| 3 | `bin/activity/` views app — static table v1 | ⬜ |
| 3 | Sortable columns, sticky header, 1 Hz refresh | ⬜ |
| 3 | Per-CPU usage strip across the top | ⬜ |
| 4 | "Force quit" via `kill(2)` | ⬜ |
| 4 | CPU-history sparkline per row | ⬜ |

---

## Layer 1 — Kernel accounting

This layer is **the same work** as `smp-plan.md` Phase 3.5; do it once,
both plans benefit.  Summary here for context:

- `kTask_t` gains `u_int64_t run_ticks` (optionally `user_ticks` +
  `sys_ticks` if we want a `%user` / `%sys` split).
- `struct pcpu` gains `u_int64_t busy_ticks, idle_ticks`.
- The PIT/LAPIC tick handler does, *before* `schedule()`:

  ```c
  struct pcpu *pc = curcpu();
  if (pc->current == pc->idle) pc->idle_ticks++;
  else                          pc->busy_ticks++;
  pc->current->run_ticks++;
  ```

  (On uniprocessor `curcpu()` is `&g_pcpu[0]`, so this is identical to a
  global counter — no behavior change until SMP scheduling actually runs.)

- `g_jiffies` (the existing PIT counter) and a compile-time `HZ` get a
  one-line export to userland, either as `sysctl(kern.clockrate)` or as
  the first line of `/proc/uptime`.

**RSS** is independent of the tick work and adds a small VMM helper:

```c
/* Count present user pages in this task's PD (PD indices 1..767 — skip
 * the identity-mapped first 4 MB and the kernel range 770..1023). */
u_int32_t vmm_count_resident_pages(kTask_t *t);
```

One walk per `/proc/<pid>/statm` read.  Cheap (~768 PDE checks; only
present PDEs descend into 1024 PTE checks).  No locking needed beyond
holding the task reference — the caller is already iterating the task
list under its lock.

---

## Layer 2 — procfs surface

Two existing files extended, two new files added.  All Linux-ish on
purpose so common tooling (`top`, `ps`, ports of `htop`) "just works".

### `/proc/<pid>/stat` (extend)

The current single-line `stat` gains the standard tick fields at their
Linux positions: `utime stime cutime cstime` (in ticks).  v1 fills
`utime = run_ticks`, the others zero, until we split user/sys.

### `/proc/<pid>/statm` (new)

One line, space-separated, in pages:

```
size resident shared text lib data dt
```

v1 fills `size` (total mapped) and `resident` from
`vmm_count_resident_pages()`; the rest are zero.  This is the file an
activity monitor actually reads — `stat`'s memory fields are notoriously
fiddly.

### `/proc/stat` (new)

```
cpu  <busy> <idle>          # aggregate across all CPUs
cpu0 <busy> <idle>
cpu1 <busy> <idle>
...
ctxt <context-switch count>
btime <unix epoch of boot>  # 0 until we have an RTC
processes <total forks since boot>
```

The activity monitor reads this twice (T and T+1s), subtracts, and
divides — that's the CPU% column.

### `/proc/uptime` (new)

```
<seconds since boot> <sum of all cpus' idle seconds>
```

One line, two floats.  Trivial to compute from `g_jiffies` and the
per-CPU `idle_ticks`.

---

## Layer 3 — The `views` app (`bin/activity/`)

A standard `views`-hosted app, structured the same way as `bin/term/`
post-SOLID refactor.  No new compositor protocol — pure
read-`/proc` + draw-with-`objGFX`.

### v1 (the milestone)

- Single window.  Sticky header row with column names.
- Columns: **PID · Name · PPID · State · CPU% · RSS · Time**.
- Refresh every 1000 ms via the existing views timer/select loop.
- CPU% is computed in userland:
  `100.0 * (run_ticks_now - run_ticks_prev) / (jiffies_now - jiffies_prev)`.
  No kernel-side averaging needed.
- RSS pretty-printed as KiB/MiB.  `Time` is `run_ticks / HZ` as `MM:SS`.
- Sort by clicking a header (default: CPU% descending).
- No scrollbar yet — list trimmed to fit the window.  Resize is already
  wired via `DISPLAY_WINRESIZE` so it scales naturally.

### v2 (polish)

- Per-CPU usage strip across the top: one mini-bar per `cpuN` in
  `/proc/stat`, height = % busy.  This is the only place the SMP work is
  *visible* to the user, and it's a great smoke test for Phase 4.
- A per-row CPU sparkline (last 60 samples in a ring buffer in the app).
- A "Force quit" button → `kill(SIGTERM)` then `kill(SIGKILL)` 3 s later
  (works the moment `kill(2)` is wired; signals plan is already in flight).
- Filter box ("show only matching name").

### Not in scope

- Per-thread view (UbixOS is process-per-task at the kernel level today).
- Disk/Network/Energy tabs (no kernel counters for these yet).
- Open-files inspector (we have `/proc/<pid>/fd/`, but it's a separate
  app; activity monitor stays focused).

---

## Sequencing

1. **Layer 1 fields** — additive struct fields + two `++` in the tick
   handler.  Should be one commit, no behavior change.
2. **`vmm_count_resident_pages()`** — one helper, no callers yet.
3. **procfs extensions** — file-at-a-time; each one independently
   testable from the shell (`cat /proc/stat`).
4. **`bin/activity/` v1** — the whole table, refresh, sort.  Ship.
5. **v2 polish** lands incrementally as the underlying features
   (`kill`, multi-CPU, etc.) come online.

A determined v1 is **roughly 2–3 days** of work: half a day kernel, half
a day procfs, one to two days on the `views` app.  Layer 1 is already
on the books as `smp-plan.md` Phase 3.5, so for the activity-monitor
budget it's free.

---

## Interaction with other plans

- **`smp-plan.md` Phase 3.5** owns the accounting fields.  This plan
  consumes them.  Do Phase 3.5 first (it's tiny) or interleave — the
  activity-monitor v1 can ship with `busy_ticks` always landing on
  `cpu0` while `-smp 1` is the only tested target.
- **`completed/signal-plan.md` Phase 3** delivers `kill(2)` (done), which
  unblocks "Force quit".
- **`solid-refactor`** — model `bin/activity/` after the post-refactor
  `bin/term/` structure (separate view, model, and event-loop classes).
  Don't repeat the monolithic-`main.cc` mistake.
