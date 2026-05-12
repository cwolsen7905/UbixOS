# ioctl / termios Redesign Plan

## Goal

Reach `tcgetattr`/`tcsetattr` working correctly so apps like ncurses and readline
can set canonical/raw mode and echo control without using the UbixOS-native
`tty_setraw()`/`tty_setecho()` API.

After completion:
- `tcgetattr(0, &t)` and `tcsetattr(0, TCSANOW, &t)` work
- `cfmakeraw(&t)` works
- `TIOCGWINSZ` returns 80×25
- `isatty(fd)` returns correct results for TTY and non-TTY fds
- Nothing currently working breaks: shell echo, login password masking, ed editor

## What Already Existed (Before This Work)

- `sys_ioctl` wired in `sys/kernel/descrip.c` — handled `TIOCGETA` (hardcoded flags)
  and `TIOCGWINSZ` (wrong: returned 50×80)
- `struct termios` and `struct winsize` defined in `sys/include/sys/ioctl.h`
- `fd_type` field in `struct file` (`sys/include/sys/descrip.h`), used only for pipes
- Keyboard ISR canonical/raw line discipline complete and working in `sys/isa/atkbd.c`
- `sys_ttyctrl` (native slot 42) implemented in `sys/fs/vfs/file.c`

## Phases

### Phase 1 — Complete the Kernel ioctl Handler ✅ DONE

**Files changed:**
- `sys/include/sys/ioctl.h` — added `TIOCSETA`, `TIOCSETAW`, `TIOCSETAF`,
  `TIOCSWINSZ`; added c_lflag constants (`ECHO`, `ICANON`, `ISIG`, etc.)
- `sys/kernel/descrip.c` — `sys_ioctl`:
  - `TIOCGETA`: now derives live `c_lflag` from `_current->term->t_raw` /
    `t_echo` instead of returning hardcoded values; also fixed `td_retval` early
    return so error path returns -1 correctly
  - Added `TIOCSETA`/`TIOCSETAW`/`TIOCSETAF`: reads `c_lflag`, writes
    `term->t_raw` and `term->t_echo`
  - `TIOCGWINSZ`: fixed to return `ws_row=25, ws_col=80`
  - Narrowed to fd 0/1/2 with NULL term guard; non-TTY fds return -1

**QEMU test:** `bin/ttytest` — call `ioctl(0, TIOCGETA)`, print flags, toggle
raw mode, verify single-char read without Enter, restore, verify canonical.

---

### Phase 2 — Userland termios + libc Functions ✅ DONE

**Depends on:** Phase 1

**Files to create:**
- `include/termios.h` — `struct termios`, flag constants (`ICANON`, `ECHO`,
  `TCSANOW`, `TCSADRAIN`, `TCSAFLUSH`, `NCCS`, etc.)
- `lib/libc/sys/termios.c` — `tcgetattr`, `tcsetattr`, `cfmakeraw`,
  `cfsetispeed`, `cfsetospeed`, `cfgetispeed`, `cfgetospeed`
- `lib/libc/sys/isatty.c` — `isatty(fd)`: calls `ioctl(fd, TIOCGETA)`,
  returns 1 on success, 0 on -1
- `lib/libc/sys/ioctl.c` — userland `ioctl()` wrapper around `int $0x80` slot 54

**Files to modify:**
- `lib/libc/sys/Makefile` — add `termios.o isatty.o ioctl.o`
- `include/unistd.h` — add `int isatty(int fd);`

**Side effect:** `isatty(0)` will now correctly return 1, potentially enabling
histedit in `bin/sh`. Verify shell still works interactively.

---

### Phase 3 — Regression Verification ✅ DONE

No code changes. Boot QEMU, log in, verify:
- Password field still masked (`tty_setraw` and `tcsetattr` write the same
  `t_raw`/`t_echo` fields — they are consistent)
- Shell echoes correctly
- `ed` works

---

### Phase 4 — fd Type Tag (Close the Narrow Shortcut)

**Depends on:** Phase 1 (independent of Phase 2)

**Files to modify:**
- `sys/include/sys/descrip.h` — add `#define FD_TYPE_TTY 1`
- fd init sites (fork, exec, task init) — set `fd_type = FD_TYPE_TTY` on fds 0/1/2
- `sys/kernel/descrip.c` `sys_ioctl` — check `fd->fd_type == FD_TYPE_TTY`
  instead of `fd <= 2`; return `ENOTTY` for non-TTY fds

**Risk:** Find every fd 0/1/2 initialization site before implementing.
Use `grep -rn "o_files\[0\]\|o_files\[1\]\|o_files\[2\]"`.
Missing one means `isatty(0)` returns 0 and the shell loses interactive mode.

---

### Phase 5 — Full termios State in tty_term (Optional, Later)

Store a full `struct termios` inside `tty_term` so `tcgetattr`/`tcsetattr`
round-trips are lossless and `ISIG`/`VEOF`/`VINTR` are honoured.

Not needed until a real ncurses port is attempted.

**Files:**
- `sys/include/ubixos/tty.h` — add `struct termios t_termios` to `tty_term`;
  keep `t_raw`/`t_echo` as ISR-readable cache
- `sys/kernel/tty.c` `tty_init` — initialize `t_termios` to defaults
- `sys/kernel/descrip.c` `sys_ioctl` — `TIOCGETA` copies `t_termios`;
  `TIOCSETA` writes `t_termios` and derives `t_raw`/`t_echo` from it

---

## Disposition of sys_ttyctrl (Native Slot 42)

Leave it fully functional. It writes the same `t_raw`/`t_echo` fields as the
POSIX ioctl path so they are automatically consistent. `login` depends on it.

Mark deprecated in a comment in `sys/kernel/syscalls.c` slot 42 but do not
remove. In a future cleanup `tty_setraw()`/`tty_setecho()` could be
reimplemented as wrappers over `tcsetattr`, but that is not urgent.

---

## Dependency Graph

```
Phase 1 (kernel: TIOCSETA + live TIOCGETA + fix TIOCGWINSZ)   ✅ DONE
    |
    +---> Phase 2 (userland: termios.h, tcgetattr, tcsetattr, cfmakeraw, isatty)
    |         |
    |         +---> Phase 3 (regression: login, shell, ed)
    |
    +---> Phase 4 (fd type tag — independent of Phase 2)
    |
    +---> Phase 5 (full struct termios in tty_term — optional, later)
```

---

## Key Breakage Risks

| Risk | Mitigation |
|------|------------|
| `TIOCGETA` flags don't round-trip (extra bits like ISIG discarded) | Accepted Phase 1/2 debt; fixed in Phase 5 |
| `isatty(0)` returning 1 enables histedit in `bin/sh` | Desired; test shell works |
| Phase 4 fd_type: missing an init site makes `isatty(0)` return 0 | Grep all sites first |
| `TIOCSETA` races with ISR reading `t_raw`/`t_echo` | Single-core i386, no preemption during syscall; safe |
