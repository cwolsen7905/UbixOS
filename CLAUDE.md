# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

UbixOS is a hobby/research x86 (i386) operating system written in C and C++ (plus x86 assembly), developed since 2002. It boots via GRUB2 using the multiboot protocol, runs on bare metal or QEMU, and uses a FAT32 disk image as its primary filesystem.

## Build System

The project uses **BSD make (`bmake`)**. On macOS use `bmake`; on FreeBSD `make` is already bmake. GNU make will not work — the Makefiles use BSD-specific syntax (`.CURDIR`, `.if`/`.else`, `!=` shell assignment).

### Common targets

```sh
bmake                  # kernel + world (default)
bmake kernel           # kernel only → sys/compile/kernel
bmake world            # userland only → build/bin/, build/lib/, build/libexec/
bmake image            # build a fresh bootable FAT32 disk image from scratch
bmake run              # launch QEMU with ubixos.img
bmake run-debug        # headless QEMU, serial to stdout
bmake clean            # clean all build artifacts
```

### Disk image workflow

```sh
bmake mount-image      # mount ubixos.img FAT32 partition at /Volumes/UBIXOS
bmake unmount-image    # unmount it

bmake install-kernel   # mount → copy kernel → unmount
bmake install-world    # mount → copy world + source tree → unmount
bmake install          # install-world + install-kernel

bmake kernel-to-image  # fast path: mcopy kernel into existing image (no mount)
```

`bmake image` always builds a clean image from scratch via `tools/mkimage.sh` — use it for releases or first-time setup. `install-kernel` / `install-world` are faster incremental updates to an existing image.

The disk image (`ubixos.img`) can be mounted on macOS with `bmake mount-image` or directly via `hdiutil attach -imagekey diskimage-class=CRawDiskImage ubixos.img`. The FAT32 volume appears at `/Volumes/UBIXOS`.

### Installed layout on the disk image

| Path | Contents |
|------|----------|
| `/bin`, `/lib`, `/libexec` | Compiled world binaries and libraries |
| `/boot/kernel/kernel` | Kernel binary |
| `/boot/grub/` | GRUB config and modules |
| `/etc/` | System config (`userdb`, `fstab`, `motd`) |
| `/usr/include/` | Userland headers (for self-hosted builds) |
| `/usr/src/` | Full source tree (kernel + world, no build artifacts) |

The `/usr/src` layout mirrors FreeBSD convention to support eventual self-hosted compilation.

### macOS prerequisites

```sh
brew install x86_64-elf-binutils x86_64-elf-gcc bmake qemu mtools i686-elf-grub
```

The Makefile auto-detects Darwin and sets `CROSS_PREFIX=x86_64-elf-` with `CROSS_M32=-m32` so all compilations target i386. On FreeBSD the prefix is empty and the host toolchain is used directly.

> **Note**: The project uses `x86_64-elf-gcc -m32` (not `i386-elf-gcc`) because the Homebrew `i386-elf-gcc` formula is not maintained. All kernel and world CFLAGS include `-mno-sse -mno-sse2 -mno-mmx -mno-3dnow` because the kernel never sets `CR4.OSFXSR`; executing XMM instructions triggers `#UD`.

### Dependency tracking

All component Makefiles use `-MMD -MP` (via the `.incl` files) and include generated `.d` files via `.for`/`.sinclude`. Editing a header automatically recompiles dependent `.c` files on the next `bmake`.

## Source Tree Architecture

The build is split into two independent halves: **kernel** (`sys/`) and **world** (userland).

### Kernel (`sys/`)

Each subsystem is its own directory with its own Makefile. `sys/Makefile` orchestrates them in order, producing `.o` files that are all linked together by `sys/compile/Makefile` using `sys/compile/ldscript.i386`.

Key subsystems and what they own:

| Directory | Owns |
|-----------|------|
| `sys/init/` | Bootstrap (`start.S` — entry point, GDT setup, multiboot detection) and `main.c` (`kmain`) |
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

Built separately from the kernel with different flags. Libraries build first, then `libexec/`, then `bin/`. All output goes into `build/`.

- `lib/libc/` — FreeBSD-derived POSIX C library (primary libc)
- `lib/ubix/` — OS-specific startup code (static initializers, `crt1`)
- `lib/ubix_api/` — UbixOS-native API (`ubix_getcwd` etc.); header at `include/api/ubix.h`
- `libexec/` — runtime dynamic linker (`ld.so`); at runtime it expects libraries at `sys:/lib/`
- `bin/init/` — PID 1; uses MPI mailboxes, spawns `login`

