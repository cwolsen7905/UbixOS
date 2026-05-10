# Changelog

All notable changes to UbixOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `kprint_len(char *, size_t)` — kernel print function that writes up to a specified number of characters to the display.
- `sys/fs/fat/Makefile` — FAT filesystem driver now has its own build file (was missing, causing fat objects to be excluded from the kernel link).
- `sys/lib/kern_trie.c` now included in the lib build (`kern_trie.o` added to `sys/lib/Makefile`) so sysctl trie operations link correctly.
- Sized C++ delete operators (`operator delete(void*, unsigned int)` / `operator delete[](void*, unsigned int)`) added to `sys/lib/libcpp.cc` for GCC 14+ compatibility.

### Fixed
- **macOS world build (`feature/macos-build-qemu`)**: resolved all compile and link errors blocking `bmake world` (userland) with the `x86_64-elf-gcc` cross-compiler.
  - **Cross-compiler propagation**: top-level `Makefile` now includes `Makefile.incl` so `CROSS_PREFIX` and toolchain overrides reach all world sub-makes.
  - **`MAKESYSPATH` propagation**: changed assignment to `?=` with `.export` so bmake's include path is inherited by recursive sub-makes without being overwritten.
  - **ELF architecture mismatch**: added `LDFLAGS = -Wl,-m,elf_i386` to `bin/Makefile.incl`; the baremetal cross-linker does not auto-select `elf_i386` for static links. Added `$(LDFLAGS)` to link commands in all active `bin/*/Makefile` files.
  - **`libc_old.so` as link input**: the baremetal `x86_64-elf` toolchain cannot produce ET_DYN shared objects; `libc_old.so` was being built as ET_EXEC. Fixed `bin/clock`, `bin/cp`, `bin/disklabel`, and `bin/fdisk` Makefiles to link against `../../lib/libc_old/*/*.o` instead.
  - **`-Wl,-m,elf_i386` in shared library links**: added to `lib/ubix_api/Makefile`, `lib/libc_old/Makefile`, and `libexec/ld/Makefile` so those shared objects use the correct 32-bit linker emulation.
  - **`elf_i386_fbsd` linker emulation**: cross-linker only supports `elf_i386`; fixed in `libexec/ld/Makefile`.
  - **`__progname` multiply defined**: made definition in `lib/libc_old/gen/setprogname.c` weak so it doesn't conflict with the strong definition in `lib/ubix/sstart.c`.
  - **`vfprintf` buffer pointer**: fixed `vsprintf(&data, ...)` → `vsprintf(data, ...)` in `lib/libc_old/stdio/vfprintf.c`.
  - **`malloc.c` missing `memset`**: added `#include <string.h>` to `lib/libc_old/stdlib/malloc.c`.
  - **`sstart.c` implicit declarations**: added `extern int main(int, char **, char **); extern void exit(int);` forward declarations.
  - **`getPage` undeclared**: added `void *getPage(int pages, int flags);` to `libexec/ld/ld.h`.
  - **Bare `make` in sub-makes**: replaced all `;make)` with `;$(MAKE))` in `lib/Makefile`, `lib/libc_old/Makefile`, `bin/Makefile`, and `libexec/Makefile`.
  - **`muffin` and `objgfx`**: disabled from world build — require hosted C++ headers (`<functional>`, `<map>`, `<iostream>`) not available in the baremetal toolchain.
  - **`bool` typedef**: guarded in `include_old/sys/types.h` with `__STDC_VERSION__ < 202311L` check to avoid conflict with C23's built-in `bool`.
- **macOS cross-build (`feature/macos-build-qemu`)**: resolved all compile and link errors blocking `bmake kernel` under GCC 16 with `-std=c23` defaults.
  - **C23 / GCC 16 compatibility**: updated `()` function declarations to typed signatures throughout `sys/include/ubixos/syscalls.h`, `sys/include/ubixfs/ubixfs.h`, `sys/include/ufs/ufs.h`, `sys/include/i386/atkbd.h`, `sys/include/isa/atkbd.h`, and `sys/include/ubixos/ld.h`.
  - **`stdatomic.h`**: reordered GCC vs. Clang detection so GCC 16 (which now satisfies `__has_extension(c_atomic)`) correctly uses `__GNUC_ATOMICS` instead of the missing `__c11_atomic_*` builtins.
  - **`ubthread`**: changed `lock` fields from `bool` to `uint32_t` to match `xchg_32` signature; replaced `ATOMIC_VAR_INIT(0)` with plain `= 0`.
  - **`sys/vmm/`**: added missing casts (`(uint32_t *)PD_BASE_ADDR`, `(void *)`, `(uint32_t)`) and missing includes (`string.h`, `kpanic.h`, `endtask.h`, `vmm.h`) across `paging.c`, `unmappage.c`, `vmm_allocpagetable.c`, `vmm_mmap.c`, `getfreevirtualpage.c`, `pagefault.c`.
  - **`sys/kernel/`**: added missing includes and forward declarations in `descrip.c`, `vfs_calls.c`, `gen_calls.c`, `execve.c`, `kern_pipe.c`, `sem.c`, `shutdown.c`, `syscall.c`, `ubthread.c`, `vitals.c`.
  - **`sys/fs/vfs/`**: added missing includes and forward declarations in `mount.c`, `stat.c`, `namei.c`, `inode.c`; renamed `vfsFindFS` → `vfs_findFS` to match header.
  - **`sys/pci/`**: fixed implicit-int `static hdC` in `hd.c`; added missing includes in `pci.c` and `lnc.c`; fixed `vmm_getRealAddr` pointer casts in `lnc.c`.
  - **`sys/net/`**: forward-declared lwIP socket functions in `sys_arch.c` and `descrip.c` instead of including `net/sockets.h` (whose macros redefine `fcntl`/`close`/`ioctl` and break `descrip.h`); added `string.h` to `ethernetif.c`; changed `lnc_netif` and `tmpBuf` in `init.c`/`ethernetif.c` to `extern` (authoritative definitions are in `pci/lnc.c`).
  - **`sys/lib/kern_trie.c`**: fixed recursive call (`deletion` → `delete_trieNode`), replaced `free` with `kfree`, added `haveChildren` forward declaration.
  - **`sys/sde/Makefile`**: changed `make allBuild` → `$(MAKE) allBuild` so bmake is used recursively instead of GNU make.
  - **`sys/Makefile.incl`**: added `-Wno-incompatible-pointer-types` to suppress GCC 16 errors on syscall table function pointer casts (all i386 calling conventions are compatible in practice).
  - **`sys/include/sys/descrip.h`**: fixed `int_kern_openat` typo; corrected `kern_openat` parameter count; added `fdestroy` declaration.
  - **`sys/include/vmm/paging.h`**: corrected stale `vmmClearVirtualPage` → `vmm_clearVirtualPage`.
  - **`sys/arch/i386/`**: added missing includes in `fork.c` and `trap.c`; fixed pointer/integer casts in `i386_exec.c` and `bioscall.c`.
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
