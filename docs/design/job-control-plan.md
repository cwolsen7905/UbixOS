# Job Control Implementation Plan

## Goal

Full POSIX job control so that tcsh (and eventually bash) can use Ctrl-Z,
`fg`, `bg`, and `jobs` correctly — equivalent to what FreeBSD or Linux provide.

---

## What Is Already Done

| Feature | Status | Notes |
|---------|--------|-------|
| Process groups (`pgrp`, `sid`) in `kTask_t` | ✅ | Set by `sys_setpgid`, inherited in fork |
| `sys_setpgid` / `sys_getpgid` | ✅ | Signal plan Phase 3 |
| `TIOCSCTTY` / `TIOCGPGRP` / `TIOCSPGRP` | ✅ | TTY plan Phases 1–3 |
| SIGTSTP delivered by Ctrl-Z (via `c_cc[VSUSP]` in `tty_inject`) | ✅ | TTY plan Phase 6 |
| SIGTTOU / SIGTTIN for background I/O | ✅ | TTY plan Phase 12 |
| SIGCONT, SIGSTOP signal numbers defined | ✅ | `sys/include/i386/signal.h` |
| `signal_post_pgrp(pgrp, sig)` | ✅ | Added in TTY plan Phase 12 |
| `sys_wait4` waits for child death | ✅ | Doesn't handle WUNTRACED/WSTOPPED |
| `signal_check` at syscall exit | ✅ | Delivers pending signals to user |

---

## What Is Missing

### 1. STOPPED task state

`kTask_t.state` has: `PLACEHOLDER=-2, DEAD=-1, NEW=0, READY=1, RUNNING=2,
IDLE=3, WAIT=5, UNINTERRUPTIBLE=6, INTERRUPTIBLE=7`.

There is no `STOPPED` state.  A process that receives SIGTSTP or SIGSTOP must
enter a scheduler state where it is not runnable but is also not dead.
`sched_yield` must skip STOPPED tasks entirely.

### 2. SIGTSTP/SIGSTOP default action must stop, not terminate

`signal_check` currently treats signals not in `SIGTERM_MASK` as "terminate".
`SIG_DFL` for SIGTSTP, SIGSTOP, and SIGTTIN/SIGTTOU must put the process into
STOPPED state instead.

### 3. SIGCONT must wake a stopped process

When SIGCONT is posted to a stopped process, it must transition from STOPPED
back to READY — even before `signal_check` runs at syscall exit, because a
stopped process never exits a syscall.

### 4. `sys_wait4` must handle `WUNTRACED` and report stop status

tcsh calls `waitpid(child, &status, WUNTRACED)` to learn that a child was
stopped by a signal.  `sys_wait4` currently only wakes when a child dies.
It must also wake when a child enters STOPPED state, and encode the stop
signal in the `wstatus` word using the standard POSIX encoding:
`((sig & 0x7f) << 8) | 0x7f` (stopped) vs `(sig & 0x7f)` (killed).

### 5. Shell must be able to resume jobs with SIGCONT + `tcsetpgrp`

When the user types `fg %1`, tcsh:
1. Calls `tcsetpgrp(fd, job_pgrp)` — sets `term->t_pgrp = job_pgrp`
2. Sends SIGCONT to `job_pgrp`
3. Calls `waitpid(job_pgrp, &status, WUNTRACED)` to wait for it to stop or exit

