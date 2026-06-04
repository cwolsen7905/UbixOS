# TTY / termios / ioctl Implementation Plan

## Goal

A complete POSIX TTY layer so that shells, ncurses apps, and job-control work
correctly — equivalent to what FreeBSD or macOS provide on a serial terminal.

---

## What Is Already Done

| Feature | Status | Notes |
|---------|--------|-------|
| `struct termios` in `tty_term` (c_iflag/oflag/cflag/lflag/cc/speeds) | ✅ | Full FreeBSD ABI layout, initialised to sane defaults in `tty_init` |
| `struct winsize` in `tty_term` (all 4 fields) | ✅ | ws_row=25, ws_col=80, pixels=0 |
| `t_pgrp` foreground process-group tracking | ✅ | Set by TIOCSCTTY; used by `signal_post_tty` |
| TIOCGETA / TIOCSETA / TIOCSETAW / TIOCSETAF | ✅ | TIOCSETAW/AF accept but don't drain/flush yet (Phase 6) |
| TIOCGWINSZ / TIOCSWINSZ | ✅ | Stores size; SIGWINCH not yet sent (Phase 5) |
| TIOCGPGRP / TIOCSPGRP / TIOCSCTTY | ✅ | |
| FIONREAD | ✅ | Returns `stdinSize` |
| FD_TYPE_TTY / FD_TYPE_TTYV fd tagging | ✅ | ioctl and read/write route on fd_type |
| Canonical mode — line buffering, newline flush | ✅ | `t_linebuf` in `tty_inject` |
| Canonical mode — echo | ✅ | Conditional on `t_echo` |
| Canonical mode — ERASE (0x08 / DEL) | ✅ | BS+space+BS echo |
| Raw mode — immediate pass-through | ✅ | |
| Stdout/stderr write routing | ✅ | `fd->fd == NULL` path in `sys_write` calls `tty_print(_current->term)` |
| SIGINT generation (Ctrl-C) | ✅ | In keyboard ISR, delivered to `t_pgrp` |
| POSIX signal infrastructure (phases 1–5) | ✅ | See `docs/design/completed/signal-plan.md` |

---

## Open Phases

### Phase 4 — fd-type cleanup (fd 0/1/2 always has correct type) ✅ DONE

The `fd->fd == NULL` write path works, but fds 0/1/2 inherited across fork+exec
still carry `fd_type == 0` (not `FD_TYPE_TTY`).  This makes `isatty(1)` on
stdout rely on the fd-NULL heuristic rather than an explicit tag.

Also: `sys_write` routes raw-TTY fds through `_current->term`, but if
`_current->ct_tty` and `_current->term` differ (after `setsid()`) writes go to
the wrong terminal.  Fix: prefer `ct_tty` over `term` in the write path.

**Files:**
- `sys/kernel/vfs_calls.c` `sys_write` — use `ct_tty ?: term`, not just `term`
- `sys/arch/i386/fork.c` — ensure fds 0/1/2 get `fd_type = FD_TYPE_TTY` when
  they are raw tty fds (fd->fd == NULL)
- `sys/arch/i386/i386_exec.c` `sys_exec` — same for exec'd processes

---

### Phase 5 — TIOCSWINSZ → SIGWINCH ✅ DONE

`TIOCSWINSZ` stores the new size in `t_winsize` but never notifies the
foreground process.  Shells and editors rely on SIGWINCH to redraw.

**Files:**
- `sys/kernel/descrip.c` `sys_ioctl` case `TIOCSWINSZ`:
  - After storing the new `t_winsize`, call `signal_post_tty(term, SIGWINCH)`
    (same helper used by Ctrl-C)

---

### Phase 6 — Line discipline: signal generation from `c_cc[]` ✅ DONE

Currently SIGINT is generated in the keyboard ISR with a hardcoded check for
`0x03`.  The correct architecture is:

1. ISR delivers every raw byte to `tty_inject` unconditionally.
2. `tty_inject` checks `c_lflag & ISIG` and compares the byte against
   `c_cc[VINTR]`, `c_cc[VQUIT]`, `c_cc[VSUSP]` to generate signals.
