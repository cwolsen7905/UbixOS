# POSIX Signal Implementation Plan

## Background

UbixOS has partial signal infrastructure already in place:

- `struct thread` has `sigset_t sigmask` and `struct sigaction sigact[128]`
- `sys_sigaction` (slot 416) and `sys_sigprocmask` (slot 340) store handlers
- `sys_kill` (slot 37) exists but calls `endTask` directly, bypassing signals
- `sys_sigreturn` (slot 417) is wired but points to `sys_invalid`
- The C dispatcher `sys_call_posix()` (`sys/kernel/syscall_posix.c`) is the
  correct hook point to check pending signals before returning to user mode

### What broke Ctrl-C

The keyboard ISR (`sys/isa/atkbd.c`) detected Ctrl-C and called
`sched_killTree()` directly from interrupt context.  After adding `sti` to
`sys_call_posix.S` (to let the AC97 ISR fire during syscalls), the keyboard
ISR could fire while the process was mid-syscall.  `sched_killTree` marks the
target DEAD, but `sched()` may context-switch without sending the PIC EOI,
leaving IRQ 1 stuck in-service and killing further keyboard input.

The POSIX fix: keyboard ISR posts SIGINT (a single atomic bit-set); the signal
is delivered safely on the way back to user mode, after the syscall completes
and the PIC EOI has been sent.

---

## FreeBSD reference (how a modern Unix does it)

1. **TTY line discipline** intercepts Ctrl-C in process context — not the ISR.
   The ISR just enqueues the raw character.  The line discipline checks for
   the INTR character and calls `pgsignal(tty->t_pgrp, SIGINT, 1)`.
2. **Process groups** — Ctrl-C targets the foreground *process group*
   (`tty->t_pgrp`), set by the shell via `tcsetpgrp()` on `fork`.
3. **Deferred delivery** — the signal is posted to `p_siglist` (a bitmask).
   On every return from kernel mode (syscall exit, trap exit), the kernel
   checks "pending unblocked signals?" and builds a signal frame if so.

---

## Implementation Phases

### Phase 1 — Signal posting + default actions
**Status: `[x]` Complete**

| Task | File | Done |
|------|------|------|
| Add `volatile uint32_t sig_pending` to `struct thread` | `sys/include/sys/thread.h` | `[x]` |
| `signal_post(pid, sig)` — set bit in `td->sig_pending`; ISR-safe (no locks) | `sys/kernel/signal.c` | `[x]` |
| `signal_check(frame)` — on syscall exit: SIG_DFL terminating → `endTask+yield`; SIG_IGN → clear bit | `sys/kernel/signal.c` | `[x]` |
| Call `signal_check(frame)` at end of `sys_call_posix()` | `sys/kernel/syscall_posix.c` | `[x]` |
| Keyboard ISR Ctrl-C: `signal_post_tty(term, SIGINT)` | `sys/isa/atkbd.c` | `[x]` |
| USB HID keyboard: same fix | `sys/usb/hid_kbd.c` | `[x]` |
| `sys_kill`: call `signal_post` instead of `endTask` | `sys/kernel/gen_calls.c` | `[x]` |
| sigprocmask SIG_BLOCK/SIG_UNBLOCK/SIG_SETMASK logic | `sys/kernel/signal.c` | `[x]` |

**Bug fixed**: Ring-0 kernel stack overflow (`sig_pending = 0x20202020`).
FAT 8.3 space-padded filenames overwrote `td.sig_pending` when `sys_chdir`
overflowed the 4096-byte ring-0 stack.  Fix: increased to 8192 bytes in
`sched_core.c`.

---

### Phase 2 — Signal frame + `sys_sigreturn` (custom handlers)
**Status: `[x]` Complete**

#### Signal frame layout (pushed on user stack)

```
[new_esp+0]   sf_retaddr   → &sf_trampoline[0]  (handler's cdecl return addr)
[new_esp+4]   sf_signum                          (arg 1 to handler)
[new_esp+8]   sf_trampoline[14]                  (machine code for sigreturn)
[new_esp+22]  (2 bytes pad to uint32_t align)
[new_esp+24]  sf_eax … sf_edi, sf_eip, sf_eflags, sf_mask (saved CPU + sigmask)
```