### Third-party (`contrib/`)

lwIP 2.0.3, jemalloc, gdtoa (float↔ASCII), TCC (Tiny C Compiler), tzcode, NetBSD test suite. These are integrated into the kernel or world build but are not modified.

## Key Architectural Constraints

**Kernel entry point**: `sys/init/start.S` (`_start`) detects multiboot magic (`0x2BADB002` in `%eax`) and extracts boot device info before calling `vmm_init()` then `kmain()`. Any change to the boot protocol must update both the assembly entry and `get_bootargs`.

**Syscall paths**: There are two syscall tables — native (`syscalls.c` / `sys_call.S`, `int $0x81`) and POSIX (`syscalls_posix.c` / `sys_call_posix.S`, `int $0x80`). New syscalls must be added to the correct table. POSIX syscall numbers follow the FreeBSD ABI layout. The UbixOS-native API (`lib/ubix_api/`) uses `int $0x81`.

**VFS paths**: The kernel stores the full VFS path in `_current->oInfo.cwd` including mountpoint (e.g. `sys:/bin/`). POSIX `sys_getcwd` strips the mountpoint prefix for compatibility; the native `sys_getvfscwd` (slot 41) returns the full path. The shell uses `ubix_getcwd()` from `lib/ubix_api/` for its prompt.

**IPC**: The kernel uses a custom MPI (message-passing) system, not System V IPC. `init` and most system processes communicate via MPI mailboxes. Pipes and semaphores are also available.

**VFS dispatch**: All filesystem calls go through `sys/fs/vfs/`. Each filesystem driver registers a set of function pointers. Do not call filesystem driver functions directly from outside the VFS layer.

**Memory allocation**: Use `kmalloc`/`kfree` (in `sys/lib/kmalloc.c`) inside the kernel. Userland uses the FreeBSD libc allocator.

## VS Code Integration

`.vscode/c_cpp_properties.json` provides two IntelliSense configurations:
- **Kernel** — uses `sys/include/`, `-nostdinc`, `x86_64-elf-gcc` at `/opt/homebrew/bin/x86_64-elf-gcc`
- **World** — uses `include/`, lib headers

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

Requires: `brew install clang-format` (already in PATH) and `brew install llvm` (clang-tidy at `/opt/homebrew/opt/llvm/bin/clang-tidy`).

## Versioning

The single source of truth for the OS version is **`sys/include/ubixos/version.h`**. Edit only that file to bump the version — everything else derives from it automatically.

### Version bump checklist

1. Edit `UBIXOS_VERSION_MAJOR`, `MINOR`, `PATCH`, and `TAG` in [sys/include/ubixos/version.h](sys/include/ubixos/version.h).
2. Add a dated release section to `CHANGELOG.md` (rename `[Unreleased]` → `[X.Y.Z-TAG] - YYYY-MM-DD`, add a fresh empty `[Unreleased]` above it, update the footer diff links).
3. Rebuild: `bmake kernel world image`
4. Commit: `git add sys/include/ubixos/version.h CHANGELOG.md && git commit -m "Release X.Y.Z-TAG"`
5. Tag: `git tag -a vX.Y.Z-TAG -m "Release X.Y.Z-TAG" && git push && git push --tags`

## Current State

The system boots to a login prompt under QEMU:

1. GRUB2 (i686-elf-grub) loads the kernel via multiboot from a FAT32 disk image.
2. Kernel mounts the FAT32 partition as `sys:/` using the IDE + FAT driver stack.
3. `init` (PID 1) execs, forks `login`, which prompts for username/password.
4. Default credentials: `root` / `user` (from `tools/userdb`).
5. Shell prompt shows full VFS path: `uBixCube@sys:/bin/#`.

**Key lessons learned**:
- Use `x86_64-elf-gcc -m32` — the `i386-elf-gcc` Homebrew formula is unmaintained.
- All code must be compiled with `-mno-sse -mno-sse2 -mno-mmx -mno-3dnow`. GCC can silently emit XMM instructions for struct copies which trigger `#UD` fault 6.
- `kprintf` outputs to both VGA and COM1 serial. Run `bmake run` and check `serial.log` for kernel debug output.
- The FAT library treats the partition-relative sector 0 as BPB; `hdRead` adds `parOffset` (LBA 2048) transparently — do not double-add the offset.
- `sys:/etc/userdb` must exist on the image for `login` to authenticate. `tools/mkimage.sh` copies `tools/userdb` there automatically.
- TCC-compiled binaries require R_386_GOT32X relocation support (patched in `contrib/tcc/tccelf.c`).
