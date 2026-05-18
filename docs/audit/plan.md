# UbixOS Kernel Audit Plan

## Motivation

The UbixOS kernel has been developed since 2002. While many subsystems have been
significantly improved, the codebase contains legacy code paths, silent error conditions,
and latent bugs that don't surface under normal operation but can cause instability,
corruption, or security issues.

This audit focuses on the original kernel code (excluding upstream third-party code such
as lwIP `sys/net/core/`, `sys/net/api/`). The goal is systematic, subsystem-by-subsystem
review producing actionable findings with file:line references.

## Scope

| Subsystem | Files | Findings |
|-----------|-------|----------|
| VMM | `sys/vmm/` | [findings/vmm.md](findings/vmm.md) |
| Scheduler / Exec | `sys/kernel/sched.c`, `sys/arch/i386/i386_exec.c`, `sys/kernel/endtask.c` | [findings/scheduler.md](findings/scheduler.md) |
| VFS / Syscalls | `sys/kernel/vfs_calls.c`, `sys/kernel/descrip.c`, `sys/fs/vfs/` | [findings/vfs.md](findings/vfs.md) |
| FAT Driver | `sys/fs/fat/` | [findings/fat.md](findings/fat.md) |
| IPC / Pipes / Semaphores | `sys/kernel/sem.c`, `sys/kernel/pipe.c`, `sys/mpi/` | [findings/ipc.md](findings/ipc.md) |
| ISA Drivers | `sys/isa/` | [findings/isa.md](findings/isa.md) |
| PCI / e1000 / IDE | `sys/pci/` | [findings/pci.md](findings/pci.md) |
| TTY | `sys/kernel/tty.c`, `sys/kernel/ttydev.c` | [findings/tty.md](findings/tty.md) |
| Kernel Lib | `sys/lib/` | [findings/lib.md](findings/lib.md) |
| Networking Bridge | `sys/net/net/`, `sys/net/netif/` | [findings/net-bridge.md](findings/net-bridge.md) |

## What Reviewers Look For

- **Null pointer dereference** — unchecked return values from `kmalloc`, `getfd`, VFS ops
- **Buffer overflow / overread** — fixed-size kernel buffers, string ops without bounds
- **Integer overflow / truncation** — address arithmetic using `uint32_t`, size calculations
- **Use-after-free** — task/fd/file structs freed while still referenced
- **Double-free** — error paths that free the same allocation twice
- **Uninitialized memory** — structs used before all fields are set
- **Race conditions** — shared state accessed without locks in ISR-reachable paths
- **Missing bounds checks** — array indices derived from user input or hardware data
- **Ignored error returns** — functions that can fail silently
- **Dead / unreachable code** — leftover stubs and `#ifdef __IGNORE` blocks that rot

## Severity Levels

| Level | Meaning |
|-------|---------|
| 🔴 Critical | Can cause kernel panic, memory corruption, or security breach |
| 🟠 High | Likely causes incorrect behavior or instability under normal use |
| 🟡 Medium | Silent failure, degraded behavior, or latent crash under stress |
| 🔵 Low | Code quality, dead code, or minor logic issue with no immediate impact |

## Status

Last updated: 2026-05-18

| Subsystem | Status | Finding Count |
|-----------|--------|---------------|
| VMM | ⬜ Pending | — |
| Scheduler / Exec | ⬜ Pending | — |
| VFS / Syscalls | ⬜ Pending | — |
| FAT Driver | ⬜ Pending | — |
| IPC / Pipes / Semaphores | ⬜ Pending | — |
| ISA Drivers | ⬜ Pending | — |
| PCI / e1000 / IDE | ⬜ Pending | — |
| TTY | ⬜ Pending | — |
| Kernel Lib | ⬜ Pending | — |
| Networking Bridge | ⬜ Pending | — |
