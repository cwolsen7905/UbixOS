# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

UbixOS is a hobby/research x86 (i386) operating system written in C and C++ (plus x86 assembly), developed since 2002. It boots via GRUB2 using the multiboot protocol, runs on bare metal or QEMU, and uses a FAT32 disk image as its primary filesystem.

## Build System

The project uses **BSD make (`bmake`)**. On macOS use `bmake`; on FreeBSD `make` is already bmake. GNU make will not work — the Makefiles use BSD-specific syntax (`.CURDIR`, `.if`/`.else`, `!=` shell assignment).

### Common targets

```sh
bmake                  # kernel + world (default)
bmake kernel           # kernel only → build/boot/kernel
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

The NetSurf browser build (Step 4 of `bmake world`) additionally needs a host
**`bison` ≥ 3** (Apple's `/usr/bin/bison` 2.3 is too old for nsgenbind's
parser) and host **`libpng`** (for NetSurf's `convert_image` resource tool):

```sh
brew install bison libpng
```

`tools/build-netsurf.sh` auto-detects the keg-only brew bison
(`/opt/homebrew/opt/bison`) and resolves libpng via `pkg-config`.

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

**Linker script** (`sys/compile/ldscript.i386`): loads kernel at physical/virtual address `0x300000` (3 MB). The first 4 MB of physical RAM is identity-mapped (PD[0], all 1024 PT entries). Physical layout: ISA/VGA/BIOS at `0x0–0xFFFFF`, page bitmap at `0x101000+` (1 MB for 256 MB RAM), kernel at `0x300000`. `vmm_memMapInit` places the bitmap at `page_align(_end)` — RAM-size-independent. Free pages begin immediately after the bitmap; the first 1 MB and kernel+bitmap pages are reserved. The kernel virtual memory layout is lower 4 MB identity-mapped, 4 MB–3 GB per-process, top 1 GB kernel-only.

### Userland (`bin/`, `lib/`, `libexec/`)

Built separately from the kernel with different flags. Libraries build first, then `libexec/`, then `bin/`. All output goes into `build/`.

- `lib/libc/` — FreeBSD-derived POSIX C library (primary libc)
- `lib/ubix/` — OS-specific startup code (static initializers, `crt1`)
- `lib/ubix_api/` — UbixOS-native API (`ubix_getcwd` etc.); header at `include/api/ubix.h`
- `libexec/` — the active runtime linker is musl's (`/lib/ld-musl-i386.so.1`); libraries are at `/lib/`. (The older native `libexec/ld`, which used `sys:/lib/`, is superseded.)
- `bin/init/` — PID 1; uses MPI mailboxes, spawns `login`

### Display stack

Two-layer graphical system modelled after macOS WindowServer + Core Graphics:

| Layer | Component | Role |
|-------|-----------|------|
| Compositor | `bin/views/` (C++) | Owns the VESA framebuffer via `sys_mapfb()`. Manages windows, server-side decorations, Z-order, drag, close. Composites shared-memory buffers to screen. |
| App rendering | `lib/objgfx/` (C++) | Surface drawing API (`ogSurface`, `ogScalableFont` (TrueType); `ogBitFont` is legacy). Apps render into their shared-memory window buffer using this library. |

**Rules:**
- `views` is the only process that calls `sys_mapfb()`. All other processes get a `vmm_share_region` buffer.
- Apps draw with `objGFX` (`ogSurface`/`ogScalableFont`; `ogBitFont` is legacy). No app writes to the framebuffer directly.
- MPI carries only signals (`DISPLAY_CLAIM`, `DISPLAY_FLIP`, `DISPLAY_RELEASE`), never drawing commands.
- objGFX headers live in `include/objgfx/`. Apps include with `<objgfx/objgfx.h>` and pass `-I../../include` (not `-I../../lib/objgfx`).

Display protocol header: `include/views/display_proto.h`. Design document: `docs/design/display-plan.md`.

### Third-party (`contrib/`)

lwIP 2.0.3, jemalloc, gdtoa (float↔ASCII), TCC (Tiny C Compiler), tzcode, NetBSD test suite. These are integrated into the kernel or world build but are not modified.

## Key Architectural Constraints

**Kernel entry point**: `sys/init/start.S` (`_start`) detects multiboot magic (`0x2BADB002` in `%eax`) and extracts boot device info before calling `vmm_init()` then `kmain()`. Any change to the boot protocol must update both the assembly entry and `get_bootargs`.

**Syscall paths**: There are two syscall tables — native (`syscalls.c` / `sys_call.S`, `int $0x81`) and POSIX (`syscalls_posix.c` / `sys_call_posix.S`, `int $0x80`). New syscalls must be added to the correct table. POSIX syscall numbers follow the FreeBSD ABI layout. The UbixOS-native API (`lib/ubix_api/`) uses `int $0x81`.

**VFS paths**: Fully POSIX. Mount points are POSIX paths (`/` root FAT32, `/dev` devfs, `/proc` procfs); the `sys:/` mountpoint prefix is gone. `_current->oInfo.cwd` is a plain POSIX path (new processes start at `/`). `sys_getcwd` (POSIX) and `sys_getvfscwd` (native slot 41) now both return that cwd verbatim — the old strip-vs-full distinction is vestigial. The shell uses `ubix_getcwd()` from `lib/ubix_api/` for its prompt.

**IPC**: The kernel uses a custom MPI (message-passing) system, not System V IPC. `init` and most system processes communicate via MPI mailboxes. Pipes and semaphores are also available.

**VFS dispatch**: All filesystem calls go through `sys/fs/vfs/`. Each filesystem driver registers a set of function pointers. Do not call filesystem driver functions directly from outside the VFS layer.

**Memory allocation**: Use `kmalloc`/`kfree` (in `sys/lib/kmalloc.c`) inside the kernel. Userland uses the FreeBSD libc allocator.

## VS Code Integration

`.vscode/c_cpp_properties.json` provides two IntelliSense configurations:
- **Kernel** — `sys/include/**`, `x86_64-elf-gcc` (no `-nostdinc` in IntelliSense args so GCC built-in headers like `stdint.h` are found)
- **World** — `include/**`, musl and libcxx contrib headers

Both configs reference `compile_commands.json` via `"compileCommands"`. When that file exists (generated by `bear`), IntelliSense uses the exact per-file flags from the build and the fallback paths are ignored.

**Generating `compile_commands.json` (do once after any major Makefile change):**
```sh
python3 tools/gen-compile-commands.py --world
```
This walks `sys/`, `bin/`, `lib/`, and `libexec/` and emits one entry per source file with the correct per-subsystem flags. The resulting `compile_commands.json` in the repo root gives IntelliSense perfect per-file include paths and defines. Commit it so other developers get correct IntelliSense without running any tools.

Note: `bear` (Build EAR) does not work reliably on macOS with SIP enabled because `DYLD_INSERT_LIBRARIES` is blocked for sub-processes launched by bmake. The generator script above is the reliable alternative.

On Intel Macs the compiler path is `/usr/local/bin/x86_64-elf-gcc` — update `c_cpp_properties.json` if IntelliSense shows spurious errors.

Build tasks (`Ctrl+Shift+B`): Build Kernel, Build World, Build All, Create Disk Image, Run QEMU. The debug launch config connects `x86_64-elf-gdb` to QEMU's GDB stub on `localhost:1234`.

## Code Style and Tooling

Coding style is **FreeBSD `style(9)`**: 8-space hard tabs, Allman braces, 120-column limit (see `.clang-format`), pointer aligned to variable name (`int *foo`). Style is enforced by `.clang-format` at the repo root.

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

### Mandatory rules for all new code

**Every new function or method must have a Doxygen doc block** immediately above its definition using this format:

```c
/**
 * Brief one-line description of what the function does.
 *
 * Longer explanation if the behaviour is non-obvious — constraints,
 * side-effects, locking requirements, etc.  Omit if the brief line
 * is sufficient.
 *
 * @param foo  Only when range, ownership, or in/out direction is non-obvious.
 * @return 0 on success, -errno on failure.  (omit for void functions)
 */
