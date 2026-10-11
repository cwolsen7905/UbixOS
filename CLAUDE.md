# CLAUDE.md

Guidance for Claude Code (and any other AI agent or human) working in this
repository. This file is an **index**: facts about the system live here in
brief; the *rules* live in [`docs/standards/`](docs/standards/README.md) and
are enforced by `tools/check/`. Read the standards that touch your task before
you edit.

## What This Is

uBixOS is a hobby/research operating system in C, C++ and assembly, developed
since 2002. **master is 64-bit only: AArch64 (arm64) is the default and primary
forward target (QEMU `virt`, heading for Raspberry Pi / Orange Pi); x86_64 is
the second supported architecture and the abstraction anchor** — the diverse
reference that keeps the machine-independent / machine-dependent split honest.
**32-bit i386 is frozen**: it is not maintained on master and lives only in
the **`releng/2`** branch (fixes go there; do not spend effort keeping i386
green here).

**Product identity:** *console-first, graphical-optional, one tree,
profile-driven, MMU-class only.* The kernel and base system are
display-agnostic; the graphical stack (`views` compositor + `objGFX`) is a
userland layer, not the foundation. MCU-class (no-MMU) and locked phone
hardware are out of scope. See `docs/design/console-and-arch-convergence-plan.md`
and `docs/design/cross-arch-plan.md`.

## The Rules (read these)

| Topic | Document |
|-------|----------|
| Index, precedence, legacy/ratchet policy | [docs/standards/README.md](docs/standards/README.md) |
| C/C++/asm language rules, banned APIs, errors, docs | [docs/standards/coding.md](docs/standards/coding.md) |
| Kernel rules: locking, allocation, user pointers, stacks, MMIO, no FP | [docs/standards/kernel-rules.md](docs/standards/kernel-rules.md) |
| Native ABI + FreeBSD-compatible POSIX layer; adding a syscall | [docs/standards/abi-and-syscalls.md](docs/standards/abi-and-syscalls.md) |
| aarch64 + x86_64 parity; MI/MD split | [docs/standards/arch-parity.md](docs/standards/arch-parity.md) |
| Review checklist | [docs/standards/review-checklist.md](docs/standards/review-checklist.md) |
| Definition of done | [docs/standards/definition-of-done.md](docs/standards/definition-of-done.md) |
| Naming, headers, formatting detail | [STYLE.md](STYLE.md) |
| Contribution workflow, doc-sync table | [CONTRIBUTING.md](CONTRIBUTING.md) |
| Source tree + subsystem architecture | [ARCHITECTURE.md](ARCHITECTURE.md), `docs/architecture/` |

**Non-negotiables (details in the documents above):**

1. Keep **both** 64-bit arches green: build and boot aarch64 *and* x86_64
   before calling anything done. An arch-specific fix is checked on the other
   arch (P-1, P-4).
2. Every user pointer/length is untrusted: validate and copy it (K-40..K-44).
3. Every new function has a Doxygen block; every touched file is
   `clang-format`ted; new file-scope data is `static`, `g_`-prefixed and at the
   top of the file (C-15, C-40).
4. No FP/SIMD, VLAs, recursion or large stack buffers in the kernel
   (K-1, K-10). Never `-Wno-*` a warning away.
5. Syscalls: native ABI first; FreeBSD slots are fixed and never squatted;
   errors are negative errno (A-1..A-12).
6. Run `tools/check/run.sh` (or `tools/mcr.sh`) before committing. Do not
   commit until the person confirms the change works. Never push to master.

## Build System

The project uses **BSD make (`bmake`)** — GNU make will not work (`.CURDIR`,
`.if`, `!=`). Build output is arch-homed under `build/${ARCH}/`; select the
target with `TARGET=` (alias for `TARGET_ARCH=`). **Default is aarch64.**

```sh
bmake kernel world TARGET=aarch64   # → build/aarch64/   (default arch)
bmake kernel world TARGET=x86_64    # → build/x86_64/    (keep green too)
bmake kernel                         # kernel only → build/aarch64/boot/kernel
bmake world                          # userland only
bmake image                          # fresh bootable image via tools/mkimage.sh
bmake run-aarch64                    # QEMU virt (graphical, HVF on Apple Silicon)
bmake run-debug-aarch64              # headless, serial only
bmake run-x86_64 TARGET=x86_64       # QEMU std-VGA desktop (TCG on Apple Silicon)
bmake run-debug-x86_64 TARGET=x86_64 # headless x86_64
bmake clean
```

- **Concurrent sessions:** whole-tree builds from two sessions corrupt shared
  `contrib/` state. Serialise with
  `tools/buildlock.sh with "<who>" bmake world image` (see `AGENTS-COORD.md` when present).
  Per-directory `bmake -C <dir>` builds don't need the lock.