| Task | File | Done |
|------|------|------|
| Define `struct ubx_sigframe` and `struct ubx_sigcontext` | `sys/include/ubixos/signal.h` | `[x]` |
| `signal_deliver_frame(sig, sa, frame, td)` | `sys/kernel/signal.c` | `[x]` |
| `sys_sigreturn(uap)` — restore trapframe + sigmask | `sys/kernel/signal.c` | `[x]` |
| Wire slot 417 to `sys_sigreturn` | `sys/kernel/syscalls_posix.c` | `[x]` |
| `signal_check` calls `signal_deliver_frame` for custom handlers | `sys/kernel/signal.c` | `[x]` |
| Interruptible reads (EINTR) for pipe, serial, VGA TTY | `sys/kernel/vfs_calls.c` | `[x]` |

---

### Phase 3 — Process groups + sigsuspend + pause + SIGCHLD
**Status: `[x]` Complete**

| Task | File | Done |
|------|------|------|
| `signal_post_tty` uses `term->t_pgrp`; fallback to highest-PID live task | `sys/kernel/signal.c` | `[x]` |
| `sys_setpgid` fixed — parent can set child's pgrp | `sys/kernel/gen_calls.c` | `[x]` |
| Stale `t_pgrp` cleared on pgrp-leader death | `sys/arch/i386/sched_switch.c` | `[x]` |
| `sys_sigsuspend` (slot 179) — atomically replace mask, sleep until unblocked signal | `sys/kernel/signal.c` | `[x]` |
| musl `pause()` patched to call `sigsuspend(&empty_mask)` (slot 29 = recvfrom in FreeBSD ABI) | `contrib/musl/src/unistd/pause.c` | `[x]` |
| SIGCHLD posted to parent when child enters DEAD state | `sys/arch/i386/sched_switch.c` | `[x]` |
| SIG_DFL for SIGCHLD is ignore (not in SIGTERM_MASK) | `sys/kernel/signal.c` | `[x]` |

**AST (Asynchronous Signal Trap)**: `signal_ast_check()` added to `timer.S` — fires after
`sched()` returns when the interrupted CS had RPL=3 (user mode).  Delivers SIG_DFL
terminate and SIG_IGN without waiting for the next syscall.  Custom handlers stay pending
for syscall-exit delivery (full register context not available in timer ISR).

**Known limitation**: Regular file reads (`read()` on FAT) run entirely in ring-0.
Ctrl-C during a large `fread` is delivered only when the syscall exits (when loading
finishes).  This is standard POSIX behavior — regular file reads are non-interruptible
at the kernel level.

---

### Phase 4 — `SA_SIGINFO` and `siginfo_t` population
**Status: `[x]` Complete**

| Task | File | Done |
|------|------|------|
| `SI_USER`, `SI_KERNEL`, `SI_QUEUE`, `SI_TIMER`, `SEGV_MAPERR`, `BUS_ADRALN` constants | `sys/include/sys/signal.h` | `[x]` |
| `struct ubx_musl_siginfo` — 128-byte musl-layout siginfo for user stack | `sys/include/ubixos/signal.h` | `[x]` |
| `struct ubx_sigframe_info` — SA_SIGINFO frame (3-arg handler) | `sys/include/ubixos/signal.h` | `[x]` |
| `signal_deliver_frame` branches on `SA_SIGINFO`: builds 3-arg frame with populated siginfo_t | `sys/kernel/signal.c` | `[x]` |

#### SA_SIGINFO frame layout

```
[+0]   retaddr   → &sf_trampoline
[+4]   sf_signo  arg1: signal number
[+8]   sf_info_ptr arg2: &sf_info (musl-layout siginfo_t)
[+12]  sf_uctx_ptr arg3: NULL (no ucontext yet)
[+16]  sf_info   siginfo_t, 128 bytes (musl layout)
[+144] sf_trampoline[14]
[+158] sf_pad[2]
[+160] sf_sc     saved CPU context
```

