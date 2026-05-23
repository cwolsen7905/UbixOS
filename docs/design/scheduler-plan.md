# UbixOS Scheduler Improvement Plan

## Status

| # | Task | Phase | Status |
|---|------|-------|--------|
| 1.1 | Fix O(n²) dead-task cleanup — inline splice | 1 | ✅ Done |
| 1.2 | Remove `FORK` state — insert as `READY` | 1 | ✅ Done |
| 1.3 | Fix wrap-around double-scan | 1 | ✅ Done |
| 1.4 | Wire up `need_resched` + quantum decrement | 2 | ⏸ Deferred — needs timer.S to pre-set need_resched |
| 1.5 | Hash table for `schedFindTask` | 1 | ✅ Done |
| 2.1 | Per-priority run queues + `ready_mask` data structure | 2 | ✅ Done |
| 2.2 | O(1) dispatch via `__builtin_clz(ready_mask)` | 2 | ✅ Done |
| 2.3 | Priority bands (32 levels, 4 bands) | 2 | ✅ Done |
| 2.4 | Per-band time quanta + preemption | 2 | ✅ Done |
| 2.5 | Add `priority`, `base_priority`, `quantum`, `rq_next/prev` to `kTask_t` | 2 | ✅ Done |
| 3.1 | QoS classes as `base_priority` floor | 3 | ✅ Done |
| 3.2 | Temporary priority boosts (I/O wakeup, CPU-bound decay) | 3 | ✅ Done |
| 3.3 | Priority inheritance for mutexes | 3 | ⬜ Not started |
| 3.4 | Starvation aging (background timer, +1 per 50 ms) | 3 | ✅ Done |
| 4.1 | Split `kTask_t` → `kProc_t` + `kThread_t` | 4 | ⬜ Not started |
| 4.2 | `clone()` / `rfork()` syscall | 4 | ⬜ Not started |
| 4.3 | Thread-local storage via GS register | 4 | ⬜ Not started |
| 4.4 | libc pthreads wired to `clone()` + futex | 4 | ⬜ Not started |

### Correctness fixes landed 2026-05-23

| Fix | Commit |
|-----|--------|
| `sched_io_wakeup` dequeue/re-enqueue before priority change (run-queue corruption) | 4457e5a |
| `td->td_retval[0]` must be negative errno for EINTR (musl Linux ABI) | 4457e5a |
| `fork`: inherit `sigact`+`sigmask` from parent (POSIX); clear only `sig_pending` | aa4c4ed |
| `exec (sys_exec)`: stop resetting `pgrp` to own PID — POSIX exec preserves pgrp | aa4c4ed |
| `sched_yield()`: no-op for pri≥31 only (was ≥24, broke High-band yields) | aa4c4ed |
| `schedNewTask`: init `last_run_tick` to `sysTicks` so new tasks qualify for aging | aa4c4ed |
| `fork`: inherit `base_priority` (QoS floor) from parent | f99c47f |
| `exec`: clear `boost_quanta` / reset `priority` to `base_priority` on execve | f99c47f |
| `sched_init` bootstrap task: priority set to QOS_REALTIME (was 0 from memset) | f99c47f |
| Non-preempt zone narrowed to pri≥31 (QOS_REALTIME); High band gets 20-tick quantum | 7280805 |
| Add `QOS_KERNEL=24` to `qos_class_t` enum | f99c47f |

### Known deferred items (not bugs, future work)

- `systemTask` is a polling loop — needs blocking `mpi_fetchMessage` before it can safely run at `QOS_KERNEL`. Currently at `QOS_DEFAULT=12`.
- `mpi_fetchMessage`: no blocking/sleep path — caller must poll with `sched_yield()`.
- `execThread` kernel threads stay at QOS_DEFAULT until the above is resolved.

**Legend:** ⬜ Not started · 🔄 In progress · ✅ Done · ⏸ Blocked

---

## Overview

The current scheduler is a simple round-robin over a flat doubly-linked task list.
Every timer tick it scans from `_current->next` to find the next `READY` task — O(n)
per tick with several correctness rough edges. This plan brings it to a modern
priority-based design drawing from Windows NT, macOS/XNU, and FreeBSD ULE.

---

## Current State

**Algorithm:** flat doubly-linked list, O(n) round-robin scan every timer tick.

