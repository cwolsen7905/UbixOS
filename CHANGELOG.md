# Changelog

All notable changes to UbixOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `kprint_len(char *, size_t)` — kernel print function that writes up to a specified number of characters to the display.
- `ARCHITECTURE.md` — technical documentation covering kernel subsystems, memory layout, boot sequence, and design decisions.
- `BUILDING.md` — detailed build guide covering toolchain requirements, make targets, VirtualBox VM workflow, and troubleshooting.
- `CHANGELOG.md` — this file; project now tracks changes under Keep a Changelog format with semantic versioning.

### Changed
- `README.md` — rewritten with feature overview, quick-start build instructions, directory table, and documentation index.
- `doc/vmm.txt` — expanded with clearer memory layout diagram, complete function descriptions, and a page fault handling section.
- `doc/UbixOS_Build.txt` — condensed to a VM workflow summary; full build details moved to `BUILDING.md`.
- Cleaned up extraneous output from several kernel functions.

---

## [1.1.0-CURRENT] - 2018-11-07

Active development branch following the 1.24 release. New ABI, expanded POSIX syscall coverage, and continued filesystem work.

### Added
- `fcntl.h` and `fcntl` syscall implementation (`F_DUPFD`, `F_GETFD`, `F_SETFD`, `F_GETFL`, `F_SETFL`, and cmd 17).
- `dup2` syscall.
- `fstat` support for FAT (DOS) filesystem entries including file size reporting.
- `off_t` type defined as 64-bit (`int64_t`).
- `%i` format specifier added to `kprintf`.
- Shutdown routine with filesystem cleanup on halt.
- Doxygen documentation pass over kernel source.
- New kernel headers (`_null.h`, `socket.h`, others).
- `sys_open` and `openat` syscall stubs.
- `getlogin` / `setlogin` syscalls.
- `getrlimit` / `setrlimit` syscalls.
- `setgsbase` for `%gs` thread-local storage base setup.
- `readlink` syscall stub.
- All POSIX syscall numbers assigned.
- Trie-based `sysctl` implementation (`kern_sysctl`).
- Pipe file descriptor duplication on `fork`.

### Changed
- New syscall ABI (`1.1-CURRENT`); syscall dispatch table restructured and split into POSIX (`systemCalls_posix`) and native tables.
- `fseek` renamed to `kern_fseek` throughout the kernel to distinguish from the userland version; parameter type changed from `long` to `u_int32_t`.
- `setguid` corrected to syscall number 181 (was incorrectly mapped to 34).
- VFS layer updated: improved offset tracking, `close` now notifies the filesystem driver to flush/sync.
- FAT driver functions updated and improved.
- Increased maximum open file descriptors per process.
- `argv[0]` set correctly to the program name on `execve`.

### Fixed
- `errno` not propagating correctly to userland.
- `fdestroy` cleanup path corrected.
- Compiler warnings cleaned up across multiple source files.

---

## [1.24.0] - 2018-01-25

First tracked release commit. Brought up networking, graphics, dynamic linking, and a working userland shell.

### Added
- BMP image loading support in the graphics subsystem (credit: flameshadow).
- `objGFX` pixel conversion (`PixConv`) tested and working; `ogImage` functional.
- Software Display Environment (SDE) foundation (`sde.cc`, `objgfx40`).
- `libedit` (BSD editline) integrated into userland for readline-style input.
- BSD-derived shell (`bin/sh`) with initial built-in commands.
- Runtime dynamic linker (RTLD / `ld.so`) — new implementation replacing earlier stub.
- Thread Local Storage (TLS) support for i386 (`%gs`-based).
- `sys_mmap` `MAP_ANONYMOUS` support via `vmm_freeVirtualPage`.
- `lstat` and `stat` syscalls.
- `select()` syscall (initial implementation, noted as rough).
- `sendto` networking syscall.
- `ARGV` and `ENVP` passing through `execve`.
- LDT (Local Descriptor Table) entry in the GDT.
- PID groups / process group support.
- `vmm_getRealAddr(uint32_t)` — returns the physical address for a given virtual address.
- `vmm_allocPageTable` — allocates a new page table entry; assumes caller holds the memory map lock.
- lwIP 2.0.3 TCP/IP stack added to `contrib/` and integrated with the kernel network layer.
- Lance (PCNET / LNC) NIC driver working; interrupt handling and packet send confirmed.
- `sys_arch` layer for lwIP OS integration (mutexes, semaphores, mailboxes).
- C startup unit (`csu/crt1.c`) fixed; `environ` and `__progname` now set correctly.
- Tiny C Compiler (TCC) added to `contrib/` and `bin/`.
- `sendto` and initial socket layer wired up.

### Changed
- `uIntX` typedefs replaced with standard `uintX_t` throughout the codebase.
- ELF loader consolidated — multiple redundant loading paths merged into one.
- VMM performance improvements; page table allocation path sped up.
- New kernel stack layout.
- Kernel make system cleaned up and reorganized; architecture files relocated to `sys/arch/i386/`.
- PCI subsystem code cleaned up.
- UFS superblock and IDE sector-count bug investigated and partially fixed.
- Atomic locking and spinlocks corrected; race condition in the scheduler fixed.
- `init` improved: now correctly spawns login and manages child process lifecycle.

### Fixed
- `crt1.c` hack removed; `environ` no longer set to `0x0` by default.
- Race condition in process scheduling resolved.
- Spinlock ordering corrected.
- LNC driver interrupt path debugged and confirmed working.

---

## [0.1.0] - 2017-11-15

Initial git import from prior CVS/SVN history. Kernel booted, basic VFS and VMM in place.

### Added
- i386 kernel with protected mode, GDT, IDT, paging, and a basic scheduler.
- Virtual Memory Manager (VMM) with copy-on-write page fault handling.
- Virtual Filesystem layer (VFS) with UbixFS and UFS driver stubs.
- ISA device drivers: PIC (i8259), PIT, AT keyboard, floppy, NE2000 NIC, RS-232 serial, mouse.
- PCI bus enumeration and IDE hard disk driver.
- MPI (Message Passing Interface) for inter-process communication.
- Pipes and semaphores.
- ELF binary loader (early version).
- FreeBSD-derived libc, libstdc++, and C++ runtime.
- `bin/init`, `bin/login`, `bin/sh` (early versions).
- NE2000 Ethernet driver cleanup.
- `lseek` syscall (`SEEK_END` not yet implemented).
- TCC added to base system.

[Unreleased]: https://github.com/cwolsen7905/UbixOS/compare/acb8ba9a...HEAD
[1.1.0-CURRENT]: https://github.com/cwolsen7905/UbixOS/compare/30af09b3...acb8ba9a
[1.24.0]: https://github.com/cwolsen7905/UbixOS/compare/6e02e5b2...30af09b3
[0.1.0]: https://github.com/cwolsen7905/UbixOS/releases/tag/6e02e5b2
