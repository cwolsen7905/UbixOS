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
8. [Device Driver Model (newbus-lite)](#device-driver-model-newbus-lite)
9. [IRQ Dispatch](#irq-dispatch)
10. [Keyboard Input](#keyboard-input)
11. [Display Stack](#display-stack)
12. [Networking](#networking)
13. [USB Stack](#usb-stack)
14. [Userland](#userland)
15. [Third-Party Components](#third-party-components)

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
│   ├── start.S     - Assembly bootstrap (segments, initial stack, multiboot detection)
│   └── main.c      - C entry point; GDT setup and subsystem initialization table
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
├── net/            - Network stack (lwIP 2.0.3 integration)
├── isa/            - ISA bus device drivers
├── pci/            - PCI bus enumeration and drivers
├── usb/            - USB host controller and device drivers
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

1. **GRUB2** loads the kernel via the multiboot protocol from a FAT32 disk image, passing `boot_device` info in `ebx`.
2. **`sys/init/start.S`** — detects multiboot magic (`0x2BADB002` in `%eax`), saves `_multiboot_info`, establishes segment registers and initial stack, then calls `vmm_init()` → `kmain()`.
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

4. **Subsystem initialization** runs from the table in `sys/include/ubixos/init.h` (in order):

| Step | Function | Notes |
|------|----------|-------|
| 1 | `static_constructors()` | C++ static initializers |
| 2 | `i8259_init()` | Programmable Interrupt Controller (8259 PIC) |
| 3 | `idt_init()` | Interrupt Descriptor Table |
| 4 | `vitals_init()` | Kernel statistics / tick counters |
| 5 | `sysctl_init()` | sysctl interface |
| 6 | `vfs_init()` | Virtual filesystem |
| 7 | `sched_init()` | Preemptive scheduler |
| 8 | `pit_init()` | Programmable Interval Timer (scheduler tick) |
| 9 | `isa_bus_init()` | ISA bus probe: AT keyboard + PS/2 mouse via newbus-lite |
| 10 | `time_init()` | Time services |
| 11 | `devfs_init()` | Device filesystem (must precede `pci_init`) |
| 12 | `pci_init()` | PCI bus enumeration; attaches e1000 NIC, IDE disk, UHCI USB |
| 13 | `tty_init()` | TTY subsystem |
| 14 | `ufs_init()`, `fat_init()` | Filesystem drivers |
| 15 | `net_init()` | lwIP TCP/IP stack + e1000 netif |

5. After init, `kmain` mounts the FAT32 boot partition at `/` using the multiboot `boot_device` field, then `execFile("/bin/init")` launches PID 1.

### SMP Application Processor Startup

APs are brought up via the trampoline in `sys/arch/i386/ap-boot.S`. The BSP writes the trampoline to physical address `0x000000`, signals the AP via the LAPIC, and the AP enters protected mode. Spinlock synchronization in `sys/kernel/smp.c` serializes the AP initialization sequence.

---

## Memory Management (VMM)

Source: `sys/vmm/`

### Address Space Layout

Each process has a private 4 GB virtual address space with the following regions:

```
0x00000000 - 0x003FFFFF  (4 MB)   Identity-mapped — PD[0] all 1024 PT entries
                                   0x000000–0x0FFFFF  ISA/VGA/BIOS (first 1 MB reserved)
                                   0x101000–0x201FFF  Page bitmap staging (256 MB config)
                                   0x300000–0x392000  Kernel image (text/data/BSS)
                                   0x392000+          Page bitmap (placed at page_align(_end))
0x00400000 - 0xBFFFFFFF  (~3 GB)  Per-process — code, data, heap, stack
0xC0000000 - 0xFFFFFFFF  (1 GB)   Kernel-only — not accessible from ring 3
                                   0xC0800000         Page bitmap remapped here (VMM_MMAP_ADDR_PMODE)
```

Physical addresses at or above `numPages × PAGE_SIZE` (≥ 256 MB with default `-m 256` QEMU) are MMIO — framebuffer, PCI BARs, etc. These frames have no entry in `vmmMemoryMap` and must never be passed to `freePage`.

### Key Functions

| Function | Description |
|----------|-------------|
| `vmmInit()` | Top-level init; calls `vmmMemMapInit` then `vmmPagingInit` |
| `vmmMemMapInit()` | Builds the physical page map — a linked list of available frames tracking COW status and ownership |
| `vmmPagingInit()` | Enables paging; sets up the kernel's default page directory |
| `vmmCreateVirtualSpace(pid)` | Allocates a new page directory for a process; pre-maps the shared lower 4 MB identity map and kernel top 1 GB |
| `vmmCopyVirtualSpace(pid)` | Forks the address space using copy-on-write: all pages in 2 MB–3 GB are marked COW; physical copies happen lazily on page-fault |

### Copy-on-Write

When `vmmCopyVirtualSpace()` runs (during `fork()`), no physical memory is copied. Both parent and child share the same frames, marked read-only and COW. On the first write attempt a page fault fires, `pagefault.c` allocates a new frame, copies the content, and updates the faulting process's page table.

---

## Process Model

Source: `sys/kernel/`, `sys/arch/i386/`

- **Scheduler:** O(1) preemptive scheduler driven by PIT interrupts routed through the scheduler TSS (`sched.c`). Tasks are assigned a QoS class (`SCHED_CLASS_RT`, `INTERACTIVE`, `NORMAL`, `BATCH`) which maps to a priority band in the run-queue bitmap. I/O completion boosts priority transiently; CPU-bound tasks decay one band over time; starved tasks are aged upward after `AGING_THRESHOLD` ticks.
- **Task lifecycle:** `READY → RUNNING → BLOCKED/STOPPED → ZOMBIE → (reaped)`. A dying task transitions to `ZOMBIE`, signals the parent with `SIGCHLD`, and is reaped by the parent's `wait4` call. `STOPPED` tasks are suspended by `SIGSTOP` and resumed by `SIGCONT`.
- **Forking:** `fork.c` calls `vmmCopyVirtualSpace()` then duplicates the kernel task structure.
- **ELF execution:** `elf.c` + `execve.c` parse and load standard i386 ELF binaries, set up the initial stack and entry point, then switch to ring 3.
- **Signals:** `signal.c` / `kern_sig.c` implement POSIX signal delivery (`sigaction`, `sigprocmask`, `sigpending`, `sigsuspend`). `SA_RESTART` and `SA_SIGINFO` flags are honoured. Default actions are enforced for `SIGSTOP`, `SIGCONT`, `SIGKILL`, `SIGTTIN`, `SIGCHLD`, etc.
- **Threading:** `ubthread.c` provides user-level thread support.
- **SMP:** `smp.c` coordinates multi-processor scheduling.
- **TTY:** `tty.c` manages up to 5 virtual terminals (`TTY_MAX_TERMS`). `tty_foreground` points to the active terminal; `tty_change()` switches between them.

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
| `vfs/stat.c` | File metadata (`stat`, `statx`, `fstat`) |

### VFS Path Convention

All VFS paths use standard POSIX form (`/bin/init`, `/etc/userdb`, `/dev/tty0`). The historical `sys:` mountpoint prefix is no longer used in userland or kernel code. `cwd` (`_current->oInfo.cwd`) is a plain POSIX path; `sys_getcwd` and `sys_getvfscwd` (native syscall 41) now both return it verbatim (the old strip-vs-full distinction is vestigial).

### Supported Filesystems

| Filesystem | Driver | Notes |
|------------|--------|-------|
| UbixFS v1 | `fs/ubixfs/` | Native UbixOS filesystem |
| UbixFS v2 | `fs/ubixfsv2/` | Improved version with directory caching |
| UFS | `fs/ufs/` | BSD Unix File System |
| FAT16/FAT32 | `fs/fat/` | Boot partition and cross-platform media |
| DevFS | `fs/devfs/` | Virtual device filesystem (`/dev/tty0`…) |
| procfs | `fs/procfs/` | Process information (`/proc/<pid>/status`, `/proc/mounts`) |

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

## Device Driver Model (newbus-lite)

Source: `sys/include/sys/bus.h`, `sys/include/sys/isa_bus.h`

All drivers use a lightweight newbus-inspired model. Each driver declares a `struct ubx_driver`:

```c
struct ubx_driver {
    const char      *drv_name;
    int            (*drv_probe)(struct ubx_device *);   /* 0 = match */
    int            (*drv_attach)(struct ubx_device *);  /* 0 = success */
    int            (*drv_detach)(struct ubx_device *);
};
```

Resources (IRQ, I/O ports, MMIO BARs) are described by `struct ubx_resource` arrays attached to each `struct ubx_device`. `ubx_bus_probe_and_attach()` walks a driver table, calls `drv_probe`, and on match calls `drv_attach`.

### ISA Bus (`sys/isa/`)

`isa_bus_init()` allocates `ubx_device` entries from a static table in `sys/include/sys/isa_bus.h` and probes the ISA driver table:

| File | Device |
|------|--------|
| `8259.c` | Programmable Interrupt Controller (i8259) |
| `pit.c` | Programmable Interval Timer |
| `atkbd.c` | AT keyboard (PS/2) |
| `mouse.c` | PS/2 mouse |
| `fdc.c` | Floppy disk controller |
| `irq.c` + `irq_stubs.S` | Shared IRQ dispatch layer (see [IRQ Dispatch](#irq-dispatch)) |

### PCI Bus (`sys/pci/`)

`pci_init()` enumerates buses 0–1, probes each device, and walks `pci_drv_table`:

| File | Device |
|------|--------|
| `pci.c` | PCI bus enumeration |
| `hd.c` | PCI IDE hard disk (ATA) |
| `e1000.c` | Intel 82540EM Gigabit Ethernet (primary NIC) |
| `lnc.c` | Lance (Am7990) Ethernet — legacy, not in default boot |

USB host controllers found during PCI enumeration are also attached here — see [USB Stack](#usb-stack).

---

## IRQ Dispatch

Source: `sys/isa/irq.c`, `sys/isa/irq_stubs.S`

Multiple drivers may share a hardware IRQ line (e.g., UHCI and e1000 both use IRQ 11 under QEMU's PIIX4). A per-IRQ handler chain avoids the `setVector` clobber problem.

```
Hardware IRQ N
    │
    └── irq_entry_N  (irq_stubs.S — one stub per IRQ 0-15)
            │  pusha / push N / call irq_dispatch / popa / iret
            ▼
        irq_dispatch(int irq)   (irq.c)
            │  walks irq_handlers[irq][0..count]
            │  calls each handler
            └── sends EOI (slave then master for IRQ ≥ 8)
```

Drivers register with `irq_register(irq, handler)`. On first registration the dispatch stub is installed into the IDT and the IRQ is unmasked. Subsequent registrations append to the chain.

**Rule**: handlers must **not** send EOI — `irq_dispatch` sends exactly one EOI after all handlers run.

---

## Keyboard Input

Source: `sys/isa/atkbd.c`, `sys/include/isa/kbd.h`, `sys/include/ubixos/tty.h`

All keyboard input — PS/2 and USB — flows through a single `kbd_ring` circular buffer. The line discipline runs in process context, not in the interrupt handler.

### Data Flow

```
PS/2 IRQ (keyboardHandler)          USB UHCI interrupt (hid_kbd_callback)
    │                                           │
    │  modifier tracking, LED control           │  HID report decode
    │  emergency keys: Ctrl-C (kills           │  HID Usage → ASCII / KEY_*
    │    foreground task), reboot,              │
    │    Ctrl-X (panic), console switch        │
    │                                           │
    └──────────── kbd_ring_push(kc, 1) ────────┘
                         │
              kbd_ring[]  (64-entry circular buffer)
                         │
          ┌──────────────┴──────────────┐
          │                             │
    TTY mode                       GUI mode
    (no views running)             (views running)
          │                             │
    getchar()                    sys_getkbd() → views
    drains ring                  drains ring raw
    through kbd_apply_event      routes DISPLAY_KEY MPI
    (canonical/raw editing)      → focused window (term)
          │
    tty_foreground->stdin[]
          │
    sys_read(fd=0) returns line
```

### kbd_apply_event (single line discipline)

`kbd_apply_event()` in `atkbd.c` is the **only** place canonical/raw TTY editing is implemented:
- **Backspace**: erase from `t_linebuf` (canonical) or deliver raw
- **Enter**: flush `t_linebuf` → `tty_foreground->stdin[]`, echo newline
- **Ctrl-U**: clear `t_linebuf`
- **Printable chars**: append to `t_linebuf` + echo (canonical) or deliver raw

The PS/2 ISR and USB callback both do **zero** line discipline — they only translate hardware events to keycodes and push to `kbd_ring`.

### Key Types

| Range | Meaning |
|-------|---------|
| `0x01`–`0xFF` | Translated ASCII (shift/ctrl already applied) |
| `0x100`+ | Special keys: `KEY_UP`, `KEY_F1`, etc. — GUI only, TTY ignores |

### Syscalls

| Syscall | Number | Description |
|---------|--------|-------------|
| `sys_getkbd` | 46 (native) | Drain one event from `kbd_ring`; used by views |

---

## Display Stack

Source: `bin/views/`, `bin/term/`, `bin/taskbar/`, `lib/objgfx/`, `sys/kernel/fb.c`

Two-layer graphical system modelled after WindowServer + Core Graphics:

```
┌────────────────────────────────────────┐
│  Application (bin/term, bin/muffin…)   │
│  Renders into shared-memory buffer     │
│  via lib/objgfx (ogSurface/ogScalableFont) │
└──────────────┬─────────────────────────┘
               │  MPI: DISPLAY_FLIP / DISPLAY_KEY / DISPLAY_CLOSE
               │  vmm_share_region: shared framebuffer window buffer
               ▼
┌────────────────────────────────────────┐
│  views compositor (bin/views)          │
│  Owns VESA framebuffer via sys_mapfb() │
│  Manages windows, Z-order, decorations │
│  Polls mouse (syscall 44) + kbd (46)   │
│  Composites damage regions to screen   │
│  Uses shadow back-buffer for flicker   │
└────────────────────────────────────────┘
```

**Rules:**
- `views` is the only process that calls `sys_mapfb()`. All other processes receive a `vmm_share_region` buffer.
- Apps render with `objGFX` (`ogSurface`/`ogScalableFont`; `ogBitFont` is legacy). No app writes to the framebuffer directly.
- MPI carries only signals (`DISPLAY_CLAIM`, `DISPLAY_FLIP`, `DISPLAY_KEY`, `DISPLAY_CLOSE`), never drawing commands.
- The compositor uses deferred damage tracking — only dirty regions are recomposited each frame.

### GUI Terminal (bin/term)

`bin/term` is a windowed terminal emulator. It receives `DISPLAY_KEY` MPI messages from `views`, does its own canonical line editing, and writes completed lines to the shell's stdin via a pipe. The shell process reads from that pipe (fd type 3), not from the keyboard ring directly.

---

## Networking

Source: `sys/net/`, `sys/pci/e1000.c`

UbixOS delegates the TCP/IP stack to **lwIP 2.0.3** (Lightweight IP), integrated through the network interface layer in `sys/net/netif/`. The socket API in `sys/net/api/` exposes standard BSD socket calls to userland.

### Primary NIC: Intel e1000 (82540EM)

`sys/pci/e1000.c` drives the Intel 82540EM Gigabit Ethernet controller (QEMU's default `e1000` model). It is registered via newbus-lite as `e1000_ubx_driver` and uses `irq_register()` to share IRQ 11 with UHCI.

The e1000 netif bridge (`sys/net/netif/e1000netif.c`) glues the driver to lwIP's `netif` abstraction.

### Legacy NIC

`sys/pci/lnc.c` (Lance/Am7990) is retained in the source tree but is **not** in the default `pci_drv_table`; it was replaced by e1000 in the boot sequence.

---

## USB Stack

Source: `sys/usb/`

| File | Role |
|------|------|
| `usb.h` / `usb_driver.h` | Core types: `usb_device`, `usb_driver`, `usb_ep_desc` |
| `usb.c` | Device enumeration: `usb_new_device()` — reads descriptors, matches driver table |
| `uhci.c` | UHCI host controller driver — DMA ring, control/bulk/interrupt transfers |
| `hid_kbd.c` | HID boot-protocol keyboard driver (class 3/1/1) |

### UHCI Host Controller

`uhci_ubx_driver` is registered in `pci_drv_table` and attaches to any PCI device with class `0x0C/0x03/0x00` (USB UHCI). It allocates a DMA pool of QH/TD pairs, wires interrupt slot QHs into all 1024 frame list entries, and registers its ISR via `irq_register()`.

**Pool layout** (within the DMA page):
```
indices 0-2      : control skeleton QH + bulk/term QH + frame-list sentinel
indices 3-6      : interrupt slots (UHCI_INTR_SLOTS = 4)
indices 7+       : free for control/bulk transfers (UHCI_CTRL_BASE = 7)
```

### USB Driver Matching

`usb_new_device()` walks a `usb_drivers[]` table. Each driver declares `drv_class / drv_subclass / drv_protocol`. The HID keyboard driver (`hid_kbd_driver`) matches class 3/1/1 and calls `uhci_schedule_intr()` to arm an 8-byte interrupt IN transfer. Its callback (`hid_kbd_callback`) decodes HID Usage Page 0x07 reports and pushes translated keycodes into `kbd_ring` via `kbd_ring_push()`.

---

## Userland

Userland is built separately from the kernel. All binaries link against **musl libc** (in `contrib/musl/`). The world build produces output in `build/bin/`, `build/lib/`, `build/libexec/`.

### Executables (`bin/`)

| Binary | Description |
|--------|-------------|
| `init` | PID 1 — system initialization and process supervision |
| `login` | User authentication |
| `tcsh` | tcsh 6.24.16 — primary interactive shell |
| `cat`, `cp`, `ls` | Core file utilities |
| `mount` | Show or perform filesystem mounts (no-arg reads `/proc/mounts`) |
| `fdisk`, `disklabel`, `format` | Disk management |
| `kill` | Signal delivery |
| `stat` | File metadata |
| `ed` | POSIX `ed` line editor |
| `uname` | Print OS/hardware identification |
| `ps` | Process status listing (reads procfs) |
| `clock` | System clock display |
| `ttyd` | TTY daemon |
| `views` | GUI compositor — owns framebuffer, composites windows, routes input |
| `taskbar` | Taskbar panel with launcher button and clock |
| `term` | Windowed terminal emulator — pipes stdin/stdout to/from shell |

### Libraries (`lib/`)

| Library | Description |
|---------|-------------|
| `msun/` | Math library |
| `objgfx/` | C++ surface rendering API (`ogSurface`, `ogScalableFont` (TrueType); `ogBitFont` legacy) — headers in `include/objgfx/` |
| `ubix_api/` | UbixOS-specific API (`ubix_getcwd`, MPI helpers, `gettime`) |
| `ubix/` | Core startup library — `crt1` (`_start.S`) and static initializers |
| `libedit/` | Line editing library |
| `libmd/` | Message digest library |
| `csu/` | C startup support (crtbegin/crtend) |

### Runtime Linker (`libexec/`)

The runtime dynamic linker is musl's (`/lib/ld-musl-i386.so.1`, the `PT_INTERP` of userland binaries). Shared libraries are installed at `/lib/` on the UbixOS volume. (The older native `libexec/ld` — which used `sys:/lib/` — is superseded.)

---

## Third-Party Components

| Component | Location | Purpose |
|-----------|----------|---------|
| lwIP 2.0.3 | `contrib/lwip-2.0.3/` | TCP/IP network stack |
| musl libc | `contrib/musl/` | Primary C library for userland |
| jemalloc | `contrib/jemalloc/` | Memory allocator |
| gdtoa | `contrib/gdtoa/` | float/double ↔ ASCII conversion |
| TCC | `contrib/tcc/` | Tiny C Compiler (self-hosted builds) |
| tzcode | `contrib/tzcode/` | Timezone database and library |
| libc-pwcache | `contrib/libc-pwcache/` | User/group password cache |
| libc-vis | `contrib/libc-vis/` | String encoding/visualization |
| NetBSD tests | `contrib/netbsd-tests/` | Portable test suite |
| LLVM libc++ | `contrib/libcxx/` | C++ standard library for views/taskbar/term |
| libcxxabi | `contrib/libcxxabi/` | C++ ABI support (exception handling, RTTI) |