```

- The brief line is **required** for every new function — no exceptions.
- Expand to multiple paragraphs only when the WHY or constraints are non-obvious.
- Use `@param` **selectively** — only when the parameter's valid range, ownership, or in/out direction is not obvious from its name and type. Do not list every parameter mechanically (`@param foo the foo value` adds no information).
- Always include `@return` for non-void functions.
- C++ methods follow the same format; use `/** */` not `//`.

**File-scope variable rules:**

- File-scope variables that are not part of the public API **must be `static`** — enforced by `misc-use-internal-linkage` in `.clang-tidy`.
- All `static` data definitions must appear **at the top of the source file**, after `#include`s and before any function definitions. Do not scatter them between functions.
- Dead file-scope variables (no callers in the translation unit and no declaration in any header) must be removed, not left for later.

**Formatting and linting are mandatory on every touched file:**

1. Run `clang-format -i <file>` on every `.c`, `.cc`, `.cpp`, `.h` file you write or modify.
2. Run `tools/mcr.sh <file>` (clang-format + clang-tidy) before committing. Fix all errors; warnings in existing code may be left but must not be introduced by new code.
3. Naming conventions enforced by `.clang-tidy`: functions and variables `lower_case`, global/static variables `g_` prefix, typedefs `_t` suffix, enum values `UPPER_CASE`, macros `UPPER_CASE`.

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
2. Kernel mounts the FAT32 partition at `/` using the IDE + FAT driver stack.
3. `init` (PID 1) execs, forks `login`, which prompts for username/password.
4. Default credentials: `root` / `user` (from `tools/userdb`).
5. Shell prompt shows the POSIX cwd (e.g. `uBixCube@/bin/#`).