3. ISR no longer contains any signal logic.

This also adds the two missing signals:

| Char | `c_cc` index | Signal | Default key |
|------|-------------|--------|-------------|
| VINTR | 0 | SIGINT | Ctrl-C (0x03) |
| VQUIT | 1 | SIGQUIT | Ctrl-\\ (0x1C) |
| VSUSP | 2 | SIGTSTP | Ctrl-Z (0x1A) |

**Files:**
- `sys/kernel/tty.c` `tty_inject` — add ISIG block at top (before canonical
  processing): if `(term->t_termios.c_lflag & ISIG)` compare ch to c_cc values
  and call `signal_post_tty(term, sig)` then `return`
- `sys/isa/atkbd.c` `keyboardHandler` — remove hardcoded `if (ch == 0x03)`
  SIGINT block; just call `tty_inject`
- `sys/usb/hid_kbd.c` — same removal

---

### Phase 7 — Line discipline: `c_cc[]` dispatch for editing chars ✅ DONE

Special chars (ERASE, KILL, WERASE) are matched via hardcoded constants in
`tty_inject`'s `switch(ch)`.  If a user runs `stty erase ^?` the new value has
no effect.

**Changes in `sys/kernel/tty.c` `tty_inject`:**
- Replace `case '\b': case 0x7F:` with `if (ch == term->t_termios.c_cc[VERASE])`
- Replace `case 0x15:` (KILL) with `if (ch == term->t_termios.c_cc[VKILL])`
- Add WERASE: `if (ch == term->t_termios.c_cc[VWERASE])` — walk `t_linebuf`
  backwards, skipping trailing spaces then non-space chars; echo erasure if
  `ECHOE` set
- Add EOF: `if (ch == term->t_termios.c_cc[VEOF])` — flush the partial line to
  stdin even if no `\n`, signal EOF to the reader (set `stdinSize` with a
  sentinel or use a separate flag)
- KILL echo: after clearing `t_linelen`, if `ECHOK` set echo `\n`

---

### Phase 8 — Input flag enforcement (ICRNL, IXON) ✅ DONE

`t_termios.c_iflag` is stored correctly but never consulted.  The most
important flags:

| Flag | Value | Effect |
|------|-------|--------|
| ICRNL | 0x100 | Translate CR (0x0D) → NL (0x0A) on input |
| INLCR | 0x040 | Translate NL → CR |
| IGNCR | 0x080 | Discard CR |
| IXON  | 0x200 | Ctrl-S stops output; Ctrl-Q restarts |

Currently ICRNL is hardcoded always-on.  IXON is never enforced.

**Changes in `sys/kernel/tty.c` `tty_inject`:**
- Gate the `'\r' → '\n'` translation on `c_iflag & ICRNL`
- Add IXON: if `(c_iflag & IXON)` and ch == `c_cc[VSTOP]` set a `t_stopped`
  flag; ch == `c_cc[VSTART]` clears it.  In `sys_write` / `tty_print`, spin on
  `t_stopped` before emitting each byte (or just discard for now as a first pass)

---

### Phase 9 — Output flag enforcement (OPOST/ONLCR) ✅ DONE

`tty_print` hardcodes CR/LF/backspace/tab handling without checking `c_oflag`.

**Changes in `sys/kernel/tty.c` `tty_print`:**
- Gate all output translation on `c_oflag & OPOST`
- Within OPOST, gate `\n → \r\n` on `c_oflag & ONLCR`
- Gate `\r` at column-0 suppression on `c_oflag & ONOCR`

---

### Phase 10 — TIOCSETAW / TIOCSETAF real drain; TIOCDRAIN; TIOCFLUSH ✅ DONE

Currently `TIOCSETAW` and `TIOCSETAF` apply the new `termios` immediately
without draining/flushing output.  This is wrong when a process calls
`tcsetattr(fd, TCSADRAIN, ...)` (e.g. after printing a prompt before going raw).