**Known issues:**

| Issue | Root Cause |
|-------|-----------|
| O(n²) dead-task cleanup | `sched_deleteTask` searches the list again even though the pointer is already in hand |
| `FORK` state is unnecessary | Tasks inserted as `FORK` then transitioned to `READY` on the first scan; one wasted full pass per new task |
| Wrap-around double-scan | `goto schedStart` re-visits already-checked nodes before the wrap point (~2× work per busy pass) |
| `need_resched` is dead code | Set by `wake_up()` but never checked by `sched()`; freshly unblocked tasks don't preempt sooner |
| `schedFindTask` is O(n) | Called from procfs, signals, `waitpid` — linear search every time |

**Files:**
- `sys/kernel/sched_core.c` — task list management, `schedFindTask`, `wake_up`
- `sys/arch/i386/sched_switch.c` — `sched()` dispatch loop and TSS switch
- `sys/include/ubixos/sched.h` — `kTask_t`, `tState`, externals
- `sys/include/ubixos/sched_internal.h` — shared between the two .c files

---

## Phase 1 — Easy Wins

_No design change. Fix correctness issues and add O(1) PID lookup._

### 1.1 Fix O(n²) dead-task cleanup

**File:** `sys/arch/i386/sched_switch.c` — `sched()` DEAD branch (~line 83)

Currently calls `sched_deleteTask(delTask->id)` which walks the list a second time.
We already hold `delTask`; splice it directly:

```c
/* Before (O(n) search inside O(n) scan): */
sched_deleteTask(delTask->id);

/* After (O(1) direct splice): */
if (delTask->prev) delTask->prev->next = delTask->next;
else               taskList            = delTask->next;
if (delTask->next) delTask->next->prev = delTask->prev;
```

### 1.2 Remove `FORK` state

**Files:** `sched_switch.c`, `sched_core.c`, `sched.h`, fork/exec paths

`FORK` exists so callers can insert a task before it is fully initialized, then the
scheduler transitions it to `READY` on the first scan pass. The cleaner approach:
don't add the task to the run list until initialization is complete, then insert as
`READY`. Remove the `FORK` case from `sched()` and the `tState` enum.

### 1.3 Fix wrap-around double-scan

**File:** `sched_switch.c` — bottom of `sched()`

```c
/* Current — visits tasks before wrap-point a second time: */
if (tmpTask == NULL) { tmpTask = taskList; goto schedStart; }

/* Fix — track whether we have already wrapped: */
int wrapped = 0;
tmpTask = _current ? _current->next : taskList;
if (tmpTask == NULL && !wrapped) { wrapped = 1; tmpTask = taskList; }
/* stop if we have scanned a full circle */
if (tmpTask == _current) { /* no runnable task — idle */ }
```

### 1.4 Wire up `need_resched`

**File:** `sched_switch.c` — entry of `sched()`

```c
/* Check at the top so a woken high-priority task can preempt immediately: */
if (!need_resched) return;
need_resched = 0;
```

Also decrement `_current->counter` on each tick and set `need_resched = 1` when it
reaches zero (currently `counter` is never decremented — quantum is effectively
infinite).

### 1.5 Hash table for `schedFindTask`

**File:** `sched_core.c`

256-bucket chained hash table keyed on `pid & 0xFF`. The existing `taskList` linked
list is kept intact for iteration (procfs, signals, `waitpid`). The hash table is a
parallel index used only for point lookups.

```c
#define SCHED_HASH_BUCKETS 256
static kTask_t *pid_hash[SCHED_HASH_BUCKETS];   /* hash chain via task->hash_next */

/* Insert (called from schedNewTask): */
static inline void pid_hash_insert(kTask_t *t) {
    int b = t->id & (SCHED_HASH_BUCKETS - 1);
    t->hash_next = pid_hash[b];
    pid_hash[b]  = t;
}

/* Lookup (replaces O(n) loop): */
kTask_t *schedFindTask(uint32_t id) {
    kTask_t *t = pid_hash[id & (SCHED_HASH_BUCKETS - 1)];
    for (; t; t = t->hash_next)
        if (t->id == id) return t;
    return NULL;
}
```

Add `kTask_t *hash_next` to `kTask_t` in `sched.h`.

---

## Phase 2 — Priority Run Queue