- **Board builds** (e.g. Raspberry Pi 3): the world is built once per arch and
  shared; only the kernel/image are board-specific —
  `bmake kernel-rpi3`, `bmake image-rpi3` (output under
  `build/aarch64/boards/rpi3/`). See `docs/design/raspberry-pi-3b-bringup.md`.
- **Image workflow:** `bmake image` builds a clean image; `bmake stage-src`
  (after `image`) stages the FreeBSD-style `/usr/src` → `/usr/obj` tree for
  on-device builds. Prefer `bmake image` over `install-world` (slow rsync).
  See `docs/design/usr-src-build-layout-plan.md`.
- **macOS prerequisites:** `brew install bmake qemu mtools aarch64-elf-gcc
  x86_64-elf-gcc`; NetSurf additionally needs `bison` ≥ 3 and `libpng`
  (`tools/build-netsurf.sh` finds the keg-only bison).
- **Dependency tracking:** `-MMD -MP`; editing a header recompiles dependents.

See `BUILDING.md` for the long form.

### Compile flags that matter

Kernel objects are freestanding (`-ffreestanding -nostdinc -fno-builtin
-fno-pic`), include path `sys/include` (+ `sys/arch/<arch>`) only, and carry
the per-arch ISA flags from `share/mk/ubix.target.<arch>.mk`: x86_64
`-mno-sse -mno-sse2 -mno-mmx -mno-3dnow -mno-red-zone -mcmodel=large`;
aarch64 `-march=armv8-a -mgeneral-regs-only`. A narrow set of bug classes is
promoted to errors (`WERROR_BUGCLASSES` in the top `Makefile`:
implicit-function-declaration, int↔pointer casts, return-type).

## Source Tree

Two independent halves: **kernel** (`sys/`) and **world** (userland). Programs
live where the FreeBSD/macOS hierarchy puts them; a program's tree decides its
install path (`Makefile.incl` sets `BINDIR`).

| Tree | Install path | Holds |
|------|--------------|-------|
| `sys/` | — | Kernel. `sys/arch/{aarch64,x86_64}` = machine-dependent minimum (`kern/ vmm/ dev/ lib/ net/ board/ bringup/`); everything else is machine-independent |
| `bin/` | `/bin` | `init` (PID 1), `login`, `shell`, `tcsh`, core commands |
| `sbin/` | `/sbin` | Disk/FS/system admin |
| `usr.bin/` | `/usr/bin` | User commands + GUI apps |
| `usr.sbin/` | `/usr/sbin` | Daemons: `views` (compositor), `vlogin`, `authd`, `logd`, … |
| `tests/` | `/usr/tests` | Test/dev harnesses |
| `lib/`, `libexec/` | `/lib`, … | libc (musl + FreeBSD-derived), `ubix`, `ubix_api`, `objgfx`; musl's runtime linker |
| `contrib/` | — | Third-party (lwIP, jemalloc, musl, BearSSL, TCC, NetSurf, …). **Not modified**, not checked |

`/bin/init`, `/bin/sh` and `/lib` (`PT_INTERP`) are hard-coded. When a program
exec'd by absolute path moves, update `etc/init.d/*`, `tools/makereg.c` seeds
and any `"/bin/…"` literal (e.g. `CONSOLE_DESKTOP` in `bin/init/main.c`).

**Kernel headers** live in `sys/include/` — the only include path for kernel
code; userland headers (`include/`) are never visible to it. Subsystem map:
`sys/arch/<arch>/{boot,kern}` (bootstrap, entry), `sys/kern` (core, scheduler, exec),
`sys/posix` (POSIX syscall layer, signals, tty, pipes), `sys/vmm`, `sys/fs/*`
(VFS + drivers), `sys/mpi`, `sys/net` (lwIP), `sys/dev`, `sys/usb`, `sys/lib`
(`kprintf`, `kmalloc`, string). Details: `ARCHITECTURE.md`.

### Display stack

| Layer | Component | Role |
|-------|-----------|------|
| Compositor | `usr.sbin/views/` (C++) | Sole owner of the framebuffer (`sys_mapfb()`); windows, decorations, Z-order; composites shared-memory buffers |
| App rendering | `lib/objgfx/` (C++) | `ogSurface`, `ogScalableFont` (TrueType; `ogBitFont` is legacy) |

Only `views` maps the framebuffer; apps draw into their `vmm_share_region`
buffer with objGFX; MPI carries signals (`DISPLAY_CLAIM/FLIP/RELEASE`), never
pixels. Headers: `include/objgfx/`, `include/views/display_proto.h`. Design:
`docs/design/display-plan.md`. Refresh `docs/apps/*` when `display_proto`,
objGFX or `hello` change.

## Architectural Constraints (summary)

