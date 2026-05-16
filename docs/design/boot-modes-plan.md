# UbixOS Boot Modes & Terminal Plan

## Overview

UbixOS supports two boot modes selected at runtime by which services are
enabled in `sys:/etc/init.d/`.  No kernel rebuild is required to switch
between them — editing two files on the disk image is enough.

| Mode | Use case |
|------|----------|
| **Graphical** | Desktop workstation, default |
| **Headless** | IoT device, server, embedded, recovery |

---

## init.d service matrix

| File | Graphical | Headless | Notes |
|------|-----------|----------|-------|
| `10-logd` | enabled | enabled | kernel log drain, always on |
| `15-ubistry` | enabled | enabled | registry service, always on |
| `20-views` | enabled | disabled | compositor; owns the entire graphical path |
| `30-ttyd` | optional | enabled | serial terminal on COM1 |

`guilogin` is **not** an init.d entry — views owns it internally (see below).

---

## Graphical path

```
init
 ├── logd      (10-logd)
 ├── ubistry   (15-ubistry)
 └── views     (20-views)
       │
       │  [after framebuffer init, views forks:]
       └── guilogin  (internal child, not in init.d)
             │
             │  [on successful authentication:]
             └── taskbar  (desktop shell)
                   │
                   │  [user action:]
                   └── term, other apps
```

### views — compositor and display server

- Owns the VESA framebuffer exclusively via `sys_mapfb` (syscall 43).
- After its framebuffer and compositor loop are initialised, it forks
  `guilogin` as its first managed child.
- If guilogin exits abnormally, views re-forks it (same restart-on-exit
  loop that init uses for login today).
- All other graphical processes receive a shared-memory buffer via
  `sys_shareregion` (syscall 45) and never touch the framebuffer directly.

### guilogin — graphical login screen

- **Not in init.d.**  Views spawns it; init knows nothing about it.
- Receives a fullscreen shared-memory window buffer from views via MPI +
  `sys_shareregion`.
- Draws with `objgfx`: UbixOS branding, username field, password field
  (masked), status line.
- Reads keyboard input via `sys_getkbd` (syscall 46).
- Authenticates against `sys:/etc/userdb` using the same logic as
  `bin/login`.
- On success: `fork` + `execve` taskbar, then exits.  Views sees guilogin
  exit cleanly and does not restart it.
- On failure: clears password field, shows error, waits for retry.

### taskbar — desktop shell

- Taskbar is the desktop shell, equivalent to macOS Finder or Windows
  Explorer.
- Launched by guilogin on successful login; nothing else runs at that
  point.
- A fresh desktop is presented — no terminal window opened automatically.
- App launching (term, future apps) is initiated by the user through the
  taskbar.
- Keyboard hotkeys for the compositor (e.g. bring-terminal-to-front) are
  a taskbar/compositor concern, implemented when the shell matures.  They
  are not a kernel-level feature.

### Serial in graphical mode

Serial (COM1) remains write-only kprintf debug output.  `bmake run-debug`
routes it to stdout on the host.  No change from today.

---

## Headless path

```
init
 ├── logd      (10-logd)
 ├── ubistry   (15-ubistry)
 └── ttyd      (30-ttyd)
       │
       │  [per connection on COM1:]
       └── login → shell
```

When `20-views` is absent from init.d, init falls through to the existing
text login loop (unchanged from today).  `ttyd` provides an additional
terminal session over COM1 for remote/serial access.

### Serial terminal — what needs to be built

Currently COM1 is write-only (kprintf).  Making it a proper POSIX terminal
requires three pieces:

#### 1. Serial RX driver  (`sys/isa/serial.c`)

- Enable COM1 receive interrupts (IRQ4, currently only TX is wired).
- ISR pushes received bytes into `serial_rx_ring[]` — same circular buffer
  pattern as `kbd_ring[]` in `sys/isa/atkbd.c`.
- Existing TX path (`kprintf` → `outb` to 0x3F8) is unchanged.

#### 2. `sys_serial_read` syscall (native ABI, slot 48)

```c
struct sys_serial_read_args {
    char    *buf;
    int      max;
};
int sys_serial_read(struct thread *, struct sys_serial_read_args *);
```

