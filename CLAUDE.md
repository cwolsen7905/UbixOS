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
brew install x86_64-elf-binutils x86_64-elf-gcc bmake qemu mtools i686-elf-grub
```

The Makefile auto-detects Darwin and sets `CROSS_PREFIX=x86_64-elf-` with `CROSS_M32=-m32` so all compilations target i386. On FreeBSD the prefix is empty and the host toolchain is used directly.

> **Note**: The project uses `x86_64-elf-gcc -m32` (not `i386-elf-gcc`) because the Homebrew `i386-elf-gcc` formula is not maintained. `x86_64-elf-gcc` supports `-m32` to produce i386 output. All kernel and world CFLAGS include `-mno-sse -mno-sse2 -mno-mmx -mno-3dnow` because the kernel never sets `CR4.OSFXSR`; executing XMM instructions in kernel or userspace triggers `#UD`.

The `bmake image` target calls `tools/mkimage.sh` which creates a single FAT32 raw disk image (`ubixos.img`) with GRUB embedded in sectors 1-2047 and all world files in the FAT32 partition starting at LBA 2048. Serial output from the kernel is available on COM1 (captured with `-serial file:serial.log` in the run target).

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

**Kernel entry point**: `sys/init/start.S` (`_start`) supports both multiboot (GRUB) and the legacy FreeBSD `bootinfo` protocol. On the `feature/macos-build-qemu` branch, GRUB loads via multiboot; `start.S` detects the multiboot magic (`0x2BADB002` in `%eax`) and extracts boot device info before calling `vmm_init()` then `kmain()`. Any change to the boot protocol must update both the assembly entry and `get_bootargs`.

**Syscall paths**: There are two syscall tables — native (`syscalls.c` / `sys_call.S`) and POSIX (`syscalls_posix.c` / `sys_call_posix.S`). New syscalls must be added to the correct table and have their number assigned. POSIX syscall numbers follow the FreeBSD ABI layout.

**IPC**: The kernel uses a custom MPI (message-passing) system, not System V IPC. `init` and most system processes communicate via MPI mailboxes. Pipes and semaphores are also available.

**VFS dispatch**: All filesystem calls go through `sys/fs/vfs/`. Each filesystem driver registers a set of function pointers. Do not call filesystem driver functions directly from outside the VFS layer.

**Memory allocation**: Use `kmalloc`/`kfree` (in `sys/lib/kmalloc.c`) inside the kernel. The kernel has no `malloc`. Userland uses the FreeBSD libc allocator backed by jemalloc.

## VS Code Integration

`.vscode/c_cpp_properties.json` provides two IntelliSense configurations:
- **Kernel** — uses `sys/include/`, `-nostdinc`, `x86_64-elf-gcc` at `/opt/homebrew/bin/x86_64-elf-gcc`
- **World** — uses `include/`, `include_old/`, lib headers

On Intel Macs the compiler path is `/usr/local/bin/x86_64-elf-gcc` — update `c_cpp_properties.json` if IntelliSense shows spurious errors.

Build tasks (`Ctrl+Shift+B`): Build Kernel, Build World, Build All, Create Disk Image, Run QEMU. The debug launch config connects `x86_64-elf-gdb` to QEMU's GDB stub on `localhost:1234`.

## Code Style and Tooling

Coding style is **FreeBSD `style(9)`**: 8-space hard tabs, Allman braces, 80-column limit, pointer aligned to variable name (`int *foo`). Style is enforced by `.clang-format` at the repo root.

Apply **file-by-file as files are touched** — do not reformat the whole tree at once (breaks `git blame`).

```sh
clang-format -i sys/vmm/paging.c   # reformat one file in place
```

**`tools/mcr.sh`** — Machine Code Review: runs clang-format + clang-tidy against changed files.

```sh
tools/mcr.sh                  # check files changed vs HEAD (default)
tools/mcr.sh --staged         # check staged files only
tools/mcr.sh --fix            # auto-apply clang-format fixes
tools/mcr.sh sys/vmm/paging.c # check specific file(s)
tools/mcr.sh --format-only    # skip clang-tidy (faster)
```

Requires: `brew install clang-format` (already in PATH) and `brew install llvm` (clang-tidy at `/opt/homebrew/opt/llvm/bin/clang-tidy`). Neither affects the cross-compiler build.

VS Code formats on save automatically via `.vscode/settings.json` + `.clang-format`.

## Current State (feature/macos-build-qemu)

The `feature/macos-build-qemu` branch is fully functional for macOS development. The system boots to a login prompt under QEMU:

1. GRUB2 (i686-elf-grub) loads the kernel via multiboot from a FAT32 disk image.
2. Kernel mounts the FAT32 partition as `sys:/` using the IDE + FAT driver stack.
3. `init` (PID 1) execs, forks `login`, which prompts for username/password.
4. Default credentials: `root` / `user` (from `tools/userdb`).

**Key lessons learned on this branch**:
- Use `x86_64-elf-gcc -m32` — the `i386-elf-gcc` Homebrew formula is unmaintained.
- All code (kernel and userland) must be compiled with `-mno-sse -mno-sse2 -mno-mmx -mno-3dnow`. The kernel never sets `CR4.OSFXSR`; GCC can silently emit `movdqa`/XMM instructions for struct copies which trigger `#UD` fault 6.
- `kprintf` outputs to both VGA and COM1 serial. Run `bmake run` and check `serial.log` for kernel debug output.
- The FAT library (`sys/fs/fat/fat_access.c`) treats the partition-relative sector 0 as BPB; `hdRead` adds `parOffset` (LBA 2048) transparently — do not double-add the offset.
- `sys:/etc/userdb` must exist on the image for `login` to authenticate. `tools/mkimage.sh` copies `tools/userdb` there automatically.
