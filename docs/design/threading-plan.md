# UbixOS Kernel Threads & POSIX Threads Plan

> Carved out of `scheduler-plan.md` (2026-06-03). The scheduler itself (priority
> run queue, QoS, boosts, PI, aging) is **complete** — see
> `completed/scheduler-plan.md`. This plan builds on it: kernel threads + POSIX
> threads.

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
| E2 | pthreads — `cond`, detached threads, cancellation | 🔲 Remaining |
| F | Split `kTask_t` → `kProc_t` + `kThread_t` | 🔲 Deferred |
| G | True shared fd table | 🔲 Deferred |

**Legend:** ✅ Done · 🔄 In progress · 🔲 Not started/Deferred

**Bottom line:** the v1 *functional core* (A–E milestone 1) is complete and
verified — real `pthread_create`/mutex/join work. The plan is **not 100% complete**:
E2 polish and the F/G refactors remain (all non-blocking for app use).

## What's left — decision matrix

| Item | What it is | Effort | Blocks | Notes |
|------|-----------|--------|--------|-------|
| **E2a — `pthread_cond`** | Verify condition variables (producer/consumer test). Runs on the existing futex; uses `FUTEX_WAIT`-with-timeout + the REQUEUE-as-WAKE path. | ~½ day | A worker-pool / job-queue design (e.g. the **NetSurf async fetcher**) | Cheap to test now; likely works, but unverified. Recommended before building anything on cond vars. |
| **E2b — detached threads** | `pthread_detach` / detached-create teardown. Needs a kernel-assisted "unmap own stack + exit" syscall: the stack-arg ABI can't `munmap` its own stack then pass `exit`'s args (no stack left). `__unmapself.s` is currently a documented non-functional stub. | ~½–1 day | Detached threads only (joinable threads work — the joiner frees the map) | Add a native `thread_exit_unmap(base,size,code)` call that does both in one trap. |
| **E2c — cancellation** | `pthread_cancel` / `SIGCANCEL` delivery into a blocked futex (`-EINTR` return). | ~½ day | `pthread_cancel` users (rare) | Futex currently doesn't return `-EINTR` on signal; a blocked thread defers signals until woken. |
| **F — kProc/kThread split** | Separate "process" (AS, fds, signals) from "execution context" (stack, regs). The scheduler would operate on `kThread_t`. | ~weeks | Cleanliness; fixes the v1 PT/PD page leak below | Big refactor; intentionally deferred. v1 runs on shared-AS `kTask_t`. |
| **G — shared fd table** | `struct fdtable` with correct cross-thread `close()`/`dup()`. | ~days | Fully-POSIX fd semantics across threads | v1 shallow-shares `o_files[]` (threads share the `fileDescriptor_t` objects but keep separate arrays — a `close()` in one thread isn't reflected in another's slot). Fine for typical threaded code. |

**Known v1 leaks (tracked under F/G, not blocking):**
- PT/PD pages allocated by a *non-last* `tgid` member aren't reclaimed (the page
  bitmap is per-pid). Fix = `tgid`-based page ownership (F).
- Shallow-shared `o_files` aren't freed by the reaper (it frees the legacy
  `files[0]`), so no double-free, but no proper shared-close either (G).

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