- **Syscalls:** two shared tables — native (`sys/kern/syscalls.c`) and the
  FreeBSD-numbered POSIX table (`sys/posix/syscalls_posix.c`). Per-arch entry
  code only decodes registers. Rules: [abi-and-syscalls.md](docs/standards/abi-and-syscalls.md).
- **VFS:** fully POSIX paths (`/`, `/dev`, `/proc`); `_current->oInfo.cwd` is a
  plain POSIX path. All filesystem calls go through `sys/fs/vfs/`; never call
  a driver directly from outside it.
- **IPC:** custom MPI mailboxes (not SysV IPC); pipes and semaphores exist too.
- **Memory:** `kmalloc`/`kfree` in the kernel; the libc allocator in userland.
- **Dynamic linking:** musl's `/lib/ld-musl-${ARCH}.so.1`.

## Code Style and Tooling

Coding style is FreeBSD `style(9)`: 8-column hard tabs, Allman braces,
**120 columns** (`.clang-format` is authoritative), `int *foo`. Naming and
header-guard rules: [STYLE.md](STYLE.md). Apply formatting **per file as you
touch it** — never reformat the whole tree (it breaks `git blame`).

Every new function or method needs a Doxygen block:

```c
/**
 * Brief one-line description.
 *
 * Longer explanation only if constraints/locking/side-effects are non-obvious.
 *
 * @param foo  Only when range, ownership, or direction is non-obvious.
 * @return 0 on success, -errno on failure.  (omit for void)
 */
```

File-scope variables not in the public API are `static`; all `static` data
sits at the top of the file after the `#include`s; dead file-scope variables
are removed. Identifiers: functions/variables `lower_case`, globals/statics
`g_` prefix, typedefs `_t`, enum values `UPPER_CASE`, macros `UPPER_CASE`.
Update the copyright end year when touching a file.

### The gate suite

```sh
tools/check/run.sh                 # fast gates on files changed vs master
tools/check/run.sh --tier ci       # + header/warnings/tidy/cppcheck/build/boot
tools/check/run.sh --all           # whole tree, baselined
tools/check/run.sh --list          # what exists and which tier
tools/check/selftest.sh            # prove the gates catch what they claim
tools/mcr.sh [--fix] [file...]     # thin front end for format+gates
```

Gates (`format`, `banned-api`, `doxygen-docs`, `file-scope-static`,
`syscall-table`, `header-selfcontained`, `warnings`, `clang-tidy`, `cppcheck`,
`build`, `boot-smoke`) judge **added lines** or ratchet against
`tools/check/baselines/`; legacy findings do not block, new ones do. CI
(`.github/workflows/ci.yml`) runs the same entry point. Never raise a baseline
to get green.

## IntelliSense

`python3 tools/gen-compile-commands.py --world` regenerates the (gitignored)
`compile_commands.json`; regenerate after major Makefile or tree changes.
`bear` does not work reliably on macOS with SIP.

## Versioning

Single source of truth: `sys/include/ubixos/version.h`. To release: edit
`UBIXOS_VERSION_{MAJOR,MINOR,PATCH,TAG}`, roll `CHANGELOG.md` `[Unreleased]` →
`[X.Y.Z-TAG] - YYYY-MM-DD` (fresh empty `[Unreleased]` + footer links),
`bmake kernel world image`, commit, `git tag -a vX.Y.Z-TAG`. Full checklist:
`CONTRIBUTING.md`.

## Current State

- **aarch64 (primary):** boots on QEMU `virt` (HVF on Apple Silicon) to a full
  graphical desktop — kernel → virtio-blk FAT32 root → `init` → `views` →
  `vlogin` → themed desktop; GUI terminal running tcsh with POSIX signals and
  tty job control; sockets/lwIP (DHCP); mmap/fork/execve; musl dynamic linking;
  vDoom. Cooperative scheduling, EL0 via a 100 Hz timer, LP64, musl emits
  FreeBSD syscall numbers. **Still bring-up:** a hand-rolled syscall `switch`
  ahead of the shared table and `sys/arch/aarch64/bringup/` (demos + the real
  exec path). The convergence backlog is in
  `docs/design/console-and-arch-convergence-plan.md`. HVF
  "Assertion failed: (isv)" means a real bad-pointer store — rerun with
  `-accel tcg` to get the faulting address.
- **x86_64:** second supported arch; builds with `bmake kernel world
  TARGET=x86_64`, boots under QEMU via `run-x86_64`/`run-debug-x86_64`
  (`-kernel` injection + virtio-blk). Keep it green.
- **i386:** frozen on `releng/2`. Historical lessons are preserved in
  [docs/standards/legacy-i386-lessons.md](docs/standards/legacy-i386-lessons.md).
- Default credentials in a built image: `root` / `user` (from `tools/userdb`,
  copied to `/etc/userdb` by `tools/mkimage.sh`).
