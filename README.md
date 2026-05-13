# UbixOS

**Version:** 2.0.0-BETA &nbsp;|&nbsp; **Architecture:** i386 (x86 32-bit) &nbsp;|&nbsp; **License:** BSD 3-Clause

UbixOS is a hobby/research operating system for the x86 platform, developed from scratch since 2002. It implements a monolithic kernel with a modular subsystem layout, a FreeBSD-derived POSIX userland, and its own native filesystem (UbixFS).

---

## Features

- **Kernel:** Preemptive multitasking, SMP support, x86 protected mode (ring 0/3)
- **Memory:** Paged virtual memory with copy-on-write forking
- **Filesystems:** UbixFS v1/v2, UFS, FAT16/32, DevFS — all via a VFS abstraction layer
- **Networking:** lwIP 2.0.3 TCP/IP stack; NE2000 and Lance NIC drivers
- **IPC:** Custom MPI message passing, POSIX pipes, semaphores
- **Drivers:** AT keyboard, PIT, PIC (8259), floppy, serial (RS-232), PCI hard disk, mouse
- **Userland:** 24+ utilities, FreeBSD-derived libc, C++ runtime, graphics library
- **ELF loader:** Loads and executes standard i386 ELF binaries

---

## Quick Start

```sh
bmake           # Build kernel + world
bmake image     # Build bootable disk image (ubixos.img)
bmake run       # Launch in QEMU
```

See [BUILDING.md](BUILDING.md) for full toolchain requirements and platform-specific setup.

---

## Directory Structure

| Path | Description |
|------|-------------|
| `sys/` | Kernel source — all subsystems |
| `bin/` | Userland executables |
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

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for conventions, code style, and the doc-sync rules that keep documentation current with the source.