**siginfo_t layout note**: The kernel's FreeBSD-layout `siginfo_t` (64 bytes) and
musl's `siginfo_t` (128 bytes) differ in size but agree on field offsets for
`si_signo` (0), `si_errno` (4), `si_code` (8), `si_pid` (12), `si_uid` (16).
`si_addr` differs (FreeBSD: offset 24; musl: offset 12 via `__sigfault` union).
For now `si_code = SI_KERNEL`, `si_pid = 0`; per-signal sender tracking is future work.

**`ucontext`**: Passed as NULL.  FreeBSD `ucontext_t` population is future work.

---

### Phase 5 — Signal wrap-up
**Status: `[x]` Complete**

| Task | File | Done |
|------|------|------|
| `sig_code[32]` + `sig_extra[32]` union in `struct thread` for per-signal metadata | `sys/include/sys/thread.h` | `[x]` |
| `signal_post` / `signal_post_tty` set `sig_code = SI_KERNEL` | `sys/kernel/signal.c` | `[x]` |
| `signal_post_kill(sender, target, sig)` — sets `SI_USER` + `si_pid` for `sys_kill` | `sys/kernel/signal.c` | `[x]` |
| `sys_kill` calls `signal_post_kill` instead of `signal_post` | `sys/kernel/gen_calls.c` | `[x]` |
| `signal_post_fault(sig, addr, code)` — posts SIGSEGV/SIGBUS with fault metadata | `sys/kernel/signal.c` | `[x]` |
| `vmm_pageFault` delivers SIGSEGV (user mode) instead of kpanic/endTask | `sys/vmm/pagefault.c` | `[x]` |
| SA_SIGINFO frame uses `td->sig_code[sig-1]` / `td->sig_extra[sig-1]` | `sys/kernel/signal.c` | `[x]` |
| `SA_RESETHAND`: reset handler to SIG_DFL after delivery | `sys/kernel/signal.c` | `[x]` |
| `SA_RESTART`: save syscall# in `tf_err`; restore EIP-2 + syscall# in `save_sigcontext` | `sys/kernel/syscall_posix.c`, `sys/kernel/signal.c` | `[x]` |
| `fork.c` initializes `sig_code[]` and `sig_extra[]` in child | `sys/arch/i386/fork.c` | `[x]` |

**Remaining future work:**
- `ucontext_t` population: pass a real `ucontext_t` as arg3 to SA_SIGINFO handlers.
- `sigaltstack` / `SA_ONSTACK`: alternate signal stack.
- Interruptible regular-file reads (check `sig_pending` between IDE sectors).

---

## Key files

| File | Role |
|------|------|
| `sys/include/sys/thread.h` | `struct thread` — `sig_pending`, `sigmask`, `sigact[]` |
| `sys/include/sys/signal.h` | Signal numbers, `sigset_t`, `struct sigaction`, SI_ constants |
| `sys/include/ubixos/signal.h` | `ubx_sigframe`, `ubx_sigframe_info`, `ubx_musl_siginfo`, prototypes |
| `sys/kernel/signal.c` | All signal implementation |
| `sys/kernel/syscall_posix.c` | Hook: call `signal_check` before return |
| `sys/kernel/gen_calls.c` | `sys_kill`, `sys_setpgid` |
| `sys/kernel/syscalls_posix.c` | Slot wiring (417 sigreturn, 179 sigsuspend) |
| `sys/arch/i386/sched_switch.c` | SIGCHLD on child death, stale t_pgrp cleanup |
| `sys/arch/i386/timer.S` | AST: `signal_ast_check` after `sched()` |
| `sys/kernel/sched_core.c` | Ring-0 stack = 8192 bytes |
| `sys/isa/atkbd.c` | Keyboard ISR → `signal_post_tty` |
| `contrib/musl/src/unistd/pause.c` | pause() via sigsuspend (FreeBSD ABI compat) |