_Replace the O(n) scan with O(1) dispatch. Architectural change to `sched_switch.c`
and `sched_core.c`; `kTask_t` gains a `priority` field._

### 2.1 Data structure

32 per-priority doubly-linked run queues plus a bitmask (Windows ReadySummary trick):

```c
#define SCHED_PRIORITIES 32

static kTask_t *run_queue[SCHED_PRIORITIES]; /* head of per-priority list */
static uint32_t ready_mask;                  /* bit N set ↔ run_queue[N] non-empty */
```

`taskList` is kept as the "all live tasks" list for procfs/signals. The run queues
hold only `READY`/`RUNNING` tasks.

### 2.2 O(1) dispatch (Windows BSR pattern)

```c
/* Highest set bit = highest priority with a ready task: */
int pri  = 31 - __builtin_clz(ready_mask);
_current = run_queue[pri];
/* pop head: */
run_queue[pri] = _current->rq_next;
if (run_queue[pri] == NULL)
    ready_mask &= ~(1u << pri);
```

### 2.3 Priority bands

| Level | Band | Default users |
|-------|------|---------------|
| 0 | Idle | idle thread (runs only when nothing else is ready) |
| 1–7 | Background | automountd, batch jobs, indexing |
| 8–15 | Normal (dynamic) | default for new user processes |
| 16–23 | Interactive (dynamic) | shell, login, foreground apps |
| 24–30 | High | kernel threads, device drivers |
| 31 | Realtime | hard-deadline work, never adjusted by OS |

Levels 1–23 are **dynamic** — the OS adjusts them temporarily.
Levels 24–31 are **fixed** — the OS never adjusts them.

New processes default to priority **12** (mid Normal band).

### 2.4 Time quanta

Each task has a `quantum` counter decremented on every timer tick. When it hits zero
the task is preempted and re-enqueued at the tail of its priority queue (round-robin
within a band).

| Band | Ticks |
|------|-------|
| Background (1–7) | 2 |
| Normal (8–15) | 6 |
| Interactive (16–23) | 10 |
| High / Realtime (24–31) | unlimited — runs until it blocks |

`kTask_t` gains:
```c
uint8_t  priority;       /* current (possibly boosted) priority 0–31 */
uint8_t  base_priority;  /* QoS floor — boosts never go below this */
uint8_t  quantum;        /* ticks remaining in current time slice */
```

### 2.5 `kTask_t` changes

```c
/* New fields in kTask_t (sched.h): */
uint8_t  priority;
uint8_t  base_priority;
uint8_t  quantum;
kTask_t *rq_next;   /* next in per-priority run queue (separate from taskList) */
kTask_t *rq_prev;
kTask_t *hash_next; /* Phase 1 hash chain */
```

---

## Phase 3 — Dynamic Priority (macOS QoS + Windows boosts)

_Additive on top of Phase 2. No structural changes._

### 3.1 QoS classes

Stored as `base_priority` — the OS never drops a task below its QoS floor.
Inspired by macOS QoS API (`DISPATCH_QOS_CLASS_*`).

```c
typedef enum {
    QOS_BACKGROUND       =  4,  /* maintenance work, automountd */
    QOS_UTILITY          =  8,  /* compilation, long-running tools */
    QOS_DEFAULT          = 12,  /* unset — inherited from parent */
    QOS_USER_INITIATED   = 18,  /* action the user explicitly started */
    QOS_USER_INTERACTIVE = 22,  /* direct UI interaction */
    QOS_REALTIME         = 31,  /* hard-deadline (fixed, never decayed) */
} qos_class_t;
```

New processes inherit `QOS_DEFAULT` unless the parent sets otherwise.
A future `sys_qos_set()` syscall (or `setpriority()` wrapper) lets processes
lower their own QoS; only privileged processes may raise it.

### 3.2 Temporary priority boosts

All boosts are temporary — they decay back to `base_priority` after 1–3 quanta.
The scheduler restores `base_priority` when the boost expires.

