# UbixOS Documentation

## Engineering Standards

The rules every contributor — human or AI agent — follows. Start at
[standards/README.md](standards/README.md). The automated gates are in
`tools/check/` (`tools/check/run.sh --list`) and `.github/workflows/ci.yml`.

| Document | Description |
|----------|-------------|
| [standards/coding.md](standards/coding.md) | Language rules: style(9), CERT-C subset, banned APIs, integers, errors, docs |
| [standards/kernel-rules.md](standards/kernel-rules.md) | Locking, allocation, user pointers, stacks, MMIO, no FP/SIMD |
| [standards/abi-and-syscalls.md](standards/abi-and-syscalls.md) | Native ABI, FreeBSD-compatible POSIX layer, adding a syscall |
| [standards/arch-parity.md](standards/arch-parity.md) | aarch64 + x86_64 parity, MI/MD split |
| [standards/review-checklist.md](standards/review-checklist.md) | Review checklist (including the "slop" check for generated code) |
| [standards/definition-of-done.md](standards/definition-of-done.md) | When a change is finished |
| [standards/legacy-i386-lessons.md](standards/legacy-i386-lessons.md) | Lessons carried over from the frozen i386 port |

## Architecture

> The i386-era pages here (`i386-page-directory-map.md`, the `int $0x80/0x81`
> framing in `syscalls.md`) describe the frozen 32-bit kernel on `releng/2`;
> per-arch 64-bit equivalents are tracked in `design/cross-arch-plan.md`.

| Document | Description |
|----------|-------------|
| [architecture/vmm.md](architecture/vmm.md) | Virtual Memory Manager behaviour: functions, COW, MMIO guards, fork PD re-sync |
| [architecture/task-switching.md](architecture/task-switching.md) | Software context switching (`switch_to`/`cpu_switch`), the single kernel TSS, the segment-save fix, fork, FPU, v86 |
| [architecture/i386-page-directory-map.md](architecture/i386-page-directory-map.md) | **[i386, legacy]** Canonical memory map: PDEs 0–1023, physical low memory, GDT selectors, cleanup/multi-arch notes |
| [architecture/mpi.md](architecture/mpi.md) | Message Passing Interface: mailboxes, syscalls, known bugs, limitations |
| [architecture/syscalls.md](architecture/syscalls.md) | Dual syscall table design: POSIX (FreeBSD-numbered) vs native (written for i386 `int $0x80`/`$0x81`; see standards/abi-and-syscalls.md for the current rules) |
| [architecture/vfs.md](architecture/vfs.md) | VFS layer, POSIX paths/mountpoints (`/`, `/dev`, `/proc`), cwd, filesystem drivers |

## Design Specs

Forward-looking plans live in [design/](design/) (e.g. `cross-arch-plan.md`,
`smp-plan.md`, `threading-plan.md`); finished plans are archived in
[design/completed/](design/completed/).

| Document | Description |
|----------|-------------|
| [design/fbcon.md](design/fbcon.md) | VESA framebuffer console spec (draft) |

## Audit

| Document | Description |
|----------|-------------|
| [audit/plan.md](audit/plan.md) | Subsystem-by-subsystem kernel audit plan + findings index |
| [audit/vmm-audit.md](audit/vmm-audit.md) | VMM technical audit (dated snapshot: bootstrap, fork phases, COW, teardown) |

## Driver Development

| Document | Description |
|----------|-------------|
| [drivers/writing-a-driver.md](drivers/writing-a-driver.md) | How to write a UbixOS device driver |

## Root-Level Reference

| Document | Description |
|----------|-------------|
| [../README.md](../README.md) | Project overview and quick start |
| [../BUILDING.md](../BUILDING.md) | Build instructions (macOS and FreeBSD) |
| [../ARCHITECTURE.md](../ARCHITECTURE.md) | High-level source tree architecture |
| [../SYSCALLS.md](../SYSCALLS.md) | Syscall tables and ABI reference *(file absent on master — the tables in `sys/kern/syscalls.c` and `sys/posix/syscalls_posix.c` are the reference)* |
| [../DEBUG.md](../DEBUG.md) | Debugging under QEMU / GDB |
| [../BUGS.md](../BUGS.md) | Known bugs and workarounds |
| [../TODO.md](../TODO.md) | Work items and future plans |
| [../CHANGELOG.md](../CHANGELOG.md) | Release history |

## Reference

| Document | Description |
|----------|-------------|
| [reference/external-specs.md](reference/external-specs.md) | Links to ELF, Intel SDM, Multiboot, and FAT specifications |

## Assets

| File | Notes |
|------|-------|
| [`sys/sde/assets/ubix.bmp`](../sys/sde/assets/ubix.bmp) | Background bitmap for the graphical framebuffer console / SDE |
