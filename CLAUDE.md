# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

UbixOS is a hobby/research x86 (i386) operating system written in C and C++ (plus x86 assembly), developed since 2002. It compiles to a bare-metal ELF binary loaded by the FreeBSD bootloader chain, or (on the `feature/macos-build-qemu` branch) by GRUB2 via the multiboot protocol.

## Build System

The project uses **BSD make (`bmake`)**. On macOS use `bmake`; on FreeBSD `make` is already bmake. GNU make will not work — the Makefiles use BSD-specific syntax (`.CURDIR`, `.if`/`.else`, `!=` shell assignment).

```sh
bmake            # kernel + world + install
bmake kernel     # kernel only  → sys/compile/kernel
bmake world      # userland only → build/bin/, build/lib/, build/libexec/
bmake image      # create QEMU disk image (requires multiboot start.S — see feature branch)
bmake run        # launch QEMU with ubixos.img
bmake clean      # clean everything
```

On **macOS**, install prerequisites first:
```sh
brew install i386-elf-binutils i386-elf-gcc bmake qemu mtools grub
```

The Makefile auto-detects Darwin and sets `CROSS_PREFIX=i386-elf-` so all tool invocations use the cross toolchain. On FreeBSD the prefix is empty and the host toolchain is used directly.

### Install targets

`bmake install` copies build artifacts into two mount points:
- `ROOT=/ubixos` — primary UFS volume (kernel + world)
- `ROOT_FAT=/ubixos_fat` — FAT copy of the world (bin/, lib/, libexec/, etc/)

The kernel binary installs to `${ROOT}/boot/kernel/kernel`.

## Source Tree Architecture

The build is split into two independent halves: **kernel** (`sys/`) and **world** (userland).

### Kernel (`sys/`)

Each subsystem is its own directory with its own Makefile. `sys/Makefile` orchestrates them in order, producing `.o` files that are all linked together by `sys/compile/Makefile` using `sys/compile/ldscript.i386`.

Key subsystems and what they own:

| Directory | Owns |
|-----------|------|
| `sys/init/` | Bootstrap (`start.S` — entry point, GDT setup, FreeBSD bootinfo parsing) and `main.c` (`kmain`) |
| `sys/arch/i386/` | CPU: interrupts, syscall dispatch (`sys_call.S`/`sys_call_posix.S`), fork, scheduler, SMP AP trampoline |
| `sys/vmm/` | Virtual memory: paging, COW page fault handler, address space create/copy |
| `sys/kernel/` | Core services: scheduler, ELF loader, execve, signals, pipes, semaphores, TTY, sysctl, shutdown |
| `sys/fs/vfs/` | VFS layer — all filesystem calls route through here |
| `sys/fs/{ubixfs,ubixfsv2,ufs,fat,devfs}/` | Concrete filesystem drivers |
| `sys/mpi/` | Message-passing IPC (mailbox system used by init and drivers) |
| `sys/net/` | lwIP 2.0.3 integration; `net/netif/` bridges kernel NICs to lwIP |
| `sys/isa/` | ISA drivers: PIC (8259), PIT, AT keyboard, floppy, NE2000, RS-232, mouse |
| `sys/pci/` | PCI enumeration + IDE hard disk + Lance (PCNET) NIC |
| `sys/sde/` | Software display environment (C++ graphics layer) |
| `sys/lib/` | Kernel-internal library: `kprintf`, `kmalloc`, `vsprintf`, string ops |

**Kernel headers live in `sys/include/`** — this is the only include path (`-I`) passed to kernel compilation. Userland headers (`include/`) are never visible to kernel code.

**Linker script** (`sys/compile/ldscript.i386`): loads kernel at virtual address `0x20000`. The kernel virtual memory layout is lower 1 MB shared/identity-mapped, 1 MB–3 GB per-process, top 1 GB kernel-only.

### Userland (`bin/`, `lib/`, `libexec/`)

Built separately from the kernel with different flags (no `-nostdinc`, uses `include/` and `include_old/`). Libraries build first, then `libexec/`, then `bin/`. All output goes into `build/`.

- `lib/libc/` — FreeBSD-derived POSIX C library (primary libc)
- `lib/ubix/` — OS-specific startup code (static initializers, `crt1`)
- `libexec/` — runtime dynamic linker (`ld.so`); at runtime it expects libraries at `sys:/lib/`
- `bin/init/` — PID 1; uses MPI mailboxes, spawns `login`

### Third-party (`contrib/`)

lwIP 2.0.3, jemalloc, gdtoa (float↔ASCII), TCC (Tiny C Compiler), tzcode, NetBSD test suite. These are integrated into the kernel or world build but are not modified.

## Key Architectural Constraints

**Kernel entry point**: `sys/init/start.S` (`_start`) parses the FreeBSD `bootinfo` struct from the bootloader, zeroes BSS, sets up GDT segments, and calls `vmm_init()` then `kmain()`. Any change to the boot protocol (e.g. multiboot) must update both the assembly entry and `get_bootargs`.

**Syscall paths**: There are two syscall tables — native (`syscalls.c` / `sys_call.S`) and POSIX (`syscalls_posix.c` / `sys_call_posix.S`). New syscalls must be added to the correct table and have their number assigned. POSIX syscall numbers follow the FreeBSD ABI layout.

**IPC**: The kernel uses a custom MPI (message-passing) system, not System V IPC. `init` and most system processes communicate via MPI mailboxes. Pipes and semaphores are also available.

**VFS dispatch**: All filesystem calls go through `sys/fs/vfs/`. Each filesystem driver registers a set of function pointers. Do not call filesystem driver functions directly from outside the VFS layer.

**Memory allocation**: Use `kmalloc`/`kfree` (in `sys/lib/kmalloc.c`) inside the kernel. The kernel has no `malloc`. Userland uses the FreeBSD libc allocator backed by jemalloc.

## VS Code Integration

`.vscode/c_cpp_properties.json` provides two IntelliSense configurations:
- **Kernel** — uses `sys/include/`, `-nostdinc`, `i386-elf-gcc` at `/opt/homebrew/bin/i386-elf-gcc`
- **World** — uses `include/`, `include_old/`, lib headers

On Intel Macs the compiler path is `/usr/local/bin/i386-elf-gcc` — update `c_cpp_properties.json` if IntelliSense shows spurious errors.

Build tasks (`Ctrl+Shift+B`): Build Kernel, Build World, Build All, Create Disk Image, Run QEMU. The debug launch config connects `i386-elf-gdb` to QEMU's GDB stub on `localhost:1234`.

## Active Feature Branch

`feature/macos-build-qemu` — adds macOS cross-compilation support and a QEMU disk image pipeline. The next pending step on that branch is adding a multiboot header to `sys/init/start.S` so GRUB can load the kernel directly (currently `start.S` only handles the FreeBSD bootinfo protocol).
