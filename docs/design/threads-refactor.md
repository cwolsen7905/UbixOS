# Threads — deferred follow-ups (refactor & polish)

> Backlog carved out of `completed/threading-plan.md` (2026-06-06). The threads
> v1 functional core is **done & verified** — `pthread_create`, `join`, `detach`,
> mutexes, and condition variables all work (see the archived plan). The items
> below are deliberately deferred: none blocks app use of threads. Tackle them
> when there's a concrete need.

## Status Matrix

Legend: ✅ done & verified · 🟡 partial · ⬜ not started · ⏸ blocked

| Item | Status | Notes |
|------|--------|-------|
| Threads v1 core — `pthread_create`/`join`/`detach`, mutex, cond | ✅ | archived `completed/threading-plan.md` |
| **E2c** — thread cancellation (`pthread_cancel`) | ⏸ | blocked on a 64-bit signal subsystem (SIGCANCEL = 33); flag-based deferred cancel partly works |
| **F** — split `kTask_t` → `kProc_t` + `kThread_t` | ⬜ | big refactor; coordinate with `smp-plan.md` (both reshape `kTask_t`); fixes the non-last-tgid page leak |
| **G** — true shared fd table (`struct fdtable`) | ⬜ | ideally depends on F; v1 shallow-shares `o_files[]` |

None of these blocks app use of threads — they are deliberate deferrals.

---

## E2c — Thread cancellation (`pthread_cancel`)

**Status:** Deferred. **Blocked on:** a 64-bit signal subsystem.

`pthread_cancel` sends **`SIGCANCEL` = signal 33** to the target thread; the
SIGCANCEL handler redirects a thread blocked in a cancellation-point syscall to
return `-ECANCELED`. UbixOS's signal layer is **32-bit / signals 1–31 only**
(`td.sig_pending` is a `u_int32_t`; `signal_post_kill` rejects `sig > 31`), so
signal 33 cannot be delivered.

What this needs (≈1–2 days; touches the working signal core — regression risk):
1. **64-bit signals** — widen `sig_pending` (and the sigmask path, `signal_check`,
   `signal_post*`, `sigaction`) to 64 bits so signals 32–64 work. *Bonus:* also
   unblocks musl's other implementation signals (`SIGTIMER` 32, `SIGSYNCCALL` 34)
   → POSIX timers and `__synccall` (used by `setuid` et al. across threads).
2. **Wake sleepers on signal post** — `signal_post*` must wake a target blocked in
   `sched_wait_event` (futex/cond), not just set the pending bit, or a blocked
   thread never notices the signal.
3. **Interruptible futex** — `sys_futex` returns `-EINTR` when an unblocked signal
   is pending, so the cancellation-point syscall unwinds and `signal_check` runs
   the SIGCANCEL handler.
4. Then verify with a `cancetest` (cancel a thread blocked in `pthread_cond_wait`).

Note: *flag-based deferred* cancellation (a thread reaching a cancellation point
*after* `pthread_cancel`) partly works already via the shared `cancel` flag with
no kernel change — only cancelling an **already-blocked** thread needs the above.

## F — Split `kTask_t` → `kProc_t` + `kThread_t`

**Status:** Deferred (big refactor; v1 runs on shared-AS `kTask_t`).

Separate "process" (address space, fd table, signals) from "execution context"
(stack, registers, priority). The scheduler would operate on `kThread_t`;
`kProc_t` is the unit for `fork()`/`waitpid()`.

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

**Fixes a known v1 leak:** PT/PD pages allocated by a *non-last* `tgid` member
aren't reclaimed today (the page bitmap is per-pid). Tgid/process-based page
ownership falls out of this split.

**Touches:** scheduler, fork, exec, signals. ~weeks. Keep coordinated with
`smp-plan.md` (both reshape `kTask_t`) — whichever lands second accommodates the
first. **Not an aarch64 prerequisite** (the port runs single-threaded processes
on the current `kTask_t`).

## G — True shared fd table

**Status:** Deferred. **Depends on:** ideally F (the `struct fdtable` lives on
`kProc_t`).

v1 **shallow-shares** `td.o_files[]`: threads share the `fileDescriptor_t` objects
but keep separate arrays, so a `close()`/`dup()` in one thread isn't reflected in
another thread's slot — fine for typical threaded code, not fully POSIX. A real
`struct fdtable` (refcounted, shared across a thread group) gives correct
cross-thread `close`/`dup` semantics, and lets the reaper free fds once (today the
reaper frees the legacy `files[0]` only, so no double-free but no proper shared
close either). ~days.
