# Changelog

All notable changes to UbixOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `include_old/dirent.h` and `lib/libc_old/dirent/` — userland `opendir`/`readdir`/`closedir` implementation backed by the kernel VFS `sys_opendir`/`sys_readdir`/`sys_closedir` syscalls. Provides the standard `DIR`/`struct dirent` API to dynamically-linked binaries such as `ls`.
- `docs/task-switching.md` — detailed documentation of the hardware TSS-based task switching mechanism, GDT/LDT layout, fork mechanics, FPU lazy save/restore, and a critical review with improvement suggestions.
- `kTask_t.kernelStack` field — stores the base address of each task's dedicated ring-0 kernel stack for future cleanup on task exit.
- `kprint_len(char *, size_t)` — kernel print function that writes up to a specified number of characters to the display.
- `sys/fs/fat/Makefile` — FAT filesystem driver now has its own build file (was missing, causing fat objects to be excluded from the kernel link).
- `sys/lib/kern_trie.c` now included in the lib build (`kern_trie.o` added to `sys/lib/Makefile`) so sysctl trie operations link correctly.
- Sized C++ delete operators (`operator delete(void*, unsigned int)` / `operator delete[](void*, unsigned int)`) added to `sys/lib/libcpp.cc` for GCC 14+ compatibility.

### Fixed
- **Dynamic linker (`ld.so`) — `ls` now runs end-to-end with full lazy PLT resolution**:
  - **`libexec/ld/main.c` — `rel` not persisted across PLT resolution calls**: `rel` (the section-header index of `.rel.plt`) was a local variable inside `ld()`. The section-scanning loop that populates it is guarded by `if (binarySectionHeader == 0x0)` and only runs on the first call. Every subsequent PLT symbol resolution started with `rel=0`, hit the `if (rel == 0) return 0x0` guard, causing `_ld` to jump to address 0 — crash at EIP=0x3. Fixed by promoting `rel` and `relDyn` to `static int` (`binaryRel`/`binaryRelDyn`) so the section index survives across calls.
  - **`sys/kernel/ld.c` — `R_386_JMP_SLOT` used `+=` instead of `=`**: when the kernel loads `ld.so` and applies its relocations, JMP_SLOT entries were computed as `*reMap += LD_START + st_value`. Because the initial GOT slot value is a PLT stub offset (not zero), this added `LD_START` to a small integer and produced a garbage address. Fixed to `*reMap = LD_START + st_value` (absolute assignment).
- **COW / fork-exit lifecycle bugs (BUG-COW-03, BUG-COW-05, BUG-COW-06) and kernel debug cleanup**:
  - **BUG-COW-05** (`page_fault.S`): `_vmm_pageFault` used `call _popFS` after returning from `trap()`. The `call` pushed a 4-byte return address on the stack, shifting the `pop %gs/%fs/%es/%ds; popa` sequence off by one slot — every general-purpose register was misassigned after COW fault handling, corrupting the returning task's state. Fixed by replacing `call _popFS` with `add $0x4,%esp; jmp _popFS`.
  - **BUG-COW-06** (`paging.c`): `vmm_cleanVirtualSpace` zeroed non-COW present PTEs without calling `freePage()`, leaking one physical page per mapped user page on every `exec`. Fixed by replacing the commented-out open-coded free block with `freePage(pageTableSrc[y] & 0xFFFFF000)`.
  - **BUG-COW-03 partial fix** (`endtask.c`): `endTask` now calls `vmm_cleanVirtualSpace(VMM_USER_START)` before `sched_yield()`, while the dying task is still `_current` and `PT_BASE_ADDR` reflects its own page tables. This decrements COW counters for all shared user pages and frees private pages before the scheduler switches away — matching the FreeBSD/Linux approach. Previously, COW counters for parent-owned shared pages were never decremented on child exit.
  - **Removed hardcoded PID-7 spin loop** (`trap.c`): `if (_current->id == 7) while(1) asm("nop");` was left in from development. This froze the entire kernel whenever any process was assigned PID 7.
  - **Silenced expected COW write-fault logging** (`trap.c`): `trap()` printed a `trap _code:` line for every page fault before dispatching to the handler, including the completely normal user-mode COW write faults (ERR=0x7) that occur whenever a forked process writes to a shared page. These are now suppressed; only unexpected faults (kernel-mode, non-write, non-COW) still log.
  - **Removed per-operation debug noise**: removed `kprintf` calls that printed on every exec, page fault, file open, and task exit — `CR2:[...]/CR2-RET` (pagefault.c), `[read:...]/data_addr:/exec done/LDT[1]:` (i386_exec.c), `endTask:N` (endtask.c), `[sched.c:NNN]` (sched.c), `sys_fopen` (file.c). Boot-time device and mount messages are unchanged.