UbixOS has no output queue (output is synchronous via `tty_print`), so "drain"
is a no-op for now — but the ioctl should still flush the *input* buffer on
`TIOCSETAF` / `TCSAFLUSH`.

**Files:**
- `sys/kernel/descrip.c` `sys_ioctl`:
  - `TIOCSETAF`: before applying new attrs, set `term->stdinSize = 0;
    term->t_linelen = 0;`
  - Add `TIOCDRAIN`: return 0 immediately (output is synchronous)
  - Add `TIOCFLUSH`: clear `stdinSize`/`t_linelen` for FREAD; no-op for FWRITE

---

### Phase 11 — Missing ioctls (TIOCNOTTY, TIOCEXCL, TIOCOUTQ, FIONREAD cleanup) ✅ DONE

Low-priority stubs so apps don't get EINVAL:

| ioctl | Stub behaviour |
|-------|----------------|
| TIOCNOTTY | Set `_current->ct_tty = NULL`; return 0 |
| TIOCEXCL | Set a `t_exclusive` flag; return 0 (don't enforce yet) |
| TIOCNXCL | Clear `t_exclusive`; return 0 |
| TIOCOUTQ | Return 0 (synchronous output, queue always empty) |
| TIOCSTI | Inject one byte via `tty_inject(term, byte)` |
| TIOCCONS | Redirect kprintf to this tty (future) |

---

### Phase 12 — Background job I/O blocking (SIGTTOU / SIGTTIN) ✅ DONE

POSIX requires that a background process writing to its controlling terminal
receives SIGTTOU (unless the signal is ignored or `TOSTOP` is clear in
`c_lflag`).  Background reads always receive SIGTTIN.

This depends on Phase 6 (pgrp delivery working) and Phase 5 (correct pgrp).

**Changes in `sys/kernel/vfs_calls.c`:**
- `sys_write`: if `fd->fd == NULL || fd_type == FD_TYPE_TTY/TTYV` and
  `term->t_pgrp != 0 && _current->pgrp != term->t_pgrp` and
  `c_lflag & TOSTOP`, post SIGTTOU to `_current->pgrp`; return EINTR
- `sys_read`: same check → post SIGTTIN

---

## Dependency Graph

```
✅ Phases 1–3  (TIOCSETA, termios.h, tcgetattr/tcsetattr, isatty)
✅ Phase 5-old (full struct termios in tty_term)

✅ Phase 4   (fd-type cleanup, ct_tty vs term in write path)

✅ Phase 5   (TIOCSWINSZ → SIGWINCH)           independent
✅ Phase 6   (ISIG in ldisc, SIGQUIT+SIGTSTP)  independent; unlocks Phase 12
✅ Phase 7   (c_cc[] dispatch, WERASE, KILL echo, EOF)  depends on Phase 6 layout
✅ Phase 8   (input flags: ICRNL gate, IXON)   independent
✅ Phase 9   (output flags: OPOST/ONLCR gate)  independent
✅ Phase 10  (TIOCSETAF flush, TIOCDRAIN)       depends on Phase 7 (input flush)
✅ Phase 11  (stub ioctls)                      independent
✅ Phase 12  (SIGTTOU/SIGTTIN)                  depends on Phase 6
```

**Recommended order for a working interactive shell:**
4 → 6 → 7 → 5 → 8 → 9 → 10 → 11 → 12

---

## Key Risks

| Risk | Mitigation |
|------|------------|
| Removing hardcoded Ctrl-C from ISR breaks SIGINT | Phase 6 must add `tty_inject` ISIG path before removing ISR code |
| c_cc[] dispatch: wrong index offsets | Cross-check against `contrib/musl/arch/i386/bits/termios.h` VINTR=0, VQUIT=1, VSUSP=2, VKILL=5, VERASE=3, VEOF=4, VWERASE=14 |
| ONLCR change wraps output differently | Test `printf "hello\n"` before and after Phase 9 |
| SIGTTOU (Phase 12) may break existing pipe-based programs | Gate strictly on `c_lflag & TOSTOP`; default `TOSTOP` is 0 (off) so existing behaviour preserved |