This requires `tcsetpgrp` to work (it's `TIOCSPGRP` — already done ✅) and
SIGCONT delivery to wake the stopped process (Phase 3 below).

---

## Implementation Phases

### Phase A — Add STOPPED state to scheduler

**Files:** `sys/include/ubixos/sched.h`, `sys/kernel/sched_core.c`

- Add `STOPPED = 8` to the `tState` enum
- Add `void sched_stop(kTask_t *t)` — transition to STOPPED, analogous to
  `sched_wakeup` but opposite direction
- In `sched_core.c` scheduler loop: skip tasks with `state == STOPPED`
  (already skip DEAD; same pattern)
- In `sched_switch.c` (the cleanup path): STOPPED is not dead — do not reap

---

### Phase B — SIGTSTP/SIGSTOP/SIGTTIN/SIGTTOU default action: stop

**Files:** `sys/kernel/signal.c` `signal_check`

Currently `signal_check` has a `SIGTERM_MASK` of signals that terminate by
default.  Add a `SIGSTOP_MASK` for signals whose default is to stop:

```c
#define SIGSTOP_MASK  ((1u << (SIGTSTP-1)) | (1u << (SIGSTOP-1)) | \
                       (1u << (SIGTTIN-1)) | (1u << (SIGTTOU-1)))
```

In `signal_check`, before the terminating-signal check, add:

```c
if ((pending_bit & SIGSTOP_MASK) && sa->sa_handler == SIG_DFL) {
    sched_stop(_current);
    sched_yield();   /* reschedule; we will not return until SIGCONT wakes us */
    return;
}
```

Note: SIGSTOP cannot be caught or ignored — add a guard in `sys_sigaction`
that rejects `SA_HANDLER` for SIGSTOP.

---

### Phase C — SIGCONT wakes stopped processes immediately

**Files:** `sys/kernel/signal.c` `signal_post` and/or `signal_post_pgrp`

When SIGCONT is posted to a process/pgrp:
- For each matching task where `state == STOPPED`: call `sched_wakeup(t)`
  (transition to READY) before setting `sig_pending`
- The woken task will resume at the next `sched_yield` return in `sched_stop`
- Additionally: clear any pending SIGTSTP/SIGSTOP/SIGTTIN/SIGTTOU bits from
  `sig_pending` (POSIX says SIGCONT discards pending stop signals)

Special case: `signal_post_pgrp` handles SIGCONT — iterate `taskList`,
wake STOPPED tasks in the group, then set the SIGCONT bit so user handlers
(if registered) also fire.

---

### Phase D — `sys_wait4` handles WUNTRACED + encodes stop status

**Files:** `sys/kernel/gen_calls.c` `sys_wait4`

Add `t_stopped_sig` field to `kTask_t` (the signal that caused the stop, or 0).

Modify `sys_wait4`:

```c
#define WUNTRACED   0x0002   /* report stopped children */
#define WNOHANG     0x0001   /* don't block */

/* wstatus encoding */
#define W_STOPPED(sig)  (((sig) << 8) | 0x7f)  /* stopped by signal */
#define W_EXITED(code)  ((code) << 8)           /* normal exit */
#define W_SIGNALED(sig) ((sig) & 0x7f)          /* killed by signal */
```

In the wait loop:
- If `WUNTRACED` is set, also break when child enters STOPPED state
- Write `W_STOPPED(child->t_stopped_sig)` to `*wstatus`
- Return child pid

For `WNOHANG`: don't block; return 0 immediately if no child has exited or
stopped yet.

---

### Phase E — `tcsetpgrp` + shell fg/bg wiring (no kernel changes needed)

`tcsetpgrp(fd, pgrp)` is `TIOCSPGRP` — already implemented. The shell:
1. On Ctrl-Z: child receives SIGTSTP, enters STOPPED; shell's `waitpid` with
   `WUNTRACED` returns with stop status; shell records the job
2. On `fg`: shell calls `tcsetpgrp(tty_fd, job_pgrp)`, then
   `kill(job_pgrp, SIGCONT)`, then `waitpid(job_pgrp, &s, WUNTRACED)`
3. On `bg`: shell calls `kill(job_pgrp, SIGCONT)` only (no tcsetpgrp, job
   stays in background)

No kernel changes needed for Phase E beyond Phases A–D.

---

## Dependency Graph

```
Phase A  (STOPPED scheduler state)
    ↓
Phase B  (SIGTSTP/SIGSTOP SIG_DFL stops instead of terminates)
    ↓
Phase C  (SIGCONT wakes stopped processes)         ← needed for fg to work
    ↓
Phase D  (sys_wait4 WUNTRACED + wstatus encoding)  ← needed for shell to detect stop
    ↓
Phase E  (tcsh fg/bg — no kernel changes)
```

All four kernel phases must be done before Ctrl-Z/fg/bg works end-to-end.
Estimated effort: A+B = small, C = small, D = medium (most complex).

---

## Key Files

| File | Change |
|------|--------|
| `sys/include/ubixos/sched.h` | Add `STOPPED=8` to `tState`; declare `sched_stop` |
| `sys/kernel/sched_core.c` | Implement `sched_stop`; skip STOPPED in run loop |
| `sys/arch/i386/sched_switch.c` | Don't reap STOPPED tasks in cleanup path |
| `sys/kernel/signal.c` | `signal_check`: SIGSTOP_MASK stops instead of kills; `signal_post`/`signal_post_pgrp`: SIGCONT wakes STOPPED tasks |
| `sys/kernel/gen_calls.c` | `sys_wait4`: WUNTRACED, WNOHANG, stop-status encoding |
| `sys/include/ubixos/sched.h` | Add `t_stopped_sig` to `kTask_t` |

---

## Key Risks

| Risk | Mitigation |
|------|------------|
| STOPPED task never wakes if SIGCONT is lost | Ensure `signal_post_pgrp(SIGCONT)` always calls `sched_wakeup` before returning, even if `sig_pending` bit is already set |
| Shell's `waitpid(-1, WUNTRACED)` returns spuriously | Only report stop once per stop event; clear `t_stopped_sig` after waitpid collects it |
| `sched_stop` called from signal delivery (interrupt context) | Use existing `irq_save_disable` pattern; `sched_stop` only sets state, `sched_yield` does the reschedule |
| SIGSTOP inside `sys_sigaction` bypass | Reject `SA_SIGINFO`/non-DFL handler for SIGSTOP in `sys_sigaction` |
