# UbixOS Kernel Threads & POSIX Threads Plan

> Carved out of `scheduler-plan.md` (2026-06-03). The scheduler itself
> (priority run queue, QoS, boosts, PI, aging) is **complete** — see
> `completed/scheduler-plan.md`. This plan is the separate, larger initiative
> that builds on it: a real process/thread split and POSIX threads.

## Goal

Give UbixOS genuine kernel threads — multiple execution contexts sharing one
address space — and wire musl's `pthread_*` to them. Today `kTask_t` conflates
"process" (address space, fd table, signals) with "execution context" (stack,
registers, priority); this plan separates them and adds `clone()`, TLS, and
futexes.

## Status

| # | Task | Status |
|---|------|--------|
| 1 | Split `kTask_t` → `kProc_t` + `kThread_t` | ⬜ Not started |
| 2 | `clone()` / `rfork()` syscall | ⬜ Not started |
| 3 | Thread-local storage (TLS) | ⬜ Not started |
| 4 | libc pthreads on `clone()` + futex | ⬜ Not started |

**Legend:** ⬜ Not started · 🔄 In progress · ✅ Done

## Dependencies / relationship to other work

- **Built on the completed scheduler** (`completed/scheduler-plan.md`): the
  scheduler already operates on a per-priority run queue with QoS/boosts/PI/aging.
  It currently schedules `kTask_t`; after step 1 it schedules `kThread_t`.
- **Not an arm64 bring-up prerequisite.** arm64 boots, runs the scheduler, and
  runs single-threaded processes without any of this. Threads are an app-facing
  capability to add once the port is up — relevant to the mobile direction
  (apps use threads) but not blocking it.
- **Independent of SMP.** Threads (many contexts per process) and SMP (many CPUs
  running contexts) compose but neither requires the other. Both refactor
  `kTask_t`, so whichever lands first should keep the struct changes clean for
  the other; see `smp-plan.md`.

## 1. Separate process from thread

Split `kTask_t` into:

```c
typedef struct kProc {
    pidType          pid;
    kTask_t         *threads;       /* threads belonging to this process */
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

The scheduler operates on `kThread_t`. `kProc_t` is the unit for `fork()` /
`waitpid()`.

## 2. `clone()` / `rfork()` syscall

`clone()` (Linux ABI, slot 120) or `rfork()` (FreeBSD ABI) creates a new thread
sharing the caller's address space and fd table, with a caller-supplied stack
and entry point. Foundation for `pthread_create()`.

## 3. Thread-local storage (TLS) — **architecture-specific**

Each thread gets a TLS block.

- **i386**: the GS segment register points to it; `sys_set_thread_area()`
  (FreeBSD `sysarch(I386_SET_GSBASE)`) installs the pointer; `__thread` resolves
  via GS-relative addressing. Note: this is the same `%gs` mechanism the kernel
  uses for per-CPU `_current` — userland TLS uses a *different* GS base, set per
  thread, so the kernel's per-CPU `%gs` and userland's TLS `%gs` must be saved/
  restored across the user/kernel boundary (already handled by the entry stubs).
- **aarch64** (when ported): TLS is `TPIDR_EL0`, a dedicated thread-pointer
  system register — a completely different mechanism. This step therefore needs
  an arch abstraction (`<machine/tls.h>`); the i386 `sys_set_thread_area`
  implementation should move to `sys/arch/i386/` per `cross-arch-plan.md`.

## 4. libc pthreads

With `clone()` and TLS in place, musl's pthread implementation works with
minimal adaptation:
- `pthread_create` → `clone()`
- `pthread_mutex_*` → futex or kernel semaphore
- `pthread_cond_*` → futex wait/wake
