# UbixOS Architecture

This document describes the internal design of the UbixOS kernel, userland, and build system.

---

## Table of Contents

1. [Overview](#overview)
2. [Kernel Source Layout](#kernel-source-layout)
3. [Boot Sequence](#boot-sequence)
4. [Memory Management (VMM)](#memory-management-vmm)
5. [Process Model](#process-model)
6. [Filesystem Layer (VFS)](#filesystem-layer-vfs)
7. [Inter-Process Communication](#inter-process-communication)
8. [Device Drivers](#device-drivers)
9. [Networking](#networking)
10. [Userland](#userland)
11. [Third-Party Components](#third-party-components)

---

## Overview

UbixOS is a monolithic kernel targeting x86 (i386) in 32-bit protected mode. It runs in a single privilege boundary (ring 0) with a clear ring 3 user space enforced through the GDT. The kernel is compiled as a position-independent bare-metal binary linked by `sys/compile/ldscript.i386`.

Secondary architecture support for ARMv6 exists under `sys/arch/armv6/` and `sys/boot/arm/` but is not production-ready.

---

## Kernel Source Layout

```
sys/
├── arch/
│   ├── i386/       - x86 CPU, interrupts, syscall dispatch, fork, scheduler, SMP AP boot
│   └── armv6/      - ARMv6 support (experimental)
├── init/
│   ├── start.S     - Assembly bootstrap (segments, initial stack)
│   └── main.c      - C entry point; GDT setup and subsystem initialization
├── kernel/         - Core kernel: scheduler, ELF loader, exec, signals, TTY, pipes, SMP
├── vmm/            - Virtual memory manager (paging, COW, address space management)
├── fs/
│   ├── vfs/        - Virtual filesystem layer
│   ├── ubixfs/     - UbixFS v1 filesystem driver
│   ├── ubixfsv2/   - UbixFS v2 filesystem driver
│   ├── ufs/        - BSD UFS filesystem driver
│   ├── fat/        - FAT16/FAT32 filesystem driver
│   ├── devfs/      - Device filesystem driver
│   └── common/     - Shared filesystem utilities
├── mpi/            - Message passing interface (IPC)
├── net/            - Network stack (lwIP integration)
├── isa/            - ISA bus device drivers
├── pci/            - PCI bus enumeration and drivers
├── sde/            - Software display environment (graphics)
├── lib/            - Kernel library (kprintf, kmalloc, etc.)
├── sys/            - Core system services (DMA, IDT, I/O ports, video)
├── include/        - All kernel-internal headers
└── compile/
    ├── ldscript.i386  - Linker script for final kernel binary
    └── kernel         - Final linked kernel output
```

---

## Boot Sequence

### x86 Startup

1. **Bootloader** loads the kernel binary into memory and transfers control.
2. **`sys/init/start.S`** — assembly trampoline that establishes segment registers and an initial stack, then calls into C.
3. **`sys/init/main.c`** — C entry point. Builds the Global Descriptor Table (GDT) with 11 descriptors:

| Selector | Purpose |
|----------|---------|
| `0x00` | Null descriptor |
| `0x08` | Ring 0 code segment (kernel CS) |
| `0x10` | Ring 0 data segment (kernel DS) |
| `0x18` | Local Descriptor Table (LDT) |
| `0x20` | Scheduler Task State Segment (TSS) |
| `0x28` | Ring 3 code segment (user CS) |
| `0x30` | Ring 3 data segment (user DS) |
| `0x38` | GPF handler TSS |
| `0x40` | Stack fault handler TSS |
| `0x48` | SMP private data |
| `0x50` | User `%gs` (stack pointer) |

4. **Subsystem initialization** (in order):
   - `static_constructors()` — C++ static initializers
   - `i8259_init()` — Programmable Interrupt Controller
   - `idt_init()` — Interrupt Descriptor Table
   - `vitals_init()` — Kernel statistics
   - `sysctl_init()` — sysctl interface
   - `vfs_init()` — Virtual filesystem
   - `sched_init()` — Scheduler
   - `pit_init()` — Programmable Interval Timer
   - `atkbd_init()` — AT keyboard
   - `time_init()` — Time services
   - `pci_init()` — PCI bus enumeration
   - `devfs_init()` — Device filesystem
   - `tty_init()` — TTY subsystem
   - `ufs_init()`, `fat_init()` — Filesystem drivers
   - `initHardDisk()` — Hard disk
   - `initLNC()` — Lance network adapter
   - `net_init()` — TCP/IP stack

### SMP Application Processor Startup

APs are brought up via the trampoline in `sys/arch/i386/ap-boot.S`. The BSP writes the trampoline to physical address `0x000000`, signals the AP via the LAPIC, and the AP enters protected mode. Spinlock synchronization in `sys/kernel/smp.c` serializes the AP initialization sequence.

---

## Memory Management (VMM)

Source: `sys/vmm/`

### Address Space Layout

Each process has a private 4 GB virtual address space with the following regions:

```
0x00000000 - 0x000FFFFF  (1 MB)   Shared — kernel code, BIOS data, video buffers
                                   Identity-mapped 1:1 across all processes
0x00100000 - 0xBFFFFFFF  (~3 GB)  Per-process — code, data, heap, stack
0xC0000000 - 0xFFFFFFFF  (1 GB)   Kernel-only — not accessible from ring 3
                                   (unless executing a syscall)
```

At `0x00100000` each process stores its own page directory. Page table slot `0x768` (`PDE[0x300]`) points to the shared kernel page tables mapped into the top 1 GB.

### Key Functions

| Function | Description |
|----------|-------------|
| `vmmInit()` | Top-level init; calls `vmmMemMapInit` then `vmmPagingInit` |
| `vmmMemMapInit()` | Builds the physical page map — a linked list of available frames tracking COW status and ownership |
| `vmmPagingInit()` | Enables paging; sets up the kernel's default page directory and remaps the physical page map into the top 1 GB |
| `vmmCreateVirtualSpace(pid)` | Allocates a new page directory for a process; pre-maps the shared lower 1 MB and kernel top 1 GB |
| `vmmCopyVirtualSpace(pid)` | Forks the address space using copy-on-write: all pages in 2 MB–3 GB are marked COW; physical copies happen lazily on page-fault |

### Copy-on-Write

When `vmmCopyVirtualSpace()` runs (during `fork()`), no physical memory is copied. Both parent and child share the same frames, marked read-only and COW. On the first write attempt a page fault fires, `pagefault.c` allocates a new frame, copies the content, and updates the faulting process's page table.

---

## Process Model

Source: `sys/kernel/`, `sys/arch/i386/`

- **Multitasking:** Preemptive, driven by PIT interrupts routed through the scheduler TSS (`sched.c`).
- **Forking:** `fork.c` calls `vmmCopyVirtualSpace()` then duplicates the kernel task structure.
- **ELF execution:** `elf.c` + `execve.c` parse and load standard i386 ELF binaries, set up the initial stack and entry point, then switch to ring 3.
- **Signals:** `signal.c` / `kern_sig.c` implement POSIX-style signal delivery.
- **Threading:** `ubthread.c` provides user-level thread support.
- **SMP:** `smp.c` coordinates multi-processor scheduling.
- **TTY:** `tty.c` manages terminal I/O.

---

## Filesystem Layer (VFS)

Source: `sys/fs/`

The VFS provides a uniform interface over multiple concrete filesystems:

| Component | Description |
|-----------|-------------|
| `vfs/file.c` | `open`, `read`, `write`, `close` dispatch |
| `vfs/inode.c` | Inode allocation and caching |
| `vfs/mount.c` | Filesystem mount/unmount |
| `vfs/namei.c` | Path-to-inode resolution |
| `vfs/stat.c` | File metadata |

### Supported Filesystems

| Filesystem | Driver | Notes |
|------------|--------|-------|
| UbixFS v1 | `fs/ubixfs/` | Native UbixOS filesystem |
| UbixFS v2 | `fs/ubixfsv2/` | Improved version with directory caching |
| UFS | `fs/ufs/` | BSD Unix File System; file size hacked for compatibility |
| FAT16/FAT32 | `fs/fat/` | For bootloader and cross-platform media |
| DevFS | `fs/devfs/` | Device abstraction (e.g., `/dev/`) |

---

## Inter-Process Communication

Source: `sys/mpi/`, `sys/kernel/`

UbixOS uses a custom **Message Passing Interface (MPI)** as its primary IPC mechanism, supplemented by traditional Unix primitives:

| Mechanism | Files | Description |
|-----------|-------|-------------|
| MPI messages | `mpi/message.c`, `mpi/system.c` | Mailbox-based async message passing |
| MPI syscalls | `mpi/mpi_syscalls.c` | User-space MPI system call interface |
| Pipes | `kernel/kern_pipe.c`, `kernel/pipe.c` | Traditional Unix pipes |
| Semaphores | `kernel/sem.c` | Counting semaphores for synchronization |
| File descriptors | `kernel/descrip.c` | Unified fd table shared across IPC types |

The `init` process (PID 1) uses MPI mailboxes to communicate with child processes and the login daemon.

---

## Device Drivers

### ISA Bus (`sys/isa/`)

| File | Device |
|------|--------|
| `8259.c` | Programmable Interrupt Controller (i8259) |
| `pit.c` | Programmable Interval Timer |
| `atkbd.c` | AT keyboard (PS/2) |
| `fdc.c` | Floppy disk controller |
| `ne2k.c` | NE2000-compatible Ethernet NIC |
| `rs232.c` | Serial port (RS-232) |
| `mouse.c` | PS/2 mouse |

### PCI Bus (`sys/pci/`)

| File | Device |
|------|--------|
| `pci.c` | PCI bus enumeration |
| `hd.c` | PCI hard disk (IDE) |
| `lnc.c` | Lance (Am7990) Ethernet adapter |

### Adding a New Driver

See `doc/sample_driver.c` for a skeleton that demonstrates the device registration API.

---

## Networking

Source: `sys/net/`, `contrib/lwip-2.0.3/`

UbixOS delegates the TCP/IP stack to **lwIP 2.0.3** (Lightweight IP), integrated through the network interface layer in `sys/net/netif/`. The socket API in `sys/net/api/` exposes standard BSD socket calls to userland.

Supported NICs: NE2000 (`sys/isa/ne2k.c`), Lance LNC (`sys/pci/lnc.c`).

---

## Userland

### Executables (`bin/`)

| Binary | Description |
|--------|-------------|
| `init` | PID 1 — system initialization and process supervision |
| `login` | User authentication |
| `sh` / `shell` | Command shell |
| `cat`, `cp`, `ls` | Core file utilities |
| `mount` | Filesystem mounting |
| `fdisk`, `disklabel`, `format` | Disk management |
| `kill` | Signal delivery |
| `stat` | File metadata |
| `edit` | Text editor |
| `clock` | System clock display |
| `ttyd` | TTY daemon |
| `views` | Graphics viewer |
| `launcher` | Application launcher |

### Libraries (`lib/`)

| Library | Description |
|---------|-------------|
| `libc/` | FreeBSD-derived POSIX C library |
| `libstdc++/` | C++ standard library |
| `libcpp/` | C++ runtime support |
| `msun/` | Math library |
| `libmd/` | Message digest (MD5, SHA) |
| `libedit/` | BSD editline (readline-compatible) |
| `objgfx40/` | Object graphics library |
| `views/` | GUI framework |
| `ubix_api/` | UbixOS-specific API |
| `ubix/` | Core library with static startup code |

### Runtime Linker (`libexec/`)

The dynamic linker (`ld.so`) resolves shared library references at runtime. Libraries must be present at `sys:/lib/` on the mounted UbixOS volume.

---

## Third-Party Components

| Component | Location | Purpose |
|-----------|----------|---------|
| lwIP 2.0.3 | `contrib/lwip-2.0.3/` | TCP/IP network stack |
| jemalloc | `contrib/jemalloc/` | Memory allocator |
| gdtoa | `contrib/gdtoa/` | float/double ↔ ASCII conversion |
| TCC | `contrib/tcc/` | Tiny C Compiler |
| tzcode | `contrib/tzcode/` | Timezone database and library |
| libc-pwcache | `contrib/libc-pwcache/` | User/group password cache |
| libc-vis | `contrib/libc-vis/` | String encoding/visualization |
| NetBSD tests | `contrib/netbsd-tests/` | Portable test suite |
