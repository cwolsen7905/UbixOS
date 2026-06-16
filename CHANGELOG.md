# Changelog

All notable changes to UbixOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

First entries of the 3.0 series (64-bit only: x86_64 + aarch64).  Development on
`wip/aarch64-port`; i386 remains as the reference until x86_64 reaches parity.

### Added
- **x86_64 port (3.0 forward target, replaces i386)** — bring-up in progress:
  - *Phase 1 — long-mode boot.* A 64-bit kernel that boots under `qemu -kernel` and
    prints a COM1 banner.  New `x86_64-elf-` native (no `-m32`) toolchain config,
    ELF64 linker script, a 32-bit→long-mode trampoline in `start.S` (identity-map the
    low 1 GB with 2 MB pages → PAE + EFER.LME + paging → 64-bit GDT → far-jump →
    `kmain_x86_64`), booting via the PVH protocol (QEMU's multiboot loader refuses
    ELF64).
  - *Phase 2 — IDT + exceptions.* A 256-entry 64-bit IDT with the 32 CPU-exception
    stubs; the handler dumps the trapframe (vector, error, RIP/CS/RFLAGS, CR2) over
    serial so faults are visible instead of a silent triple-fault.
  - *Phase 3 — physical memory + kmalloc.* Reuses the machine-independent page
    allocator (`vmm_memory.c`) + `kmalloc` over a 256 MB identity-mapped RAM, via an
    x86_64 `vmm_machdep` + bring-up support shims (atomic spinlock, kpanic, minimal
    kprintf).  Adds the x86_64 MD header set (`<machine/{cpu,proc}.h>` now resolve to
    x86_64, not the 32-bit i386 headers).
  - *Phase 4a — PIC + PIT + IRQs.* Remap the 8259 PICs to vectors 32-47, a 100 Hz
    PIT tick, and IRQ dispatch (+ EOI) from the common ISR path — the timer/interrupt
    foundation for the scheduler.
  - *Phase 4b-1 — context switch.* `cpu_switch.S` (`x86_64_ctx_switch`): callee-saved
    + RSP save/restore with a seeded initial frame for fresh threads — verified in
    isolation (a kernel thread enters, yields back, and resumes).
  - *Phase 4b-2 — generic scheduler.* The arch-neutral scheduler
    (`sys/kern/sched_core.c` + `sched_dispatch.c`) now runs unmodified on x86_64 over
    that context switch, via x86_64 md hooks (`md_new_task` /
    `md_setup_initial_frame` / `switch_to` in `arch/x86_64/kern/proc.c`).  Two demo
    kernel threads created with `schedNewTask()`/`sched_ready()` cooperatively yield
    through the real `sched()`/`sched_yield()` dispatch (verified: both alternate
    three iterations and return to the boot context).  The PIT tick charges
    per-process CPU time (`sched_account_tick`); like aarch64 the kernel is
    non-preemptive until userland lands (no EL0 yet), so kernel threads yield.
  - *SMP M0 — per-CPU state.* `_current` is now per-CPU (one `struct pcpu` per core
    in `g_pcpu[]`), reached through the GS segment base — the long-mode analog of
    i386's `%gs:8` and aarch64's `TPIDR_EL1`, but the base comes from the GS_BASE MSR
    (`x86_64_pcpu_install` writes it once per CPU; `current` at `%gs:16`).  Installed
    at the top of `kmain` so the timer's `sched_account_tick()` always sees a valid
    block.  Also makes `<machine/{ansi,limits}.h>` resolve to LP64 x86_64 headers (the
    32-bit i386 fallback truncated every `uintptr_t` pointer cast to 32 bits).  The
    foundation for running secondary cores (LAPIC + AP trampoline next).
  - *Phase 5a — ring 3 + syscalls.* The kernel can now drop to **user mode** and
    service a syscall: a writable GDT adds the ring-3 code/data segments + a 64-bit
    TSS (the ring-0 stack the CPU loads on a ring3->ring0 trap); a 4 KB user page is
    mapped (real page-table walk in an unused PDPT slot, clear of the 2 MB boot map);
    `int $0x80` enters through a DPL3 IDT gate.  Verified end-to-end: a hand-rolled
    ring-3 payload `write`s a string and `exit`s, returning to the kernel via a
    coroutine swap.  (`syscall`/`sysret` + the FreeBSD ABI come with the ELF loader.)
  - *Phase 5b — per-process address spaces.* A ring-3 process the **scheduler**
    dispatches, each in its own page tables: `x86_64_create_user_space()` builds a
    private PML4 (sharing the kernel's low-1 GB PD by pointer, private user region
    above 1 GB); `switch_to` swaps CR3 + re-arms the TSS `rsp0` to the next task's
    kernel stack; a user task's first dispatch runs a `user_trampoline` that IRETQs
    to ring 3 (vs. the kernel-thread trampoline).  `exit` terminates the task
    (`endTask` -> ZOMBIE) and schedules on.  A ring-3 fault now kills just the task
    (CS RPL check) instead of halting the kernel — the x86_64 analog of aarch64's
    EL0->kill containment.  Verified: a scheduled process runs at ring 3, writes via
    a syscall, and exits, with control returning to the scheduler.
  - *Phase 5c (block device) — virtio-blk-pci.* The first **disk** driver: minimal
    PCI enumeration (legacy 0xCF8/0xCFC config mechanism) finds the virtio-blk
    device, and a polled legacy virtio-pci block driver (one contiguous vring,
    3-descriptor request chain — the virtqueue logic shared with aarch64, only the
    PCI-I/O-port transport differs) reads/writes 512-byte sectors.  Verified: reads
    sector 0 of the attached image and confirms the MBR `0x55AA` signature.  (The
    FAT/VFS root mount on top is 5c-2.)
  - *Phase 5c-2 — FAT root over the VFS.* The machine-independent VFS / buffer-cache
    / FAT driver now link into the x86_64 kernel and **mount a real disk root**:
    `vfs_mount(VFS_TYPE_FAT, "/")` on the virtio-blk partition, with directory
    listing working (`vfs_opendir`/`readdir` enumerate the volume).  This pulled the
    MI lib up to the real implementations — `kprintf.c`/`kconsole.c` (a COM1 sink
    replaces the bring-up stub kprintf), `string.c`/`strlen.c`/`strstr.c`, plus a
    real `systemVitals` (the VFS keeps its mount/fs lists there) and a minimal
    `getfd`.  `<stdarg.h>` now uses `__builtin_va_list` on x86_64 (the i386 `char*`
    typedef breaks 64-bit `va_arg`).  Verified: mounts the image's FAT32 volume and
    lists `/grub` + `/kernel`.
  - *Phase 5d (higher-half kernel, step A)* — groundwork for the ELF loader: the
    kernel is relinked at the higher half (`KERNBASE = 0xFFFFFFFF80000000`, +1 MB)
    so user space can own the entire low canonical half (standard amd64 layout,
    needed because static ELF64 binaries load at low VAs like 0x400000).  start.S's
    32-bit stub references symbols by physical address (`sym - KERNBASE`), maps both
    a low identity window and the kernel high half, enters long mode at a low label,
    then jumps to the high VMA.  Per-process PML4s now share the kernel's high-half
    entries (PML4[256..511]) so the kernel stays mapped across a CR3 switch.  The low
    identity map is retained for now (physical access); a high physmap + dropping the
    identity is step B.  Verified: boots high, runs the scheduler + ring-3 processes
    + mounts the FAT root, all from the higher-half VMA.
  - *Phase 5d (higher-half kernel, step B)* — completes the standard amd64 address
    layout.  A **physmap** (direct map of RAM at `PHYSMAP_BASE = 0xFFFF800000000000`,
    PML4[256]) gives the kernel pointer access to any physical page (`P2V`/`V2P`),
    replacing the low identity map: the page allocator/heap, the page-table walker,
    and the virtio-blk vring/DMA were converted to it (the device still gets physical
    addresses; the CPU touches the rings through the physmap).  Per-process address
    spaces now leave the **entire low half empty** (private, clean) and share only
    the kernel higher half, so user binaries load at their standard low VAs — the
    ring-3 demo now runs at **0x400000** (the amd64 ELF base) instead of a >1 GB
    work-around.  Layout: user = low canonical half, physmap = `0xFFFF8…`, kernel =
    `0xFFFFFFFF8…`.  Verified: ring-3 process at 0x400000 + FAT root mount.
  - *Phase 5d (step C, ELF64 loader)* — the arch-neutral ELF64 loader
    (`sys/kern/elf64_load.c`, shared with aarch64) now runs on x86_64 via two md
    hooks (`md_map_user_page` over the page-table walker, `md_sync_icache` a no-op —
    x86 I-cache is coherent) + an `x86_64/elf.h` (`ELF_TARG_MACH = EM_X86_64`).  A
    synthesized static ET_EXEC wrapping the ring-3 payload loads at the amd64 base
    (0x400000) into a fresh address space and runs as a scheduled process.  Also
    fixes the boot task's `md_cr3` (0 -> the boot PML4) so `switch_to` restores a
    CR3 with the low identity when a user task exits back to the kernel (the MI
    loader's direct frame access needs it; user PML4s have a clean low half).
    Verified: ELF64 process runs + exits.  (Real `syscall`/`sysret` + the FreeBSD
    ABI, for the musl world, are next.)
  - *Phase 5d (step C-2, SYSCALL/SYSRET)* — the real fast syscall path musl uses.
    `x86_64_syscall_init` enables EFER.SCE + STAR/LSTAR/SFMASK; an asm entry stub
    (`syscall_entry.S`) builds the shared trapframe and returns via `SYSRETQ`.  No
    `swapgs` is needed — the kernel owns `%gs` (musl keeps TLS in `%fs`), so the
    per-CPU kernel-stack top (new `struct pcpu.kernel_rsp`, set by `switch_to`) is
    read straight through `%gs` (the `syscall` instruction does not stack-switch).
    Both entry paths now share one FreeBSD-ABI handler (write = 4, exit = 1).
    Verified: an ELF64 process makes `write`/`exit` via the `syscall` instruction
    ("hello from ring 3 (syscall insn)") and `int 0x80` still works.  Routing to the
    full MI POSIX table comes with the world (5e).
  - *Phase 5e (start) — userland file I/O.* The syscall handler gains real
    `open`/`read`/`close` (FreeBSD 5/3/6) bridged to the VFS via `fopen`/`fread`/
    `fclose` + the task's `td.o_files[]` fd table (the same machinery `sys_open`
    uses).  Verified end to end: a ring-3 ELF64 process opens `/kernel/kernel` off
    the FAT root, reads 32 bytes, and writes them out — the real ELF header bytes
    (`\x7fELF…`) flow user → `syscall` → VFS → FAT → virtio-blk → user.  (The
    handler still services a hand-picked set; routing through the full 585-entry MI
    POSIX table — which pulls the entire syscall surface — lands with the musl world.)
  - *Phase 5e — the MI POSIX syscall table dispatches on x86_64.* The full
    `syscalls_posix.c` table (585 entries) + the table-driven dispatch engine
    (`ksyscall_dispatch`) now link into the x86_64 kernel, and `x86_64_syscall`
    routes through them (args from RDI/RSI/RDX/R10/R8/R9 per the FreeBSD/SysV ABI;
    a serial fast path for `write` to fd 1/2 and special `exit` handling remain).
    Bringing the table in pulled the machine-independent kernel up to its real
    implementations — `descrip`/`vitals`/`callout`/`random`/`klog`/`endtask`/
    `cpu_enum`, the POSIX file/dir/process/pipe/sem/time calls, MPI, procfs/devfs/
    ramfs — so the bring-up stubs in `ksupport.c` were retired (and `vitals_init`
    now allocates `systemVitals`).  The subsystems still needing x86_64 MD glue
    (exec/fork, mmap/VM, signals, sockets/lwIP, pty/tty, the display/input devices)
    are stubbed in `syscall_stubs.c` so the table links; each lands as its
    subsystem is ported.  Verified: a ring-3 ELF64 process opens + reads
    `/kernel/kernel` through the **real** `sys_open`/`sys_read` via the table.
  - *Phase 5e — mmap/brk VM glue.* `sys/arch/x86_64/kern/syscall_md.c` implements
    anonymous `mmap`/`mmap2` + `brk` (and `munmap`/`madvise`/`msync` as accepted
    no-ops, `machine_set_tls` via the FS_BASE MSR for musl TLS) — the allocator
    surface musl needs.  Pages are mapped via the x86_64 page-table walker with the
    zero-fill going through the **physmap** (works under the user CR3, which has no
    low identity for fresh frames).  mmap/brk are intercepted in the syscall entry
    and return the 64-bit VA directly, since the generic dispatch's `td_retval[]` is
    `int` (32-bit) and would truncate a user address.  Per-process spaces share the
    kernel low-1 GB identity again (so the MI ELF loader's direct frame access keeps
    working) and user mappings live above it (ELF + stack at 1 GB, brk 7 GB, mmap
    8 GB — matching aarch64).  Verified: a ring-3 process `mmap`s a page, writes to
    it, and reads a file — all through the `syscall` instruction.
  - *Phase 5e — execve (real on-disk binaries).* `arch/x86_64/kern/execfile.c`:
    `x86_64_build_user_image` loads an ELF64 into a fresh address space and builds
    the minimal SysV amd64 initial stack (argc/argv/envp + an `AT_PAGESZ` auxv
    entry, which musl reads or `malloc` fails); `sys_execve` reads a path off the
    VFS (`fopen`/`fread`), builds the image, and IRETQs the calling task into it.
    Verified end to end: a real static ELF64 (`/hello`, built with the cross
    toolchain + `mcopy`'d onto the FAT image) is loaded off disk and runs at ring 3
    — "hello from a real ELF64 on disk".  This is the exact path the musl world will
    load through.  (The test binary's source + build steps are in
    `tools/x86_64-test/`.)
  - *Phase 5e — fork(2).* `arch/x86_64/kern/fork.c`: a child task with a deep copy
    of the parent's user pages (the private PDPT[1..] region above the 1 GB
    identity) and a duplicate of the fork-syscall trapframe with `rax = 0`.  The
    child's kernel stack is seeded so its first dispatch lands in `ret_from_fork`
    (the shared pop-and-`SYSRETQ` tail in `syscall_entry.S`), resuming it in ring 3
    at the fork return point.  fork is intercepted in the syscall entry (it needs
    the trapframe).  Verified: a `/forktest` binary forks and both parent ("child
    pid in rax") and child ("fork returned 0") run.
  - *Phase 5e — POSIX signals.* x86_64 signal delivery + `sigreturn`: an
    `#ifdef __x86_64__` block in the MI `signal.c` builds a sigframe on the user
    stack (saving the GP regs + RIP/RFLAGS/RSP into a `ubx_sigcontext`) and
    redirects the trapframe into the handler (RDI = signo).  Like aarch64, the
    handler "returns" to a magic unmapped address (`X86_64_SIGTRAMP_RETADDR`); the
    ring-3 `#PF` on it is turned into `sys_sigreturn`, which restores the context —
    no executable user trampoline needed.  `signal_check` runs on the syscall
    return path; new `x86_64/signal.h` + an x86_64 `struct trapframe` (in
    `sys/trap.h`) carry the layout.  Verified: `/sigtest` installs a SIGINT handler,
    `kill`s itself, the handler runs and `sigreturn` resumes it.

  - *Phase 5e — musl libc runs.* The cross-build support to compile **musl** for
    x86_64: a FreeBSD-numbered `bits/syscall.h.in` (stock musl x86_64 ships Linux
    numbers — `write=1`, `mmap=9` — wrong for the FreeBSD ABI table) plus the
    UbixOS ABI `bits/{fcntl,ioctl,ipcstat,msg,shm,termios,sem}.h`, and an x86_64 PIE
    branch in `share/mk/ubix.musl.vars.mk`.  Two kernel fixes were needed to run a
    real libc binary: (1) `start.S` now enables SSE at boot — `CR4.OSFXSR`/`OSXMMEXCPT`,
    `CR0.MP`, clear `CR0.EM`, `fninit` — since musl's userland is built with SSE and
    `pxor`/`movaps` `#UD` without it; (2) musl sets up TLS via `arch_prctl(ARCH_SET_FS)`,
    which the kernel intercepts in the syscall entry and routes to `machine_set_tls`
    (FS_BASE MSR) — no FreeBSD syscall number collides.  Verified: a static
    musl-linked binary prints via `write`, `malloc`s (SSE), opens/reads files, and
    `exit`s cleanly through `__libc_start_main`.

  - *Phase 5e — world libraries cross-build.* `bmake world TARGET=x86_64` now
    builds Steps 0-2 (musl, libc++/libc++abi, and all world libraries — objGFX, the
    NetSurf C libs, libbearssl, libpw, libhttp, libaudio, zlib, …).  Each affected
    Makefile gained an `x86_64` arch branch alongside the existing aarch64/i386 ones:
    SSE stays **on** (the amd64 SysV ABI returns float/double in XMM, so the kernel's
    `-mno-sse` rule must *not* leak into the world), the musl arch headers come from
    `arch/x86_64`, and links use the `elf_x86_64` emulation + PIE (like aarch64).  The
    legacy native `libexec/ld` is skipped on x86_64 too (musl's `ld-musl-x86_64.so.1`
    is the runtime linker).  (The `bin/` programs are the next step.)

  - *Phase 5e — dynamic-linker boot path (PIE + ld-musl).* The x86_64 kernel now
    loads the real **dynamically-linked** world.  `kern/execfile.c` gained
    `load_dynamic`: it maps a PIE main executable at `DYN_MAIN_BASE`, reads + maps
    its `PT_INTERP` linker (`ld-musl-x86_64.so.1`) at `DYN_INTERP_BASE`, and builds
    the full SysV/auxv initial stack (`AT_PHDR/PHENT/PHNUM/BASE/ENTRY/RANDOM/PAGESZ`)
    the linker + `__libc_start_main` consume — a sibling of aarch64's loader, over
    the shared MI `elf64_load_at`.  A new `x86_64/vmm_layout.h` is the single source
    of truth for the user-VA regions (DYN_MAIN/INTERP/STACK + BRK/MMAP, 1 GB-aligned).
    The **UbixOS-native ABI** (`int $0x81`: MPI mailboxes, `ubix_getcwd`, used by
    `lib/ubix_api` and PID 1) is now wired: a vector-0x81 IDT gate + `isr_syscall81`
    stub + `x86_64_native_syscall` dispatching MPI/getcwd/klog directly and the rest
    through the native `systemCalls[]` table.  `bmake image TARGET=x86_64` builds a
    FAT-root world image (`mkimage-arm.sh` is now arch-parameterised); `kmain` hands
    off to `/bin/init` via the dynamic linker.  Verified: `/bin/init` loads through
    ld-musl, runs `main()`, and forks + execs the `/etc/init.d` services; a service
    binary loaded directly (no fork) links + runs cleanly.  (Known bug under
    investigation: a binary exec'd *after a fork* faults in ld-musl `sysv_lookup` —
    isolated to the fork→execve path; direct dynamic loads are unaffected.)
  - *Phase 5e — per-task TLS + portable frame access (fork/execve correctness).*
    Three fixes the dynamic world needs: (1) the MI ELF loader now reaches freshly
    allocated frames through a new `md_phys_to_virt` hook (identity on aarch64, the
    physmap/P2V on x86_64) instead of a bare `(void *)frame` cast — on x86_64 a frame
    above the low-1 GB identity window (once memory fills) was otherwise unreachable,
    silently corrupting the loaded image; (2) `switch_to` restores each task's
    `FS.base` (the syscall path uses no `swapgs`, so the TLS base is not saved per
    task by hardware — without this a task whose ld-musl reads TLS before its own
    `arch_prctl(SET_FS)` saw a stale pointer into another address space); (3) `fork`
    inherits the parent's `md_fsbase`/mmap/brk cursors (the child runs the parent's
    image until it `execve`s) and `execve` resets them for the fresh image.
  - *Phase 5e — file-backed mmap (the dynamic linker fully works).* `x86_64_user_mmap`
    now honours a file-backed mapping (MAP_ANON clear + a valid fd): it reads the
    file's bytes into each page instead of zero-filling, and the syscall entry passes
    the `fd`/`offset` args it previously dropped.  This is how ld-musl maps a shared
    library's segments — without it a library loaded after the main exe came up all
    zeroes (empty dynamic section → `sysv_lookup` faulted on a NULL hash table).  This
    was the bug that made a binary exec'd after a fork crash: a directly-loaded
    program reused ld-musl for `libc.so` and never file-mapped, while the exec'd one
    loaded it fresh.  Verified: `/bin/init` now forks + execs the `/etc/init.d`
    services with no faults — the dynamic linker is fully functional on x86_64.
    (Next: the console/tty + devfs/procfs layer so the services and `login` have a
    `/dev` and a controlling terminal.)
  - *Phase 5e — console + devfs/procfs: boots to an interactive login.* A COM1-backed
    VFS console (`dev/console_tty.c`: read/write fileops with a cooked line discipline
    — echo, CRLF, backspace) installs `g_console_ops` + the tty hooks; `serial_getc`/
    `serial_rx_ready` add COM1 input.  `kmain` registers + mounts devfs (`/dev`) and
    procfs (`/proc`), and `spawn_dynamic` wires a task's fds 0/1/2 to the console (init
    + its forked children inherit them).  `x86_64_native_syscall`/`x86_64_syscall` now
    honour musl's `NATIVE_FLAG` (0x8000) on the `syscall`-instruction path (e.g.
    `exit_group`).  Result: x86_64 boots `init` → the `/etc/init.d` services
    (automountd, logd, ubistry, authd, …) → a **base-profile text login** with a
    working `Login:`/`Password:` prompt over serial.  (Remaining: the login↔authd MPI
     round-trip times out — the last step before a shell.)
  - *Phase 5e — login authenticates (sched_yield was a no-op stub).* `sys_sched_yield`
    on x86_64 was a `return -1` stub in `arch/x86_64/kern/syscall_stubs.c` (the real
    `sys/kern/syscall.c` isn't compiled on x86_64 — it's full of i386-isms), so a
    userland `sched_yield()` did nothing.  The cooperative scheduler depends on it:
    `login` polling for `authd`'s MPI reply spun without ever handing the CPU to
    `authd`, so it always timed out ("Login incorrect").  Made the stub call the MI
    `sched_yield()`.  Now login → authd round-trips, **authenticates** (root/user),
    and execs the user's shell — the last gate to a shell is cleared. 
  - *Phase 5e — working shell (cwd init).* New x86_64 processes had an empty
    `oInfo.cwd`, so the VFS resolved a relative path (".") against "" — `ls`/`pwd`
    failed.  `spawn_dynamic` now sets PID 1's cwd to "/" (i386 does this in its exec)
    and `fork` inherits it.  **x86_64 now boots to a working interactive tcsh: it
    authenticates, runs commands (`echo`, `uname`), and lists directories (`ls /`).**
  - *Phase 5e — boots into the graphical desktop (views).* x86_64 now reaches the
    `views` compositor over a real framebuffer.  Since QEMU's virtio-gpu is
    modern-only (no legacy transport the bring-up virtio-pci speaks), the display is
    the **Bochs/std-VGA linear framebuffer** (`dev/vga_fb.c`: mode set via the DISPI
    I/O ports, LFB at PCI BAR0) — a live scanout like the i386 VESA LFB, and the
    right model for x86 PC hardware.  `dev/display.c` implements the native display
    syscalls: `sys_mapfb` maps the LFB's BAR into the compositor (wired +
    cache-disabled), `sys_fbpresent` is a no-op (live scanout), and `sys_shareregion`
    maps a window's shared backing buffer into a client across address spaces.  A
    `PTE_WIRED` bit + an MMIO check in `fork` share the framebuffer and shared
    buffers verbatim across a fork (the compositor forks vlogin) instead of
    COW-copying them.  Verified by a QMP `screendump`: views paints the full
    1024x768 desktop — grey-blue wallpaper, a centred login dialog, a taskbar.
    `bmake run TARGET=x86_64` now opens a graphical std-VGA window (serial on stdio);
    `run-debug-x86_64` is the headless serial-only variant.  (Input — keyboard/mouse
    — and the GUI terminal's pty are the next step.)
  - *Phase 5e — native int \$0x81 4th-arg fix + window sharing.* The native-ABI
    thunk (`int \$0x81`) leaves args in the C ABI, where the 4th arg is in **RCX**;
    `x86_64_native_syscall` was reading **R10** (correct only for the
    `syscall`-instruction POSIX path, which clobbers RCX).  So any native call with
    ≥4 args dropped its 4th — notably `sys_shareregion(dst, vaddr, size, out_vaddr)`,
    whose NULL `out_vaddr` left views handing vlogin a NULL window buffer ("failed to
    claim window").  Reading RCX fixes it: views now creates vlogin's window with a
    valid shared buffer (`shm=0x200023000`), keyboard input reaches the kernel
    (polled PS/2, `dev/input.c`), and the login dialog renders.  (Routing keys into
    vlogin's fields + the post-auth session are the remaining desktop polish.)

  - *Phase 5e — graphical login completes (fork fdtable + ring-0 fault containment).*
    The desktop now logs in end-to-end: typing `root`/`user` authenticates, the login
    dialog is dismissed, and the themed desktop (wallpaper + taskbar with the uBixOS
    menu, tray, and clock) comes up — with **no kernel faults**.  Two fixes: (1)
    `x86_64_fork` was copying the open-file table by *pointer* (a shallow `o_files[i]
    = parent[i]`), so a deep fork chain (views → vlogin → session) left the leaf
    process sharing `struct file` objects that an ancestor `kfree`d on exit — the
    dangling pointer then `#GP`'d on the next `read`/`close`.  It now calls the MI
    `fork_copy_fdtable` / `proc_fork_inherit_context` / `proc_fork_signal_init`
    helpers (`sys/kern/kern_fork.c`, added to the x86_64 build) exactly as i386 and
    aarch64 do, giving the child its own file copies.  (2) The ring-0 exception path
    now *contains* a fault taken while servicing a user process's syscall — it
    terminates the offending task and reschedules (the x86_64 analog of the ring-3
    `EL0→kill` containment) instead of halting the whole OS, so a single bad syscall
    can no longer freeze the desktop.  Verified headless (QMP send-key + screendump,
    `tools/x86_64-test/gui-login.py`): clean boot → auth → desktop, zero exceptions.

  - *Phase 5e — PS/2 mouse.* `dev/input.c` `sys_getmouse` was a stub; it now drives
    the 8042 aux channel.  `x86_64_mouse_init` (called at boot after the framebuffer)
    enables the aux device (`0xA8`), restores defaults (`0xF6`), and turns on
    streaming reports (`0xF4`); `sys_getmouse` polls the controller, assembles the
    standard 3-byte PS/2 packet (resyncing on the always-set bit 3, dropping overflow
    packets), and cooks it into the `mouse_event` the compositor expects — dx, a
    *negated* dy (PS/2 reports +Y up, the screen grows +Y down), and the L/R/M button
    bitmask.  Keyboard and mouse share the output buffer, so each reader consumes only
    its own bytes (status bit 5 = aux).  Verified headless (`gui-login.py` now drives
    QMP `input-send-event` rel-moves + a click): the cursor tracks both axes in the
    correct direction.

  - *Phase 5e — graphical terminal (pty/tty engine).* The desktop's **Terminal**
    (`bin/views/term`) now runs an interactive shell on a kernel pseudo-terminal.
    The arch-neutral VT100 line-discipline + pty pool (`sys/posix/tty.c`) was added
    to the x86_64 build (it had an i386/aarch64 `#if`; added an x86_64 branch — its
    own `pushfq`/`popfq` IRQ save-restore + the `<x86_64/signal.h>` signal numbers,
    and the no-op `rs232_putc`/`backSpace` echo shims it shares with aarch64).  A new
    `arch/x86_64/dev/pty.c` provides the `sys_settty` + `sys_pty{alloc,free,inject,
    snap,resize}` syscalls (thin wrappers over the engine, replacing the stubs), and
    `tty_init()` runs at boot.  One userland fix was needed: `lib/ubix_api`'s
    `ubix_getcwd` (the `int $0x81` thunk the shell calls for its prompt) had only an
    i386 asm body under `#else`, which on x86_64 assembled `movl 4(%esp),%eax` with a
    `0x67` address-size override — truncating the 64-bit stack pointer to 32 bits and
    faulting; added an x86_64 branch reading `buf` from `%rdi` per the SysV ABI.  Also
    hardened: the ring-0 fault dump now prints the faulting `pid`.  Verified headless
    (`tools/x86_64-test/gui-term.py` drives the mouse through the start menu to launch
    Terminal, then types `ls`): the shell prompt renders and `ls` lists the root —
    full keyboard → `sys_ptyinject` → shell → VT100 → render round-trip, zero faults.

  - *Phase 5e — execve passes argv/envp (real login shell).* `sys_execve` no longer
    drops the caller's arguments + environment: it now copies the user `argv`/`envp`
    vectors out of the old address space (a new `copy_user_strvec`, before the CR3
    switch invalidates them) and marshals them onto the new image's SysV stack, and
    sets the task name/cmdline via the MI `exec_set_name_cmdline` (`kern_exec.c`, now
    in the x86_64 build) like i386/aarch64.  Visible effect: the desktop Terminal now
    runs the user's real shell as a **login shell** — term's `getenv("SHELL")`
    resolves (so it launches `tcsh`, not the `/bin/shell` fallback), tcsh gets
    `argv0="-tcsh"` + `HOME`/`TERM`, sources its login files, and `ls` works; forked
    services show their real names in traces instead of blank.

  - *Phase 5e — preemptive scheduling.* The PIT timer (IRQ0) now preempts a running
    **user** task: `x86_64_irq` reports a timer tick back to the IRQ dispatcher, and
    if it interrupted ring 3 (`CS.RPL == 3`) the handler delivers any pending signal
    (so Ctrl-C reaches a CPU-bound foreground program) and calls `sched()` to pick
    the next runnable task.  The kernel itself stays **non-preemptive** (mirrors
    aarch64's EL0-only reschedule): an in-kernel context — a driver polling a virtio
    ring at boot, a service mid-syscall — runs to its next voluntary `sched_yield()`/
    block rather than being preempted mid-operation.  Before this, a user program
    that never yielded (e.g. a `while(1)` loop) starved the whole desktop; now it is
    time-sliced.  Verified headless (`tools/x86_64-test/gui-preempt.py`): with tcsh
    spinning in a pure-CPU `while(1)` loop, the taskbar clock keeps advancing
    (19:00:17 → :20 over 4 s), proving the CPU-bound task is being preempted.

  **With mmap/execve/fork/signals in place, the x86_64 kernel can now load, run,
  fork, and signal real on-disk binaries — the full runtime a userland needs.**
  `bmake TARGET=x86_64` builds the bring-up kernel only (the x86_64 userland/world is
  a later phase); `bmake run TARGET=x86_64` boots it (serial console).
  Grows by widening the i386 MD code to 64-bit.  See `docs/design/cross-arch-plan.md`.
- **aarch64 SMP bring-up (smp-plan M0–M3)** — secondary cores now run on aarch64.
  M0: per-CPU state via `TPIDR_EL1` → `struct pcpu` (`_current` is per-CPU). M1:
  start the APs via PSCI `CPU_ON` + a secondary entry that enables its MMU on the
  shared page tables. M2: per-CPU GICv2 CPU interface + CNTV timer (each AP takes
  its own 100 Hz tick). M3: AP scheduler entry — an AP adopts a per-CPU idle and
  pulls tasks from the shared run queue. M3 is **opt-in** (`AARCH64_SMP_ENABLE_APS`,
  default off) pending the M4 true-SMP hardening. See `docs/design/aarch64-smp-plan.md`.
- **Activity Monitor — per-core CPU graphs** — the Performance tab detects the CPU
  count and draws a graph per logical processor, with a "View: Overall / Per core"
  toggle (Windows Task Manager style). Backed by per-core CPU accounting
  (`sched_core.c`) exposed as Linux-style `cpu0`/`cpu1`/… lines in `/proc/stat`.

### Changed
- **aarch64 spinlock is now a real atomic lock** — `spinLock`/`spinUnlock`/
  `spinTryLock` were uniprocessor stubs (just set a flag); reimplemented with LL/SC
  (`ldaxr`/`stxr` + store-release), the prerequisite for SMP and strictly more
  correct for the existing locks. Uniprocessor is unchanged (one uncontended op).
- **Idle thread is per-CPU** — pinned at `g_pcpu[cpu].idle` and dispatched
  out-of-band when a CPU has nothing ready, instead of sitting in the shared run
  queue (which cannot scale to SMP). i386 BSP + aarch64 APs; uniprocessor unchanged.
- **CPU busy/idle accounting is per-core** — was a single global pair.

### Fixed
- **Concurrent VFS I/O corruption under SMP** — the FS read/write paths (the UbixFS
  pool reader) are not re-entrant, so two CPUs in `fread`/`fwrite`/the `fopen`
  lookup corrupted shared reader state (symptom: a short read during exec →
  "not loadable"). Serialised across CPUs with a coarse `vfs_io_lock`
  (`sys/fs/vfs/file.c`); uniprocessor is uncontended, so effectively free there.

## [2.4.0-BETA] - 2026-06-14

### Added
- **Kernel threads — per-thread TLS (Task C)** — `set_thread_area` now records each
  thread's `%gs` base in `kTask_t.tls_base`, and `cpu_switch` re-installs it into the
  shared LDT[1] descriptor on every context switch (right after the CR3 swap). All
  threads in an address space share that one LDT slot, so each resume must restore
  the running thread's own base. `bin/tlstest` verifies two threads keep independent
  TLS across switches.
- **Kernel threads — `futex` syscall (Task D)** — `sys_futex` on the kernel's
  `wait_chan` sleep/wake primitive: FUTEX_WAIT/WAKE (+WAIT_BITSET; REQUEUE treated as
  WAKE). Threads share the address space, so a user virtual address is the wait token.
  Backs musl's mutex/cond/sem/barrier/once. `bin/futextest` verifies a worker blocks
  in FUTEX_WAIT until FUTEX_WAKE.
- **Kernel threads — musl pthreads (Task E, milestone 1)** — real `pthread_create` /
  `pthread_mutex` / `pthread_join` work. `sys_rfork` now honours the three CLONE bits
  musl needs: SETTLS (installs the child's `%gs` base), PARENT_SETTID (writes the new
  tid to `*ptid`), CHILD_CLEARTID (`endTask` zeroes + futex-wakes the address, releasing
  musl's thread-list lock held across `pthread_exit`). `clone.s` passes ptid/ctid.
  `bin/pthreadtest` (4 threads, mutex-guarded counter, joins) verifies it. (Deferred:
  detached-thread `__unmapself` needs a kernel-assisted unmap+exit; `membarrier`.)
- **Kernel threads — detached threads (Task E2b)** — native `thread_exit_unmap`
  syscall (unmaps the exiting detached thread's own stack and terminates it in one
  kernel trap; a userspace munmap-then-exit is impossible under the stack-arg ABI).
  musl's `__unmapself` now calls it. `bin/detachtest` (5 waves of detached threads)
  and `bin/condtest` (pthread_cond producer/consumer) verify E2.
- **`membarrier` syscall** — uniprocessor no-op at the real FreeBSD number (POSIX 584;
  musl's Linux #375 remapped to it). POSIX table extended through 584. Keeps a real
  FreeBSD call on the FreeBSD-numbered table (native ABI is only for Linux-only calls).
- **UbixOS-native ABI for Linux-compat primitives** — `futex`, `set_thread_area`, and
  `exit_group` (Linux calls with no FreeBSD syscall number) moved from the
  FreeBSD-numbered POSIX table (`int $0x80`) to the UbixOS-native table (`int $0x81`,
  slots 64/63/65). musl flags these with bit `0x8000` on the syscall number; the i386
  shims dispatch them to `int $0x81`. POSIX slots 350/351/352 restored to faithful
  FreeBSD `Invalid` (`__acl_*`), keeping the POSIX table a clean FreeBSD ABI for a
  future FreeBSD-libc port. `clone` stays at POSIX 251 = real FreeBSD `rfork`.
- **objGFX UI primitives** — `ogFillRoundRect` / `ogRoundRect` (filled + outline
  rounded rectangles), `ogDropShadow` (soft quadratic-falloff shadow around a
  rect), and `ogBlendColor` (packed-RGB lerp).  Non-virtual additions, so existing
  app binaries stay ABI-compatible.
- **SMP — i386 application-processor bring-up (smp-plan Phase 3, opt-in)** — per-CPU
  GDT + TSS + `%gs`, a per-CPU idle thread dispatched out-of-band (never enqueued in
  the shared run queue), a per-CPU LAPIC timer driving the scheduler, and an AP
  scheduler entry (`c_ap_boot`) that joins the run queue.  Gated behind
  `SMP_ENABLE_APS` (default **off**): released APs boot cleanly, but the kernel still
  carries uniprocessor assumptions that corrupt under multi-core *load* (single
  global lazy-FPU owner, no cross-CPU TLB shootdown), so the default ships the proven
  single-core desktop.  Finishing true SMP (per-CPU FPU + TLB shootdown) is a 3.x task.

### Changed
- **Idle thread is now per-CPU (i386)** — the idle thread is pinned per CPU
  (`g_pcpu[id].idle`) and dispatched by the scheduler when nothing else is ready,
  instead of sitting in the shared run queue at `QOS_IDLE` (which cannot scale to
  SMP — two CPUs would dequeue the same idle task).  Uniprocessor behaviour is
  unchanged; aarch64 keeps the enqueued idle.
- **Modern login screen (vlogin)** — calm-slate rounded card with a soft drop
  shadow (drawn with the new objGFX primitives), `uBixOS`/`Sign in` header, and
  boxed username/password fields that light up with an accent underline + caret on
  focus, replacing the old navy panel and block-cursor rows.
- **views** — the compositor + window chrome now use `ogSurface::ogBlendColor`
  instead of a local `decor_blend`, so the focus-dim / shadow / corner-AA blend has
  a single shared implementation (output unchanged).
- **Active-window highlight** — the compositor sends `DISPLAY_FOCUS` (via a single
  `WindowManager::set_focus()` routing all focus changes incl. click-to-focus), so
  the taskbar highlights the active window's tab; deduped and sent after the claim
  ACK to avoid the handshake race.

### Fixed
- `fork` no longer COW-marks the per-process LDT page (userland TLS descriptors):
  it is CPU state, never shared, so the child gets a private writable copy and the
  parent stays writable. COW left it read-only, so the kernel's LDT[1] update in
  `cpu_switch` faulted with interrupts off.
- **Cold-boot triple fault (i386)** — `pcpu_gdt_tss_load()` seeded the per-CPU TSS by
  copying the static `0x4200` TSS, but it runs as the first thing in `kmain()`,
  *before* `idt_init()` populates `0x4200` with `ss0 = 0x10`. On a cold boot the copy
  grabbed a null `ss0` (kernel-stack selector), so the first userland timer tick
  faulted #TS → #DF → triple fault → reboot. A warm reset hid it because guest RAM
  kept the previous boot's value at `0x4200`. `ss0`/`io_map` are now seeded
  explicitly — this was the long-standing "reboots on the first boot, works on the
  second".
- **Cold-boot pool-read races (i386)** — `execFile()` (PID 1 launch) and the UbixFS
  root mount now retry on the transient first-read miss after boot. The pool's
  dataset read could report "no 'root' filesystem dataset" and drop the boot to the
  FAT fallback (which has no `/bin/init`, so exec then failed). Same defensive retry
  as the `sys_exec` fopen path.

---

## [2.3.0-BETA] - 2026-06-06

### Added
- **Kernel threads** — `rfork(RFMEM)` / `clone` foundation: tasks in a thread
  group (`tgid`) share one address space (cr3); `endTask` frees the address space
  only when the last task of a group exits (`reap_free_as`). musl `clone.s` for
  i386. (v1; TLS, futexes, and full pthread wiring to follow.)
- **NetSurf web browser** — renders real web pages in-OS as part of `bmake world`
  (`tools/build-netsurf.sh`, needs host bison ≥ 3 + libpng):
  - **JavaScript** via Duktape with nsgenbind-generated DOM bindings.
  - PNG/JPEG image decode via vendored `stb_image` (replaces libpng/libjpeg).
  - Scalable antialiased fonts (`font_stbtt` backend) with bold/italic/serif faces
    and lazy face loading.
  - Mouse/keyboard input through views (opt-in pointer-motion delivery).
  - `libnsfb` objGFX surface backend; vendored core libs (libnsutils, libutf8proc,
    libwapcaplet, …); per-process mailbox so concurrent browsers don't collide.
  - Listed in the start menu under Applications.
- **macOS-style desktop environment**:
  - **ubistry** — hierarchical persistent registry + client lib + `ulog`; seed
    generator installs defaults to `/var/db`.
  - **Settings app** — sidebar layout with Desktop, Sound, General overview, and
    About panes (system info from `uname`/`sysinfo`).
  - Desktop wallpaper from the registry (image / solid / jailbars) with an RGB
    picker and preview thumbnail; per-user wallpaper + accent-theme layering
    (tropical/synthwave/Miami defaults); default `ubix.bmp` wallpaper.
  - Data-driven cascading start menu (start icon, date clock, footer) sourced from
    the registry; About action wired to the Settings About pane.
- **busybox 1.36.1 userland** — replaces UbixOS stub utilities and adds a broad
  command set: `cat`, `ls`, `uname`, `stat`, `grep`, `find`, `less`, `wc`, `head`,
  `tail`, `sort`, `cut`, `tr`, `uniq`, `more`, `cp`, `mkdir`, `rm`, `mv`, `touch`,
  `sed`, `env`, `sleep`, `date`, `basename`, `dirname`, `which`, `top`, `ps`, and
  the **vi** editor (with a working save path via `ftruncate` + `/tmp`).
- **Scalable antialiased TrueType fonts** — `objgfx` `ogScalableFont` (stb_truetype)
  used across views window titles, taskbar, Settings, login, and a damage-tracked
  antialiased monospace font in the terminal.
- **Virtual memory**:
  - RB-tree VMA tracking (Phase 1) + lazy anonymous allocation (Phase 2.1).
  - Demand-paged file-backed `mmap` with `MAP_FIXED` overlap trimming (Phase 2.2);
    read-only library pages shared.
  - `msync` writeback for `MAP_SHARED` writable file mappings (Phase 2.3).
  - `madvise` (`MADV_DONTNEED`/`MADV_FREE` + hint no-ops) (Phase 3.3).
  - Pageout daemon (Phase 3.2).
  - Shared-region refcounting so freed frames are reclaimed, not leaked.
  - i386 address-space layout extracted into `machine/vmm_layout.h` (cross-arch).
- **procfs** — `/proc/meminfo` (total/free physical pages, file-cache count).
- **Pseudo-terminals & TTY** — pty pool backing graphical terminals; the GUI term
  runs the user's interactive shell on a pty (tcsh login shell, macOS-style);
  VT100 engine parameterized on per-TTY `t_cols`/`t_rows` with live resize +
  `SIGWINCH`; `IL`/`DL`/`ICH` CSI sequences; Ctrl-C in the GUI term.
- **Crypto / TLS** — vendored **BearSSL**; `libpw` PBKDF2-HMAC-SHA256; kernel
  ChaCha20 CSPRNG (`getrandom(2)` + `/dev/urandom`); `libhttp` HTTP/1.0 + HTTPS
  client and `httpsget`/`udptest`/`ping` tools; `authd` hashes passwords with
  `libpw` instead of plaintext compare.
- **Networking** — user-configurable network (DHCP/static) from Settings via
  ubistry + `net_configure` syscall; NE2000 (RTL8029) rewritten on newbus + lwIP;
  real sleep/wakeup with IRQ-driven e1000; one-shot callout timer subsystem;
  `close()` on a socket fd tears down the lwIP netconn.
- **SMP groundwork** — build the SMP objects; map the LAPIC and register the BSP;
  bring up application processors; per-CPU state (`struct pcpu`, `curcpu()`); APs
  adopt the kernel address space and run a per-CPU idle loop; per-CPU GDT segment
  with `%gs` (`SEL_PCPU`); true spinning lock (`spinLockIrq`/`spinUnlockIrq`).
- **Software task switching** — `md_kstack` + initial-frame builder, `switch_to` /
  `cpu_switch` (saves/restores segment registers), replacing hardware task gates.
- **Audio** — AC97 master volume + mute mixer ioctls; persisted via `/aural` keys +
  `sndcfg` boot applier; Sound settings pane.
- **Build system** — musl shared library (`libc.so`) + `libobjgfx.so` with dynamic
  linking for views/taskbar/term/vlogin; `TARGET_ARCH` abstraction lifting i386
  ISA flags into a target makefile; GitHub Actions Linux CI producing
  `ubixos.img`; centralized platform detection.
- **Syscalls** — `sysinfo` (slot 62); `net_configure`; `rename` (via
  `fat_dir_rename`); `utimensat`; `ftruncate`; `gettimeofday` returns real wall
  clock; `mkdir -p` (sysMkDir rewrite + umask stub).
- **Session teardown** — `views` reaps windows whose client process has died
  (frees the shared buffer, removes the ghost frame); `vlogin` tears down the
  session process group on logout (`sys_kill` gains POSIX process-group
  semantics); pty hangup on owner death — when a graphical terminal app exits,
  `tty_hangup_by_owner()` SIGHUPs the slave session so a shell that ran `setsid()`
  for its pty (in its own session/group) is not left running across the next
  login; a read on a released pty returns EOF.  Together these cut the per-logout
  memory leak by ~76%.
- **procfs `/proc/meminfo` `OrphanPages`** — count of in-use physical pages owned
  by PIDs that no longer exist; a value that climbs across logout/login cycles
  flags a process-teardown leak (the per-dead-PID breakdown is logged to serial
  only when orphans exist).
- Reference app `bin/hello` + `docs/apps/` guide for views developers;
  `bin/vmtest` standalone heap + FAT + stb_truetype self-test.

### Changed
- **views — modern look & feel** — Windows 11-flat window chrome; window depth
  (shadows, rounded corners, taller title bars); brighter/larger window-control
  glyphs; flat modern taskbar + start menu; hover feedback + volume tray;
  compositor caches the desktop and repaints only the dragged/damaged region;
  window minimize-to-taskbar, maximize/snap (button, edge-snap, double-click),
  and resizable windows; VESA prefers a 32bpp LFB mode for faster present.
- **Source tree restructure** — `sys/kernel/` split into `sys/kern/`,
  `sys/posix/`, `sys/exec/`; `smp.c` relocated to `sys/arch/i386/`; display-stack
  binaries grouped under `bin/views/`; busybox restructured to upstream layout.
- **Style / tooling** — project-wide clang-format (120-col) + expanded clang-tidy
  (clang-analyzer, bugprone, naming, internal-linkage); snake_case file/function
  renames; `g_` prefix on file-scope globals; braces on all single-statement
  bodies; BSD `u_int*_t` as the canonical unsigned types; mandatory Doxygen doc
  blocks; `bzero` → `memset`.
- `gettimeofday`/time — real wall clock instead of uptime; cert-time validation in
  `httpsget`.

### Fixed
- Kernel: pipe refcount + EOF handling (no more `0xBEBEBEBE`); actually close
  `fd < 3` and allocate from the lowest free slot; `kern_openat` returns negative
  errno (musl ABI); POSIX errno-sign + dir-stat + MPI mailbox cleanup (NetSurf
  runtime gates); FPU corruption from the per-CPU `_current` macro under SMP.
- `execve` now resets caught signals to `SIG_DFL` (POSIX): the old handler
  addresses point into the torn-down address space, so a process that inherited a
  caught signal across exec would jump to a wild pointer on its next signal (the
  cause of the first kernel-threads test crash — a child's `SIGCHLD` delivered to
  a stale inherited handler). `SIG_IGN` dispositions, the signal mask, and pending
  signals are preserved.
- VMM: physical use-after-free of shared regions on owner `free()`; segfault report
  now names the VMA / backing file holding `eip`.
- FAT: keep `cur_cluster` consistent after a boundary-ending read **and** write
  (random-access fix); `open(O_RDWR)` no longer truncates an existing file;
  `ftruncate` actually shrinks the cluster chain; harden basename parsing +
  `EEXIST` on existing dir; `sys_rename`; `utimensat` + unlink return propagation.
- `objgfx`: harden `ogScalableFont` glyph blit against corrupted cache entries.
- musl: zero `O_LARGEFILE` on i386 (collided with FreeBSD `O_NOCTTY`).
- login: fix password hang, add `*` feedback, unify Enter as CR.
- Boot/build: `bmake world` from a clean checkout; `ttyd`/`sys_wait4`/keymap boot
  stall + backspace.

---

## [2.2.0-BETA] - 2026-05-24

### Added
- `contrib/libcxxabi/` — self-contained minimal Itanium C++ ABI (`cxxabi.cc`): `new`/`delete`, construction guards, pure/deleted virtual, `__dso_handle`. Builds `build/lib/libcxxabi.a`. (Phase 5)
- `contrib/libcxx/` — LLVM libc++ 18.1.8 subset: `<string>`, `<vector>`, `<map>`, `<memory>`, `<algorithm>`, `<any>`, `<optional>`, `<variant>`, charconv/ryu. Builds `build/lib/libcxx.a`. Hand-written `__config_site` + `__assertion_handler`; GCC-16 `__decay` built-in patch applied. (Phase 6)
- `share/mk/ubix.musl.cxx.prog.mk` — BSD make snippet for C++ programs using STL: sets `-std=c++20 -nostdinc -nostdinc++ -fno-rtti -fno-exceptions`, wires libcxx/libcxxabi/musl include paths and link group.
- `include/ubix/mailbox.hh` — C++ RAII wrapper for MPI mailboxes (`ubix::Mailbox`); `owned_` flag prevents destructor from destroying non-owning instances. `ubix::post_message` free functions.
- `include/ubix/sched.hh` — `ubix::yield()` and `ubix::pid()` thin wrappers over `sched_yield`/`getpid`.
- `include/ubix/process.hh` — `ubix::Shell` RAII class: encapsulates `fork`, `pipe`, `dup2`, `execve`, `kill`, `fcntl` for shell subprocess management. Defines `_POSIX_SOURCE` so musl exposes `kill()`.
- `include/views/display.hh` — single include that gates all C display-protocol headers (`sys/mouse.h`, `sys/kbd.h`, `views/display_proto.h`) behind `extern "C"`.

### Changed
- `bin/views/views.cc` — migrated to STL: `std::vector<Window>`, `std::string` title, `std::aligned_alloc` for page-aligned shared buffers (replaces `malloc` + manual alignment + `void *raw` field). All C stdlib includes replaced with `<cstdio>`, `<cstdlib>`, `<cstring>` via wrapper headers. `ubix::Mailbox` replaces raw `mpi_createMbox`/`mpi_fetchMessage` calls; `ubix::yield()` replaces `sched_yield()`.
- `bin/taskbar/taskbar.cc` — migrated to STL: `static ubix::Mailbox g_tb_mbox` replaces scattered `static char tb_mbox[]` locals; `ubix::post_message` replaces `mpi_postMessage`; `ubix::yield()`/`ubix::pid()` replace raw calls; `std::strncpy`/`std::strlen`/`std::printf` replace bare C names.
- `bin/term/term.cc` — migrated to STL: ring-buffer globals replaced with `std::vector<std::string> g_lines`; `g_shell_in`/`g_shell_out`/`g_shell_pid` + `shell_spawn()` replaced with `static ubix::Shell g_shell`; `ubix::Mailbox g_mbox` replaces raw MPI calls; `msg = {}` replaces `memset`.
- `include/sys/mpi.h` — all mailbox-name parameters changed from `char *` to `const char *` for const-correctness.

### Removed
- `lib/libcpp/` — minimal hand-rolled C++ ABI shim (`libcpp.cc`, `libcpp.h`, `Makefile`) retired; replaced by `contrib/libcxxabi/`.

### Added
- **O(1) scheduler — Phase 3 (QoS / priority aging)**:
  - QoS classes: `SCHED_CLASS_RT`, `SCHED_CLASS_INTERACTIVE`, `SCHED_CLASS_NORMAL`, `SCHED_CLASS_BATCH`; each maps to a priority band in the run-queue bitmap.
  - I/O completion boost: tasks unblocked from I/O wait receive a transient priority increase.
  - CPU decay: long-running CPU-bound tasks accumulate a penalty that shifts them down one QoS band.
  - Starvation aging: tasks not scheduled for `AGING_THRESHOLD` ticks are promoted one band to prevent starvation.
  - `sched_io_wakeup` properly dequeues and re-enqueues the task when boosting from a READY state, preserving run-queue bitmap consistency.
- **POSIX signals — Phases 1 + 2**:
  - `sigaction`, `sigprocmask`, `sigpending`, `sigsuspend` syscalls.
  - Signal disposition table per task; `SA_RESTART`, `SA_SIGINFO` flags honoured.
  - `STOPPED` task state; `SIGSTOP` and `SIGCONT` default actions pause and resume tasks.
  - `SIGTTIN` stops background processes that attempt terminal reads.
  - ZOMBIE two-phase exit: dying task transitions to `ZOMBIE` state, notifies parent with `SIGCHLD`, parent reaps via `wait4`.
- **procfs `/proc/mounts`** — global file at `/proc/mounts`; walks `systemVitals->mountPoints` linked list and emits one line per mount point in Linux `mountinfo` format: `device mountpoint fstype perms 0 0`. `procfs_fstype_name()` maps `VFS_TYPE_*` constants to human-readable names.
- **`bin/mount` rewrite** — no-argument invocation reads `/proc/mounts` and displays mounted filesystems as `device on mountpoint type fstype (perms)`, matching BSD `mount(8)` output.
- `FD_TYPE_*` constants (`FILE=1`, `SOCKET=2`, `PIPE=3`, `DIR=4`, `TTY=5`, `TTYV=6`) moved to canonical home `sys/include/sys/descrip.h`; all magic integers removed from `descrip.c`.
- `VFS_TYPE_*` constants (`DEVFS=0x01`, `PROCFS=0x02`, `FAT=0xFA`, `UFS=0xAA`) added to `sys/include/fs/vfs/vfs.h`.

### Fixed
- `sys/arch/i386/fork.c` — `fork()` now propagates pipe file descriptors with the correct `fd_type` (`FD_TYPE_PIPE`); previously all inherited fds defaulted to `FD_TYPE_FILE`, causing pipe reads to go through the VFS path and block forever.
- `bin/taskbar/taskbar.cc` — helper loop now retries on `EINTR` instead of treating a signal-interrupted MPI wait as an error.
- Ring-0 kernel stack enlarged from 4096 to 8192 bytes; FAT `chdir` deep-path handling was overflowing a 4096-byte stack and corrupting `sig_pending`.

### Changed
- **sys/include/ audit** — copyright years updated to 2026; CVS `$Log` blocks stripped from 15 headers; duplicate `typedef` definitions removed (e.g. `suseconds_t` in `ubixos/time.h`, `mode_t` in `sys/descrip.h`); `timeMake` return type corrected to `uint32_t`; `AT_FDCWD` guarded in both `sys/fcntl.h` and `fs/vfs/stat.h`; `AT_*` auxv constants deduplicated in `i386/elf.h` (shared block after architecture `#endif`); `register_t`/`PAD_` macros in `sys/sysproto_posix.h` guarded against redefinition when included after `sys/sysproto.h`.
- `sys/kernel/time.c` — `tz_minuteswest` set to `0` (UTC); no timezone database is present in the kernel.
- `sys/kernel/descrip.c` — duplicate includes removed; `ioctl` default branch uses `VFS_TYPE_DEVFS` named constant.

---

## [2.1.0-BETA] - 2026-05-13

### Added
- Full composited window system (`bin/views/` C++ rewrite): `Framebuffer`, `Window`, `WindowManager` classes; server-side decorations with title bar, close button, drag; Z-order; focus-follows-click.
- `bin/taskbar/taskbar.cc` — taskbar ported to C++ with `ogSurface`/`ogBitFont`; flyout launcher menu; clock.
- `bin/term/term.cc` — windowed VT100 terminal emulator using `ogSurface` and `ogBitFont`.
- `include/objgfx/` — canonical public location for objgfx headers (moved from `lib/objgfx/objgfx/`).
- `include/views/display_proto.h` — MPI display protocol: `DISPLAY_CLOSE`, `DISPLAY_QUERY`/`DISPLAY_INFO`, `no_decor` flag, `DECOR_H` constant.
- `tools/*.DPF` — all bitmap font files consolidated into `tools/` (were split between `tools/` and `lib/objgfx40/`).
- `docs/architecture/display.md` — updated architecture document reflecting completed display stack.
- `docs/design/display-plan.md` — Phase 10 (C++ refactor) fully documented and marked complete.

### Removed
- `bin/launcher/` — dead pre-2019 application, never integrated with MPI display stack.
- `lib/objgfx40/` — older objgfx fork; superseded by `lib/objgfx/`.
- `lib/views/sunlight/` — widget toolkit tied to objgfx40; unused since launcher removal.
- `lib/libfb/` — removed from world build; no longer a public library. Pixel primitives absorbed into `bin/views` `Framebuffer` class.
- `sys/sde/` — kernel-side Software Display Environment retired; replaced by userland compositor.
- `sys/include/sde/` — SDE kernel headers removed.
- `sys/lib/ogprintf.cc` — retired kernel graphics printf; use `kprintf` for serial/VGA debug output.

### Changed
- `bin/views/views.cc` replaces `views.c`: C++ compositor with native pixel ops, no libfb dependency.
- `bin/muffin/main.cc` replaces `main.c`: ported to C++ with `ogSurface`.
- `lib/objgfx/Makefile` — now includes from `../../include` (canonical path) instead of `./objgfx`.
- `tools/mkimage.sh` — font copy path updated; `lib/objgfx40` reference removed.

---

## [2.0.1-BETA] - 2026-05-13

### Added
- `bin/ed/` — POSIX `ed` line editor: supports all standard address forms (`.`, `$`, `n`, `m,n`, `/re/`, `%`) and commands `p`, `n`, `l`, `=`, `a`, `i`, `c`, `d`, `j`, `m`, `s`, `e`, `E`, `r`, `w`, `f`, `q`, `Q`. Reads files via `fread` (robust against kernel `fgetc` EOF quirks). Built and linked against `ubix_api`.
- `bin/ed/README.md` — usage guide: address syntax, command reference, limits, and worked examples.
- `sys/kernel/syscalls.c` slot 42 — `sys_ttyctrl(cmd, val)`: kernel syscall to set TTY raw/echo mode per-terminal.
- `lib/ubix_api/ttyctrl.c` — `tty_setraw(val)` / `tty_setecho(val)` userland API wrappers using native `int $0x81` syscall 42.
- `include/api/ubix.h` — declarations for `tty_setraw` and `tty_setecho`.

### Fixed
- `sys/fs/vfs/file.c` (`fgetc`) — returned 0 at EOF instead of -1; `fgets` loops never terminated on short files. Now checks `vfsRead` return value and returns -1 on EOF (BUG-VFS-02).
- `sys/isa/atkbd.c` — keyboard ISR now implements a full TTY line discipline: canonical mode (default) buffers input in `t_linebuf`, echoes characters and backspace if `t_echo=1`, and delivers the complete line to `stdin[]` on Enter; raw mode delivers each keypress immediately with no echo. This fixes missing echo in `fgets`-based programs (e.g. `ed`) without breaking password masking in `login`.
- `sys/kernel/vfs_calls.c` (`sys_read` stdin path) — removed its own per-character echo loop; echo is now owned exclusively by the keyboard ISR line discipline to prevent double-echo.
- `lib/libc/stdio/gets.c` — removed per-character `printf` echo; the ISR line discipline handles it.
- `bin/login/main.c` (`pgets`) — updated to use `tty_setraw(1)` / `tty_setraw(0)` around password input so the ISR delivers raw chars for `*`-masking without line buffering.

### Added
- `sys/include/ubixos/tty.h` — `TTY_SETRAW` / `TTY_SETECHO` constants; `t_linebuf[512]`, `t_linelen`, `t_echo`, `t_raw` fields added to `tty_term` for the line discipline.
- `sys/include/sys/sysproto.h` — `sys_ttyctrl_args` struct and `sys_ttyctrl` prototype.

### Added
- `bin/muffin/` — new C++ GUI application using `lib/objgfx`; renders a background BMP and coloured rectangles via the SDE.
- `lib/objgfx/` — ported to bare-metal: removed all STL dependencies (`std::map`, `std::function`, `std::iostream`, `std::fstream`); replaced with plain C function pointers and POSIX I/O.
- `lib/libc/sys/lseek.S` — `lseek(int, off_t, int)` userland stub (syscall 478).
- `lib/libc/math/fabs.c` — `fabs(double)` implementation.
- `sys/sde/main.cc` — SDE kernel thread now calls `mpi_createMbox("sde")` once initialised, providing a named ready-signal that userland can probe.
- `bin/muffin/main.cc` — `sde_ensure_running()` probes the `"sde"` MPI mailbox; if absent, sends `sdeStart` to `"system"` and spins until the SDE is ready before registering a window.
- `sys/arch/i386/sched.c` — `sched_killTree(pid)`: kills a task and all its descendants, used by the Ctrl-C handler.

### Fixed
- `include/math.h` — wrapped all declarations in `extern "C"` so C++ translation units link against the unmangled `fabs` symbol.
- `include/stddef.h` — `NULL` now defined as `0` (not `(void*)0`) in C++ mode.
- `lib/objgfx/objgfx/ogTypes.h` — removed unused `#include <map>`; added `#include <sys/types.h>` for standalone inclusion.
- `contrib/tcc/ubixos_shim/syscalls.S` — removed duplicate `lseek` definition now that `lib/libc/sys/lseek.S` provides it.
- `lib/objgfx40/`, `lib/views/sunlight/` — output redirected to `build/obj/gfx/` so C++ objects do not pollute the `obj/lib/*/*.o` glob linked into plain C binaries.
- `tools/mkimage.sh`, `Makefile` — `sys/sde/assets/ubix.bmp` now installed as `/var/background/ubix.bmp` in both `bmake image` and `bmake install-world`.
- `bin/make/make.c` — Makefile detection replaced `access()` (kernel stub always returning 0) with direct `fopen` probes; shell path corrected from `sys:/bin/sh` to `sys:/bin/shell`.
- `sys/isa/atkbd.c` — Ctrl-C handler now calls `sched_killTree` instead of single-process `sched_setStatus(DEAD)`, so forked recipe children are also killed.
- `sys/arch/i386/fork.c` — `fork()` transfers `term->owner` to the child when the parent owns the terminal, so `tty_foreground->owner` tracks the actual foreground process through the `login → shell → app` chain.
- `sys/arch/i386/sched.c` — scheduler DEAD handler now returns `term->owner` to the parent when a process dies via `sched_setStatus(DEAD)`, matching the handback that `endtask()` performs for normal exits.

### Fixed
- `sys/vmm/vmm_memory.c` (`vmm_freeProcessPages`) — double-decrement of COW counters caused "COW less than 0" crash on any process run after a daemonizing `fork()`+`exit()` (e.g. `ubistry`). `endTask` already decrements COW counters via `vmm_cleanVirtualSpace`; `vmm_freeProcessPages` was then scanning by `pid` and decrementing the same pages again, freeing physical pages still mapped by the surviving daemon. Removed the `adjustCowCounter(-1)` call for COW pages; surviving mappers free the physical page through their own cleanup (BUG-COW-07).
- `sys/arch/i386/systemtask.c` (`systemTask`) — free `kTask_t.kernelStack` before `kfree(tmpTask)` to stop leaking 4 KB per task exit; NULL guard with `kprintf` warning if the pointer is unexpectedly NULL (TODO-SCHED-09).
- `share/mk/ubix.kern.mk` — suffix rules used `${OBJDIR}/${.TARGET}` but `.PATH.o: ${OBJDIR}` caused bmake to expand `.TARGET` to the full path, doubling the output directory; changed to `${OBJDIR}/${.TARGET:T}` (basename-only).
- `sys/fs/vfs/file.c` (`fgetc`) — removed debug `kprintf("[%s:%i]"…)` that fired on every character read (TODO-VFS-01).
- `sys/fs/vfs/file.c` (`sys_fclose`) — removed duplicate `args->FILE == NULL` guard that ran the check twice (TODO-VFS-02).
- `sys/mpi/mpi_syscalls.c` (`sys_mpiPostMessage`) — removed stale `kprintf("mPM: %s"…)` debug log (TODO-MPI-04).
- `sys/include/vmm/vmm.h` — defined `VMM_CHILD_PD_WINDOW 0x5A00000` as a named constant with a comment explaining its purpose (TODO-VMM-03).
- `sys/vmm/paging.c` — replaced all four occurrences of `0x5A00000` with `VMM_CHILD_PD_WINDOW` (TODO-VMM-03).
- `sys/include/mpi/mpi.h` — added `MPI_ASYNC` (0x1) and `MPI_SYNC` (0x2) named constants (TODO-MPI-06).
- `sys/arch/i386/schedyield.S` — deleted dead file containing `sched_yield_new`, which called `iret` as a plain C call — instant stack corruption if ever reached; removed `schedyield.o` from `sys/arch/i386/Makefile` (TODO-SCHED-07).

### Added
- `docs/architecture/vmm.md` — Virtual Memory Manager design document (converted from `doc/vmm.txt`; incorporates page-directory map from `doc/vmm/i386_vmm_map.txt`).
- `docs/architecture/task-switching.md` — moved from `docs/` root; content unchanged.
- `docs/design/fbcon.md` — VESA framebuffer console spec; moved from `docs/` root.
- `docs/drivers/writing-a-driver.md` — driver writing guide (rewritten from `doc/sample_driver.c` commentary).
- `docs/architecture/i386-page-directory-map.md` — full i386 page directory (PDE 0–1023) with purpose annotations (converted from `doc/vmm/i386_vmm_page_reference.xlsx`).
- `docs/reference/external-specs.md` — links to ELF ABI, Intel SDM, Multiboot, and FAT specifications.
- `docs/README.md` — documentation index.
- `sys/sde/assets/ubix.bmp` — background bitmap for graphical console / SDE (moved from `doc/ubix.bmp`).
### Fixed
- `sys/mpi/system.c` — all seven MPI bugs fixed in one pass:
  - `mpi_destroyMbox`: NULL-dereference when removing head or tail mailbox — added `prev`/`next` NULL guards and `mboxList` head update (BUG-MPI-01).
  - `mpi_createMbox`: `mbox->msg` and `mbox->msgLast` now explicitly initialized to NULL after `kmalloc` (BUG-MPI-02); `sprintf` replaced with bounded `strncpy` + explicit NUL terminator (BUG-MPI-05); `kmalloc` return now NULL-checked (BUG-MPI-07).
  - `mpi_postMessage` and `mpi_spam`: empty-queue append now sets both `mbox->msg` and `mbox->msgLast` (BUG-MPI-03); `kmalloc` NULL-checked in both paths (BUG-MPI-07); synchronous-send spin now waits on `mbox->msg` (not stale `msgLast`) and yields via `sched_yield()` (BUG-MPI-06).
  - `mpi_fetchMessage`: resets `mbox->msgLast` to NULL when queue drains (BUG-MPI-04).

### Added
- `docs/architecture/mpi.md` — full MPI audit: data structures, function-by-function walkthrough, syscall table, userland API, system mailbox registry, known bugs cross-referenced to BUGS.md, design limitations.
- `BUGS.md` — MPI section: BUG-MPI-01 through BUG-MPI-07 covering NULL dereferences in destroy, uninitialized `msgLast`, append/drain logic errors, sprintf overflow, sync-send race, and missing kmalloc NULL checks.
- `TODO.md` — MPI section: TODO-MPI-01 through TODO-MPI-07 covering mailbox cleanup on exit, blocking receive, queue depth limit, debug kprintf removal, missing destroyMbox stub, named type constants, and re-enabling init's receive loop.

### Removed
- `doc/` directory fully retired: `html/` and `xml/` Doxygen output, `vmm.txt`, `vmm/i386_vmm_map.txt`, `vmm/i386_vmm_page_reference.xlsx`, `sample_driver.c`, `UbixOS_Build.txt`, `ELF_Format.pdf`, `ChangeLog`, and `ubix.bmp` — all content migrated to `docs/` as Markdown or relocated to `sys/sde/assets/`; `doc/` added to `.gitignore`.

### Added
- `sys_pidStatus` (native syscall 7) — kernel implementation in `sys/kernel/gen_calls.c`; returns 1 while a task is alive, 0 when dead or not found. Wired in `sys/kernel/syscalls.c`. Args struct in `sys/include/sys/sysproto.h`.
- `lib/libc/stdio/fdopen.c` — `fdopen(3)` implementation for the in-kernel libc.
- `lib/ubix_api/ubixcwd.c` — `ubix_getcwd()` native API implementation.

### Fixed
- `sys/isa/atkbd.c` — Ctrl-C now works correctly: removed stale `kprintf("FreePages…")` debug output; added NULL guard on `tty_foreground`; transfers TTY ownership to the parent process (shell) before marking the child DEAD, so the shell prompt returns immediately instead of the terminal becoming orphaned.
- `sys/kernel/endtask.c` — on normal task exit via `endTask()`, TTY ownership is restored to the parent process so subsequent Ctrl-C presses target the correct task.
- `sys/arch/i386/i386_exec.c` — CR3-switch inline asm was missing an `"eax"` clobber; the compiler reused `%eax` (now holding the new CR3 value) as the `newProcess` pointer immediately after the switch, causing a page fault on every boot. Replaced with a direct `movl %0, %%cr3` using the `"r"` constraint and `"memory"` clobber.
- `bin/Makefile` — added `.PHONY: all clean ${SUBDIRS}`; bmake was treating the subdirectory names (`init`, `login`, `shell`, …) as up-to-date filesystem targets (the directories exist), so only `make` was ever built and the disk image booted to "Exec Format Error".

### Added
- `sys/include/ubixos/version.h` — single source of truth for OS version; all version strings (`kern.osrelease`, `kern.version`, boot banner) derive from macros in this one file.
- `sys/kernel/kern_sysctl.c` — `sysctl` MIB entries for `kern.ostype`, `kern.osrelease`, `kern.version`, `kern.hostname`, `hw.machine` wired to `version.h` macros.
- `sys/init/main.c` — boot banner (`"UbixOS 2.0.0-BETA — booting"`) derived from `UBIXOS_VERSION_STRING`.
- `uname(2)` syscall (POSIX syscall 164) filling `struct utsname` from `version.h` macros; kernel-side in `sys/kernel/gen_calls.c`, libc stub in `lib/libc/sys/uname.S`.
- `sysctl(3)` and `sysctlbyname(3)` userland API in `lib/libc/sys/sysctl.c`; MIB constants in `include/sys/sysctl.h`.
- `mkdir(2)` (syscall 136) and `rmdir(2)` (syscall 137) — kernel implementation in `sys/fs/vfs/file.c` + `sys/fs/fat/fat.c`; libc wrappers in `lib/libc/generic/`.
- `getenv(3)`, `setenv(3)`, `unsetenv(3)` in `lib/libc/stdlib/`; `environ` now initialized in `lib/ubix/sstart.c` before `main()`.
- `read(2)` (syscall 3) and `write(2)` (syscall 4) libc stubs in `lib/libc/sys/read.S` and `write.S`.
- `bin/uname` — userland `uname` command supporting `-a -m -n -r -s -v`.
- `bin/cat` — minimal `cat(1)` implementation (~75 lines) replacing the 388-line FreeBSD original that depended on unsupported headers.
- `bin/syscheck` — runtime test binary that exercises `uname`, `sysctl`, `mkdir`, `rmdir`, `getenv`, `setenv`, and `environ`; prints PASS/FAIL per test.
- `include/sys/utsname.h` — `struct utsname` with `_SYS_NAMELEN=256` byte fields.

### Fixed
- `sys/fs/vfs/file.c` — forward declaration of `sysMkDir` added to resolve implicit-declaration error when `sys_mkdir` calls it before its definition later in the file.
- `lib/libc/generic/mkdir.c` — syscall number corrected from 29 (`creat`) to 136 (`mkdir`).
- `lib/libc/string/strerror.c` — added `extern` declarations for `sys_nerr`/`sys_errlist` (defined in `gen/errlst.c`) to fix implicit-declaration build errors.
- `contrib/tcc/ubixos_shim/stdlib_ext.c` — removed stub `getenv` (now provided by `lib/libc/stdlib/getenv.o`).
- `contrib/tcc/ubixos_shim/syscalls.S` — removed stub `read` (now provided by `lib/libc/sys/read.o`).

### Added
- `bin/views/` — MPI-based display compositor: jailbar desktop, PS/2 arrow cursor, window table (up to 16 windows), DISPLAY_QUERY/CLAIM/FLIP/RELEASE/MOUSE protocol, hit-testing for mouse event routing. Launched by `views` on startup; taskbar is forked as a child.
- `bin/taskbar/` — system taskbar: blue strip at screen bottom, live clock (HH:MM:SS), launcher button with press/release highlight, flyout menu (Terminal, About) that opens above the button on click.
- `lib/libfb/` — shared framebuffer drawing library (`fb_rect`, `fb_blit`, `fb_text`, `fb_pixel`, `fb_set_target`, `fb_share_buffer`, `fb_poll_mouse`) used by views and display clients.
- `include/views/display_proto.h` — MPI message structs for the display protocol shared between the compositor and clients.
- `include/fb/fb.h` — public framebuffer API header.
- `sys/vmm/vmm_share_region.c` — native syscall 45: maps physical pages from one process's address space into another; used by views to give display clients a shared pixel buffer.
- `sys/isa/mouse.c` — PS/2 mouse packet decoder: relative motion accumulation, button state tracking, ring-buffer drain via `fb_poll_mouse`.
- `include/sys/mouse.h` — `mouse_event_t` struct for the mouse ring buffer.

### Fixed
- `lib/libc/sys/getpid.S` — replaced broken `getpid.c` stub that called `exit()` (eax=1) instead of getpid (eax=20), silently terminating any process that called `getpid()`.
- `sys/vmm/copyvirtualspace.c` — COW loop now skips `freePage` for physical frames at or above `numPages × PAGE_SIZE` (MMIO/framebuffer pages have no `vmmMemoryMap` entry); prevents triple-fault on fork when VESA framebuffer pages are mapped.
- `sys/vmm/paging.c` (`vmm_cleanVirtualSpace`) — likewise skips `freePage` for MMIO frames during `execve` address-space teardown.
- `sys/vmm/vmm_memory.c` (`freePage`) — explicit bounds check returns -1 for out-of-range frame indices as a safety net.
- `sys/vmm/copyvirtualspace.c` — kernel PD entries (indices 770–1015) are re-synced from the parent **after** all `vmm_getFreeKernelPage`/`vmm_getFreePage` allocations to prevent child from inheriting stale zero PD entries for newly-expanded kernel ranges.

---

## [2.0.0-BETA] - 2026-05-10

### Added
- `include_old/dirent.h` and `lib/libc_old/dirent/` — userland `opendir`/`readdir`/`closedir` implementation backed by the kernel VFS `sys_opendir`/`sys_readdir`/`sys_closedir` syscalls. Provides the standard `DIR`/`struct dirent` API to dynamically-linked binaries such as `ls`.
- `docs/task-switching.md` — detailed documentation of the hardware TSS-based task switching mechanism, GDT/LDT layout, fork mechanics, FPU lazy save/restore, and a critical review with improvement suggestions.
- `kTask_t.kernelStack` field — stores the base address of each task's dedicated ring-0 kernel stack for future cleanup on task exit.
- `kprint_len(char *, size_t)` — kernel print function that writes up to a specified number of characters to the display.
- `sys/fs/fat/Makefile` — FAT filesystem driver now has its own build file (was missing, causing fat objects to be excluded from the kernel link).
- `sys/lib/kern_trie.c` now included in the lib build (`kern_trie.o` added to `sys/lib/Makefile`) so sysctl trie operations link correctly.
- Sized C++ delete operators (`operator delete(void*, unsigned int)` / `operator delete[](void*, unsigned int)`) added to `sys/lib/libcpp.cc` for GCC 14+ compatibility.

### Fixed
- **Dynamic linker (`ld.so`) — `ls` now runs successfully with shared libraries**:
  - **`libexec/ld/addlibrary.c` — `R_386_JMP_SLOT` used `+=` instead of `=`**: when loading a shared library (e.g. `libc.so`), JMP_SLOT GOT entries were resolved as `*reMap += output + sym.st_value`. Because the initial GOT slot holds a PLT fallback stub offset (not zero), this added the load base to a small non-zero integer and produced a garbage address. The PLT lazy binding path then pushed `GOT[1]=0xDEAD` and jumped to `GOT[2]=0xBEEF` — invalid addresses — causing a crash at first cross-library call (e.g. `opendir` calling `malloc`). Fixed to `*reMap = output + sym.st_value` (absolute assignment), matching the eager resolution that `sys/kernel/ld.c:ldEnable` already performs for `ld.so` itself.
  - **Cross-compiler enforcement in world Makefile.incl files**: `lib/Makefile.incl`, `bin/Makefile.incl`, and `libexec/Makefile.incl` now detect Darwin and unconditionally set `CC`, `CXX`, `AS`, `AR`, `LD`, `NM`, `OBJCOPY`, and `RANLIB` to `x86_64-elf-*`. Previously, building from a subdirectory (e.g. `bmake -C libexec/ld`) used Apple Clang, which defines `__SIZE_TYPE__` as `long unsigned int` with `-m32`, conflicting with the `unsigned int size_t` in `sys/types.h`.
  - **Debug cleanup**: removed all investigation-specific `kprintf`/`printf` calls added while tracing the crash — `trap.c` (EIP-range dump block), `sys/fs/vfs/file.c` (`fseek` trace), `libexec/ld/findfunc.c` (per-symbol search trace), `libexec/ld/main.c` (resolution success trace), `libexec/ld/addlibrary.c` (base/dynp/dV/TLS traces).
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

[Unreleased]: https://github.com/cwolsen7905/UbixOS/compare/v2.4.0-BETA...HEAD
[2.4.0-BETA]: https://github.com/cwolsen7905/UbixOS/compare/v2.3.0-BETA...v2.4.0-BETA
[2.3.0-BETA]: https://github.com/cwolsen7905/UbixOS/compare/v2.2.0-BETA...v2.3.0-BETA
[2.2.0-BETA]: https://github.com/cwolsen7905/UbixOS/compare/v2.1.0-BETA...v2.2.0-BETA
[2.1.0-BETA]: https://github.com/cwolsen7905/UbixOS/compare/v2.0.1-BETA...v2.1.0-BETA
[2.0.1-BETA]: https://github.com/cwolsen7905/UbixOS/compare/v2.0.0-BETA...v2.0.1-BETA
[2.0.0-BETA]: https://github.com/cwolsen7905/UbixOS/compare/acb8ba9a...v2.0.0-BETA
[1.1.0-CURRENT]: https://github.com/cwolsen7905/UbixOS/compare/30af09b3...acb8ba9a
[1.24.0]: https://github.com/cwolsen7905/UbixOS/compare/6e02e5b2...30af09b3
[0.1.0]: https://github.com/cwolsen7905/UbixOS/releases/tag/6e02e5b2
