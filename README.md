# UbixOS

**Version:** 2.2.0-BETA &nbsp;|&nbsp; **Architecture:** i386 (x86 32-bit) &nbsp;|&nbsp; **License:** BSD 3-Clause

UbixOS is a hobby/research operating system for the x86 platform, developed from scratch since 2002. It implements a monolithic kernel with a modular subsystem layout, a FreeBSD-derived POSIX userland, and its own native filesystem (UbixFS).

---

## Features

- **Kernel:** Preemptive multitasking with O(1) scheduler (QoS classes, I/O boost, starvation aging); SMP support; x86 protected mode (ring 0/3)
- **Memory:** Paged virtual memory with copy-on-write forking
- **Filesystems:** UbixFS v1/v2, UFS, FAT16/32, DevFS, procfs — all via a VFS abstraction layer
- **Networking:** lwIP 2.0.3 TCP/IP stack; NE2000, Lance (PCNET), and Intel e1000 NIC drivers
- **IPC:** Custom MPI message passing, POSIX pipes, semaphores
- **Signals:** POSIX `sigaction`/`sigprocmask`/`sigsuspend`; `SIGSTOP`/`SIGCONT`; ZOMBIE/wait4 lifecycle
- **Drivers:** AT keyboard, PIT, PIC (8259), floppy, serial (RS-232), PCI hard disk, PS/2 mouse, USB mass storage
- **GUI:** Composited window system (`views`), taskbar, VT100 terminal — all via shared-memory MPI protocol
- **Userland:** 30+ utilities including tcsh 6.24.16, ed, uname, mount, ps; FreeBSD-derived libc; C++ runtime (libc++ 18 subset); objgfx rendering library
- **ELF loader:** Loads and executes standard i386 ELF binaries; runtime dynamic linker (`ld.so`)

---

## Quick Start

```sh
bmake           # Build kernel + world
bmake image     # Build bootable disk image (ubixos.img)
bmake run       # Launch in QEMU
```

**Real hardware (Raspberry Pi 3):** a board is aarch64 with the same world — only
the kernel differs — so boards nest under the arch (ARCH + BOARD):

```sh
bmake world TARGET=aarch64   # the shared aarch64 world
bmake image-rpi3             # Pi 3 kernel + microSD image → build/aarch64/boards/rpi3/rpi3-sd.img
```

See [BUILDING.md](BUILDING.md) for full toolchain requirements and platform-specific
setup, and `docs/design/raspberry-pi-3b-bringup.md` for the Pi 3 port + flashing.

---

## Directory Structure

| Path | Description |
|------|-------------|
| `sys/` | Kernel source — all subsystems |
| `bin/` | Essential user commands → `/bin` (sh, init, ls, cp, …) |
| `sbin/` | System/admin tools → `/sbin` (fdisk, mount, ubfs, …) |
| `usr.bin/` | Bulk user commands + GUI apps → `/usr/bin` (grep, vi, vdoom, …) |
| `usr.sbin/` | Daemons + services → `/usr/sbin` (views, authd, logd, sshd, …) |
| `tests/` | Test/dev harnesses → `/usr/tests` (not shipped apps) |
| `lib/` | Userland libraries (libc, libstdc++, graphics, etc.) |
| `libexec/` | Runtime linker and library execution support |
| `include/` | Userland POSIX-style headers |
| `contrib/` | Third-party libraries (lwIP, jemalloc, TCC, etc.) |
| `etc/` | System configuration files |
| `share/` | Shared data files |
| `tools/` | Build and installation utilities |
| `docs/` | Architecture, design, and driver documentation |
| `build/` | Compiled object output (generated) |
| `debug/` | Debugging utilities and test code |

For a detailed breakdown of kernel internals, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## Documentation

| Document | Description |
|----------|-------------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Kernel subsystems, memory layout, boot sequence |
| [BUILDING.md](BUILDING.md) | Toolchain setup, build steps, QEMU and VirtualBox |
| [SYSCALLS.md](SYSCALLS.md) | Syscall tables, ABI, and how to add a new syscall |
| [DEBUG.md](DEBUG.md) | Debug defines, serial output, GDB |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Conventions, PR workflow, doc-sync rules |
| [docs/](docs/README.md) | Architecture deep-dives, design specs, driver guide |

---

## License

UbixOS itself is licensed under the **BSD 3-Clause License**. The `sys/`,
`bin/`, `lib/`, `libexec/`, `share/`, `tools/`, and `include/` trees are
all original UbixOS code under that license.

### Third-party components

The `contrib/` tree contains imported third-party code, each retained
under its original license. The compiled UbixOS image bundles these
binaries alongside the BSD-licensed base — analogous to how FreeBSD
historically shipped GPL'd gcc/binutils in `/usr/bin`. License terms
apply per-binary; the BSD base is unaffected.

| Component | Version | Purpose | License |
|-----------|---------|---------|---------|
| [musl](contrib/musl/) | git snapshot | libc (dynamic, ld-musl-i386.so.1) | MIT |
| [libcxx](contrib/libcxx/) | 18.x subset | LLVM C++ standard library | Apache 2.0 with LLVM exception |
| [libcxxabi](contrib/libcxxabi/) | 18.x subset | Itanium C++ ABI | Apache 2.0 with LLVM exception |
| [lwip-2.0.3](contrib/lwip-2.0.3/) | 2.0.3 | TCP/IP stack | BSD 3-Clause |
| [jemalloc](contrib/jemalloc/) | — | malloc implementation | BSD 2-Clause |
| [tcsh-6.24.16](contrib/tcsh-6.24.16/) | 6.24.16 | Interactive shell | BSD 3-Clause |
| [tcc](contrib/tcc/) | — | Tiny C Compiler (self-hosted dev) | LGPL 2.1 |
| [tzcode](contrib/tzcode/) | — | POSIX time-zone routines | Public Domain |
| [gdtoa](contrib/gdtoa/) | David Gay's | strtod/dtoa floating-point conversion | Permissive (Gay) |
| [libc-pwcache](contrib/libc-pwcache/) | FreeBSD | passwd/group cache | BSD 2-Clause |
| [libc-vis](contrib/libc-vis/) | FreeBSD | string-visualization helpers | BSD 3-Clause |
| [minimp3](contrib/minimp3/) | — | MP3 decoder (used by `mp3play`) | CC0 / Public Domain |
| [doomgeneric](contrib/doomgeneric/) | id Software | DOOM source release (`bin/doom`, `bin/vdoom`) | GPL 2 |
| [busybox-vi](contrib/busybox-vi/) | 1.36.1 (vi only) | `bin/vi` editor | GPL 2 |

The GPL'd components (`doomgeneric`, `busybox-vi`) build into standalone
executables. Their source remains in `contrib/` (and is shipped under
`/usr/src/` on the disk image) per GPL source-redistribution terms.
The BSD-licensed UbixOS base does not link against or derive from
GPL'd code.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for conventions, code style, and the doc-sync rules that keep documentation current with the source.
