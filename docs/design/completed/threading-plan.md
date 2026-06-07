# UbixOS Kernel Threads & POSIX Threads Plan  — COMPLETE (v1)

> **Archived 2026-06-06.** The threads v1 functional core is **done & verified**:
> `pthread_create`, `join`, `detach`, mutexes, and condition variables all work in
> QEMU. Remaining follow-ups (cancellation, the `kProc`/`kThread` split, a shared
> fd table) are deliberately deferred and tracked in `../threads-refactor.md` —
> none blocks app use of threads.
>
> Carved out of `scheduler-plan.md` (2026-06-03); builds on the completed
> scheduler (`scheduler-plan.md` in this directory).

## Goal

Give UbixOS genuine kernel threads — multiple execution contexts sharing one
address space — and wire musl's `pthread_*` to them.

**Approach (decided 2026-06-06):** ship working pthreads on a shared-address-space
`kTask_t` *first* (v1), and defer the full `kTask_t` → `kProc_t` + `kThread_t`
struct split to a follow-up. Get threads working in days, not a weeks-long refactor.

## Status at a glance

| # | Item | Status |
|---|------|--------|
| A | `clone()`/`rfork` syscall — new context sharing the caller's `cr3` | ✅ Done & verified |
| B | Address-space refcount — free shared `cr3` only when the last `tgid` task exits | ✅ Done & verified |
| C | TLS — per-thread userland `%gs` base | ✅ Done & verified |
| D | `futex` syscall — wait/wake on a user address | ✅ Done & verified |
| E | musl pthreads — `pthread_create` / `mutex` / `join` | ✅ Milestone 1 done & verified |
| E2a | pthreads — `pthread_cond` | ✅ Done & verified (`bin/condtest`) |
| E2b | pthreads — detached threads | ✅ Done & verified (`bin/detachtest`) |
| E2c | pthreads — cancellation | ⏭️ Deferred → `../threads-refactor.md` (needs 64-bit signals) |
| F | Split `kTask_t` → `kProc_t` + `kThread_t` | ⏭️ Deferred → `../threads-refactor.md` |
| G | True shared fd table | ⏭️ Deferred → `../threads-refactor.md` |

**Legend:** ✅ Done · ⏭️ Deferred (see backlog)

**Bottom line:** the v1 functional core (A–E + E2a/E2b) is **complete and verified**
— real `pthread_create`/mutex/cond/join/detach work. Cancellation (E2c) turned out
to need a 64-bit signal subsystem (`SIGCANCEL`=33 exceeds UbixOS's 31-signal limit),
so it joins F/G in `../threads-refactor.md`. None of the deferred items blocks app
use of threads.

## What shipped (v1 core — all verified in QEMU)

- **A — `sys_rfork`** (FreeBSD slot 251, `fork.c`): new `kTask_t` shares the
  caller's `cr3`, runs on a caller-supplied stack at the same eip with `eax=0`,
  shallow-shares fds, joins the caller's `tgid`. musl `clone.s` drives it.
  Test: `bin/clonetest`. *(Surfaced + fixed a latent POSIX bug: `execve` didn't
  reset caught signal dispositions — `i386_exec.c` now does, per POSIX.)*
- **B — AS refcount** (`sched_tgid_others_alive`, `endTask`, reaper): the shared
  `cr3` is torn down only when the last task of a `tgid` exits.
- **C — TLS**: `set_thread_area` records `kTask_t.tls_base`; `cpu_switch`
  re-installs it into the shared LDT[1] after the CR3 swap (all threads share one
  LDT slot). `fork` gives the LDT page a private writable copy (never COW — it's
  CPU state). Test: `bin/tlstest`.
- **D — `sys_futex`** (`gen_calls.c`) on `sched_wait_event_timeout` /
  `sched_wakeup_chan`: WAIT/WAKE/WAIT_BITSET, REQUEUE-as-WAKE. Test:
  `bin/futextest`.
- **E (milestone 1)**: `sys_rfork` honours CLONE_SETTLS / PARENT_SETTID /
  CHILD_CLEARTID (the last releases musl's thread-list lock on exit via a futex
  wake in `endTask`). Test: `bin/pthreadtest` (4 threads, mutex-guarded counter,
  joins).

**Syscall ABI placement** (see memory `project_native_abi_threading`): Linux-only
primitives with no FreeBSD number live on the **native** table (`int $0x81`):
`futex` 64, `set_thread_area` 63, `exit_group` 65 — musl flags them with `0x8000`.
Calls FreeBSD *does* have stay on the **POSIX** table at their real number:
`rfork` 251, `membarrier` 584.

## Deferred refactor (F) — target shape

```c
typedef struct kProc {
    pidType          pid;
    kTask_t         *threads;       /* threads belonging to this process */
    struct vmspace  *vm;            /* address space */
    struct fdtable  *fds;           /* file descriptor table (G) */
    sigset_t         sigmask;
    /* ... uid, gid, cwd, etc. */
} kProc_t;

typedef struct kThread {
    tidType          tid;
    kProc_t         *proc;          /* owning process */
    uint8_t          priority, base_priority, quantum;
    struct md_thread md;            /* arch registers/stack */
    /* ... per-thread errno, TLS pointer, signal mask */
} kThread_t;
```

The scheduler operates on `kThread_t`; `kProc_t` is the unit for `fork()`/`waitpid()`.

## Dependencies / relationship to other work

- **Built on the completed scheduler** (`completed/scheduler-plan.md`).
- **Not an arm64 prerequisite** — arm64 boots and runs single-threaded processes
  without this; threads are an app-facing capability to add post-port. TLS is
  arch-specific (i386 `%gs`/LDT vs aarch64 `TPIDR_EL0`) and would move behind
  `<machine/tls.h>` per `cross-arch-plan.md`.
- **Independent of SMP** — threads (many contexts/process) and SMP (many CPUs)
  compose but neither requires the other. Both touch `kTask_t`; keep the struct
  changes clean for whichever lands second. See `smp-plan.md`.