- Drains `serial_rx_ring[]` into the userspace buffer, up to `max` bytes.
- Returns number of bytes copied, 0 if ring is empty.
- Non-blocking (ttyd polls with `sched_yield` between reads, same as logd).
- A VFS `/dev/ttyS0` device node is a future improvement; the syscall is
  sufficient for now and keeps the implementation contained.

#### 3. `bin/ttyd` — serial terminal daemon

- Poll loop: `sys_serial_read` → accumulate into line buffer.
- Line discipline: echo received characters back via `kprintf_serial` (or
  a new `sys_serial_write`), handle backspace, Ctrl+C, Enter.
- On Enter: `fork` + `execve` `sys:/bin/login` (or `sys:/bin/shell`
  directly for passwordless serial access — configurable).
- Bridge child's stdin/stdout through the serial port.
- When the child exits, print a new prompt and wait for the next session.

The QEMU `-serial stdio` flag (already used in `bmake run-debug`) lets
ttyd be tested without physical hardware.

---

## Implementation phases

### Phase 1 — Serial terminal (headless path)

1. `sys/isa/serial.c`: enable RX interrupt, add `serial_rx_ring[]`.
2. `sys/include/sys/sysproto.h`: add `sys_serial_read_args`.
3. `sys/kernel/syscalls.c`: wire slot 48.
4. `sys/kernel/fb.c` (or new `sys/kernel/serial_sys.c`): implement
   `sys_serial_read`.
5. `bin/ttyd/main.c`: poll loop, line discipline, session fork.
6. `tools/initd/30-ttyd`: add (disabled by default).
7. Test: `bmake run-debug`, enable 30-ttyd, connect with host terminal.

### Phase 2 — Graphical login (guilogin)

1. `bin/guilogin/main.c`: fullscreen views window, objgfx form, userdb
   auth, fork taskbar on success.
2. `bin/guilogin/Makefile`: add to world build.
3. `bin/Makefile`: add `guilogin` to SUBDIRS.
4. `bin/views/` (compositor): after init, fork `sys:/bin/guilogin`; on
   clean exit do not restart; on crash exit restart.
5. `tools/initd/20-views`: uncomment to enable graphical boot.
6. Test: `bmake run`, boot into GUI login, authenticate, confirm taskbar
   appears with empty desktop.

### Phase 3 — Taskbar as desktop shell

1. Taskbar gets an app launcher (right-click context menu or dock strip).
2. Launching `sys:/bin/term` from taskbar opens a terminal window.
3. Compositor-level keyboard shortcuts (e.g. focus-terminal) added here,
   implemented at the views/taskbar layer rather than the kernel.

---

## What does NOT change

- `kprintf` — unchanged, always writes to VGA text buffer and COM1.
- `bin/login` — unchanged, used by the text login loop and optionally by
  ttyd for headless sessions.
- `sys/isa/atkbd.c` — unchanged, keyboard events feed `kbd_ring` as today.
- The POSIX and native syscall tables below slot 48 — no renumbering.
- Existing `bin/term`, `bin/taskbar`, `bin/views` binaries — views gets
  a guilogin fork call added, everything else is untouched for Phase 1.

---

## File checklist

### New files
| File | Phase |
|------|-------|
| `sys/isa/serial_rx.c` (or extend `serial.c`) | 1 |
| `bin/ttyd/main.c` | 1 |
| `tools/initd/30-ttyd` | 1 |
| `bin/guilogin/main.c` | 2 |
| `bin/guilogin/Makefile` | 2 |

### Modified files
| File | Change | Phase |
|------|--------|-------|
| `sys/include/sys/sysproto.h` | add `sys_serial_read_args` | 1 |
| `sys/kernel/syscalls.c` | slot 48 | 1 |
| `sys/kernel/fb.c` | `sys_serial_read` impl | 1 |
| `bin/Makefile` | add ttyd, guilogin to SUBDIRS | 1+2 |
| `bin/views/*.cc` | fork guilogin after init | 2 |
| `tools/initd/20-views` | uncomment exec line | 2 |