**Key lessons learned**:
- Use `x86_64-elf-gcc -m32` — the `i386-elf-gcc` Homebrew formula is unmaintained.
- All code must be compiled with `-mno-sse -mno-sse2 -mno-mmx -mno-3dnow`. GCC can silently emit XMM instructions for struct copies which trigger `#UD` fault 6.
- `kprintf` outputs to both VGA and COM1 serial. Run `bmake run` and check `serial.log` for kernel debug output.
- The FAT library treats the partition-relative sector 0 as BPB; `hdRead` adds `parOffset` (LBA 2048) transparently — do not double-add the offset.
- `/etc/userdb` must exist on the image for `login` to authenticate. `tools/mkimage.sh` copies `tools/userdb` there automatically.
- TCC-compiled binaries require R_386_GOT32X relocation support (patched in `contrib/tcc/tccelf.c`).
- **MMIO pages in the VMM**: Physical addresses at or above `numPages * PAGE_SIZE` (≥ 256 MB with the default QEMU `-m 256` config) are MMIO — framebuffer, PCI BARs, etc. These frames have no entry in `vmmMemoryMap`. Three rules follow from this:
  1. `copyvirtualspace.c` — when COW-marking pages during `fork`, check `(phys >> 12) >= numPages` and share MMIO PTEs as-is without touching the COW counter.
  2. `vmm_cleanVirtualSpace` (paging.c) — when freeing the user address space during `execve`, skip `freePage` for MMIO pages (`(phys >> 12) >= numPages`); just clear the PTE.
  3. `freePage` (vmm_memory.c) — has an explicit bounds check; returns `-1` for out-of-range frame indices as a safety net for any caller that passes an MMIO address.
  Failure to guard these paths causes `freePage` to compute a `vmmMemoryMap` index in the billions, accesses memory at `0xC0800000 + huge_offset` which is unmapped, and triple-faults the kernel.
- **Kernel PD desync after fork**: `vmm_copyVirtualSpace` re-syncs kernel PD entries (indices 770–1015) from the parent **after** all `vmm_getFreeKernelPage`/`vmm_getFreePage` allocations, just before filling the PT_BASE_ADDR self-map page. This is critical — those allocation calls may expand the parent's kernel page directory into new PD indices; copying the entries early and then allocating more leaves the child with stale (zero) PD entries for newly-expanded kernel ranges.
- **Kernel load address and the 640 KB hole**: The kernel was moved from `0x20000` to `0x300000`. At `0x20000` the kernel BSS (text+data+bss ≈ 600 KB) extended past `0xA0000` into the VGA/BIOS hole where there is no RAM — lwIP's static heap and pool arrays landed in VGA address space, causing `mem_free: legal memory` assertion failures. Loading at `0x300000` puts the kernel above the page bitmap (`0x101000–0x201FFF`) and well within the identity-mapped first 4 MB. The identity map was extended from 256 to 1024 PT entries (full 4 MB) to cover the new load address.
- **lwIP memory sizing**: `MEM_SIZE` (the `mem_malloc` heap) = 32768 bytes; `PBUF_POOL_SIZE` = 64 pbufs at 1514 bytes each. `TCP_MSS` = 1460, `TCP_WND`/`TCP_SND_BUF` = 8×MSS. `MEMP_NUM_TCP_SEG` = 64. `MEM_ALIGNMENT` = 1 (lwIP default). Do not reduce these to paper over a kernel-size issue; fix the load address instead.
- **procfs `off_t` ABI**: `vfsRead` function pointer declares `off_t` (64-bit on i386) for the offset parameter. Any `procfs_read` implementation must also use `off_t` for offset — using `long` (32-bit) misaligns the `size` argument on the stack so every read returns 0 bytes.
- **`sprintf` null-terminator**: `kvprintf` (the internal formatting engine) never writes `'\0'`. `sprintf` must append `buf[i] = '\0'` after calling `kvprintf`. Without this, single-digit procfs entries read as two digits.
- **Task naming**: `kTask_t` has `name[256]` and `cmdline[1024]`. All task-creation paths (`execFile`, `sys_exec`, `execThread`) populate these. `execThread` takes a `const char *name` parameter. Early kernel threads (PID 1–4) are named explicitly in `sched_init` and `main.c`.
- **Socket syscalls**: `connect` (98), `bind` (104), `listen` (106), `accept` (30) are wired in `syscalls_posix.c`. Implementations in `sys/net/net/sys_arch.c` use `posix_to_lwip_addr`/`lwip_to_posix_addr` to convert between Linux-style sockaddr (no `sin_len`) and BSD-style (has `sin_len` at byte 0). Userland data must be copied to kernel buffers before passing to lwIP because tcpip_thread has no user mappings.
