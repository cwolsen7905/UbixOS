# UbixOS Documentation

## Architecture

| Document | Description |
|----------|-------------|
| [architecture/vmm.md](architecture/vmm.md) | Virtual Memory Manager behaviour: functions, COW, MMIO guards, fork PD re-sync |
| [architecture/task-switching.md](architecture/task-switching.md) | Software context switching (`switch_to`/`cpu_switch`), the single kernel TSS, the segment-save fix, fork, FPU, v86 |
| [architecture/i386-page-directory-map.md](architecture/i386-page-directory-map.md) | Canonical memory map: PDEs 0–1023, physical low memory, GDT selectors, cleanup/multi-arch notes |
| [architecture/mpi.md](architecture/mpi.md) | Message Passing Interface: mailboxes, syscalls, known bugs, limitations |
| [architecture/syscalls.md](architecture/syscalls.md) | Dual syscall table design: int $0x80 (POSIX) vs int $0x81 (native) |
| [architecture/vfs.md](architecture/vfs.md) | VFS layer, POSIX paths/mountpoints (`/`, `/dev`, `/proc`), cwd, filesystem drivers |

## Design Specs

Forward-looking plans live in [design/](design/) (e.g. `software-task-switch-plan.md`,
`smp-plan.md`, `cross-arch-plan.md`, `scheduler-plan.md`).

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
| [../SYSCALLS.md](../SYSCALLS.md) | Syscall tables and ABI reference |
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