| Trigger | Boost | Duration | Rationale |
|---------|-------|----------|-----------|
| Woke from blocking I/O | +4 | 2 quanta | Reward I/O-bound tasks; they yield willingly (FreeBSD ULE) |
| Woke from sleep < 5 ms | +6 | 1 quantum | Interactive burst — user is waiting (macOS timeshare) |
| Completed full quantum with no block | −2 | permanent until next I/O | CPU-bound penalty — decays toward background |
| Starvation (no run for > 200 ms) | +1 per 50 ms | until scheduled | FreeBSD aging — prevents starvation |
| Foreground process group (tcsetpgrp) | +3 | while fg | Windows foreground boost |
| Woke from mutex held by lower-priority task | inherit holder's priority | until lock released | Priority inheritance — prevents inversion |

### 3.3 Priority inheritance for mutexes

When task A (high priority) blocks on a mutex held by task B (low priority):
temporarily raise B to A's priority until B releases the mutex.
Required for correctness in any real-time or interactive workload.

Implementation: `struct kmutex` gains a `owner` pointer; lock/unlock paths check
whether the new waiter's priority exceeds the owner's and boost accordingly.

### 3.4 Starvation aging

A background timer (every 50 ms) scans tasks that have not been scheduled in the
last 200 ms and applies a +1 boost. Cap: `min(base_priority + 8, 23)` — aging
cannot promote a background task into the interactive band.

---

## Phase 4 — Kernel Threads + POSIX Threads Foundation

_Largest change. Deferred until Phase 3 is stable and tested._

### 4.1 Separate process from thread

Current `kTask_t` conflates "process" (address space, fd table, signals) with
"execution context" (stack, registers, priority). Split into:

```c
typedef struct kProc {
    pidType          pid;
    kTask_t         *threads;       /* list of threads belonging to this process */
    struct vmspace  *vm;            /* address space */
    struct fdtable  *fds;           /* file descriptor table */
    sigset_t         sigmask;
    /* ... uid, gid, cwd, etc. */
} kProc_t;

typedef struct kThread {
    tidType          tid;
    kProc_t         *proc;          /* owning process */
    uint8_t          priority;
    uint8_t          base_priority;
    uint8_t          quantum;
    struct md_thread md;            /* arch registers/stack */
    /* ... per-thread errno, TLS pointer, signal mask */
} kThread_t;
```

The scheduler operates on `kThread_t`. `kProc_t` is the unit for `fork()`/`waitpid()`.

### 4.2 `clone()` / `rfork()` syscall

`clone()` (Linux ABI, slot 120) or `rfork()` (FreeBSD ABI) creates a new thread
sharing the calling process's address space and fd table. The new thread gets its
own stack (caller-supplied) and starts at a specified entry point.

This is the foundation for `pthread_create()` in libc.

### 4.3 Thread-local storage (TLS)

Each thread gets a TLS block. On i386 the GS segment register points to it.
`sys_set_thread_area()` (FreeBSD: `sysarch(I386_SET_GSBASE)`) lets libc install
the TLS pointer at thread creation. `__thread` variables in C are resolved via
GS-relative addressing by the compiler.

### 4.4 libc pthreads

With `clone()` and TLS in place, musl's pthread implementation works with minimal
adaptation. Key pieces:
- `pthread_create` → `clone()`
- `pthread_mutex_*` → futex or kernel semaphore
- `pthread_cond_*` → futex wait/wake

---

## Implementation Order

```
Phase 1  ──►  Phase 2  ──►  Phase 3  ──►  Phase 4
 1–2 days     3–4 days      3–4 days       1–2 weeks

Easy wins    Run queue    QoS + boosts    Threads
No redesign  O(1) dispatch  Dynamic prio  Struct split
```

Each phase is independently testable. Phase 2 is the architectural inflection
point — once the priority run queue is in, Phases 3 and 4 are additive.

---

## Files Affected

| File | Phases |
|------|--------|
| `sys/include/ubixos/sched.h` | 1–4 |
| `sys/include/ubixos/sched_internal.h` | 1–2 |
| `sys/kernel/sched_core.c` | 1–4 |
| `sys/arch/i386/sched_switch.c` | 1–3 |
| `sys/kernel/mutex.c` _(new)_ | 3 |
| `sys/include/ubixos/mutex.h` _(new)_ | 3 |
| `sys/arch/i386/fork.c` | 1, 4 |
| `sys/arch/i386/i386_exec.c` | 2, 4 |
| `sys/kernel/syscalls_posix.c` | 4 (`clone`) |
| `sys/kernel/vfs_calls.c` | 4 (TLS syscall) |
