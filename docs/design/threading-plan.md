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

## Approach: pragmatic v1, then clean up

The original plan led with a full `kTask_t` → `kProc_t` + `kThread_t` struct
split (a big-bang refactor touching the scheduler, fork, exec, and signals).
We are deliberately **deferring that split** and shipping working pthreads on a
shared-address-space `kTask_t` first, then doing the clean refactor afterward.
Decision (2026-06-06): get threads working in days, not a weeks-long refactor.

## Status — v1 (pragmatic: shared-AS `kTask_t`)

| # | Task | Status |
|---|------|--------|
| A | `clone()` syscall — new `kTask_t` sharing parent `cr3` + sigacts; caller stack/entry | ✅ **Done & verified** (`bin/clonetest` PASS: thread ran in the shared AS, wrote our globals, exited, main reaped it) — **A1:** `sys_rfork` at FreeBSD slot 251 (`sys/arch/i386/fork.c`) shares `cr3`, runs on the caller's stack at the same eip with eax=0, shallow-shares fds, joins caller `tgid`; `tgid` field on `kTask_t` (defaults to own id). Kernel compiles. **A2:** `contrib/musl/src/thread/i386/clone.s` rewritten to invoke slot 251 with the uBixOS stack-arg ABI (was stock Linux `eax=120`); stashes fn/arg on the child stack, child calls `fn(arg)` then `SYS_exit`. v1 copies `vm_map` (shared vm_map = follow-up). **End-to-end test:** `bin/clonetest` (raw `spawn_thread` wrapper, no TLS) spawns a thread that writes shared globals. First run SIGSEGV'd — but in the *parent* on SIGCHLD delivery, not the thread: the thread ran and exited cleanly. Root cause was a **latent POSIX exec bug**, not threads — `sys_exec` never reset signal dispositions, so a process kept its parent's caught-signal handler *addresses* across `execve` (clonetest inherited tcsh's SIGCHLD handler, wild in the new AS). Fixed: `sys_exec` (`i386_exec.c`) now resets every caught signal to `SIG_DFL` on exec (SIG_IGN preserved; mask/pending untouched), per POSIX. clonetest is just the first non-shell program to both inherit a caught SIGCHLD and reap a child. **Re-test after kernel rebuild.** |
| B | Address-space refcount — tear down the shared `cr3` only when the *last* task of the `tgid` exits | ✅ **Done & verified** (clonetest: the thread's exit did not tear down main's AS) — `sched_tgid_others_alive()` (`sched_core.c`) counts live siblings; `endTask` skips `vmm_clean_virtual_space` and sets `reap_free_as=0` for a non-last thread; the reaper (`systemtask.c`) skips `vmm_free_process_pages` unless `reap_free_as`. `reap_free_as` field on `kTask_t` (defaults 1). Stayed out of `vmm_memory.c`. **Known v1 leak:** PT/PD pages allocated by *non-last* tgid members aren't reclaimed (page bitmap is per-pid) — fix is tgid-based page ownership; tracked under F. Shallow-shared `td.o_files` aren't freed by the reaper (it frees the legacy `files[0]`), so no double-free; proper shared close semantics tracked under G. |
| C | TLS — `sys_set_thread_area` (per-thread userland `%gs` base, saved/restored at the user↔kernel boundary) | ⬜ Not started |
| D | `futex` syscall — wait/wake on a user address, on the existing `wait_chan` sleep/wakeup | ⬜ Not started |
| E | musl pthreads wiring — `pthread_create`→`clone`, `mutex`/`cond`→`futex`, `set_thread_area` | ⬜ Not started |

## Status — deferred cleanup (after threads work end-to-end)

| # | Task | Status | Why deferred |
|---|------|--------|--------------|
| F | Split `kTask_t` → `kProc_t` + `kThread_t` (proper process/thread separation) | ⬜ Deferred | Big refactor; v1 ships on shared-AS `kTask_t` without it |
| G | Shared **fd table** (`struct fdtable`) with correct cross-thread `close()`/`dup()` semantics | ⬜ Deferred | v1 shallow-shares `files[]` (threads share the `fileDescriptor_t` objects but keep separate arrays, so a `close()` in one thread isn't reflected in another's slot — fine for typical threaded code, not fully POSIX) |

**Legend:** ⬜ Not started/Deferred · 🔄 In progress · ✅ Done

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