- **Scheduler / fork bugs (BUG-SCHED-01 through BUG-SCHED-07)** — full audit of the task switching and fork paths:
  - **BUG-SCHED-01** (`fork.c`): `newProcess->parent` and `_current->children++` moved to before `newProcess->state = FORK`. Previously the child could run and call `getppid()`/`wait4()` before `parent` was set, causing a NULL dereference.
  - **BUG-SCHED-02** (`fork.c`): fork spin-wait now reads `state` through `volatile kTask_t *` to prevent GCC from caching the value in a register and looping forever.
  - **BUG-SCHED-03** (`sched.c`, `sched.h`, `fork.c`, `i386_exec.c`): each task now gets a dedicated 4096-byte ring-0 kernel stack allocated in `schedNewTask()`. The base is stored in `kTask_t.kernelStack`. Previously all user tasks shared `esp0 = 0xFFFFFFFF`, causing kernel stack corruption whenever two tasks were simultaneously in ring-0 transitions.
  - **BUG-SCHED-04** (`syscall_posix.c`): removed `while(1) kprintf("MFR")` debug block on syscall 89 (`getgroups`) that permanently locked up the kernel.
  - **BUG-SCHED-05** (`i386_exec.c`): ELF magic check changed from `&&` to `||` in both `execFile` and `sys_execve`. The `&&` form only rejected files where all three bytes were wrong; partial magic was silently accepted.
  - **BUG-SCHED-06** (`sched.c`): the `sti` before `ljmp` in `sched()` was load-bearing — removing it caused `ljmp` to save EFLAGS with `IF=0` into the outgoing task's TSS, leaving that task with interrupts permanently disabled on the next schedule (breaking keyboard and timer). Fixed properly by saving `prevTask = _current` before the scheduler update and setting `prevTask->tss.eflags |= 0x200` (IF bit) after `spinUnlock`, before `ljmp`. Outgoing task now resumes with interrupts on, with no `sti` race window.
  - **BUG-SCHED-07** (`timer.S`): added `test %ebx,%ebx; jz done` guard before `div %ebx` in the timer ISR quantum check. A `quantum` value of zero (before `vitals_init()` runs) would cause a `#DE` divide exception inside the IRQ0 handler.
- **COW / fork memory bugs (BUG-COW-01 through BUG-COW-04)** — full audit of copy-on-write fork path:
  - **BUG-COW-01** (`copyvirtualspace.c`): COW PTEs for both kernel and user regions were created with `PAGE_WRITE` set (`PAGE_DEFAULT | PAGE_COW` and `KERNEL_PAGE_DEFAULT | PAGE_COW`). The x86 CPU silently allowed writes to those pages without faulting, so the COW handler in `vmm_pageFault` never fired. Fixed by masking out `PAGE_WRITE` when building the PTE: `(PAGE_DEFAULT & ~PAGE_WRITE) | PAGE_COW`. Parent PTEs also had `|= PAGE_COW` changed to `= (pte & ~PAGE_WRITE) | PAGE_COW` so the parent likewise becomes read-only for the shared pages.
  - **BUG-COW-02** (`pagefault.c`): After COW resolution the new PTE was built as `vmm_getPhysicalAddr(dst) | (memAddr & 0xFFF)`. `memAddr & 0xFFF` is the byte offset within the faulting page, not permission flags — this set random PTE bits (write-through, cache-disabled, dirty, COW, etc.). Fixed to `vmm_getPhysicalAddr(dst) | PAGE_DEFAULT`.
  - **BUG-COW-03** (`vmm_memory.c`): The loop in `vmm_freeProcessPages` that walks the dying task's user-space page tables and calls `adjustCowCounter(..., -1)` for each COW PTE was disabled with `#ifdef _IGNORE`. COW-shared physical pages are owned by the parent's PID and are never found by the child-PID scan, so their reference counts were never decremented on child exit — a physical page leak on every `fork`+`exit`. Re-enabled the block with correct user-space bound (`PD_INDEX(VMM_USER_END)`), PAGE_PRESENT check on both directory and table entries, and spinlock release around `adjustCowCounter` to prevent recursive deadlock.
  - **BUG-COW-04** (`copyvirtualspace.c`): Inner page-table loop in the user-space COW region iterated `i < PD_ENTRIES` — should be `PT_ENTRIES`. Same numeric value (1024) so no runtime effect, but semantically wrong. Fixed.
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
