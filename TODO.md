# TODO

Planned work for UbixOS. Confirmed crash/correctness bugs are tracked in [BUGS.md](BUGS.md).

---

## Legacy (pre-2016)

- Make website
- Finish `fdisk`
- Work on installer
- Clean up driver system
- Enhance shared libraries
- Work on libc

---

## VFS Improvements (identified 2026-05-10)

These are not crash bugs but will make the VFS cleaner and easier to work with.
Fix the items in [BUGS.md](BUGS.md) first.

| ID | File | Description |
|----|------|-------------|
| ~~TODO-VFS-01~~ | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | **Done** `fgetc`: removed debug `kprintf("[%s:%i]"…)`. |
| ~~TODO-VFS-02~~ | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | **Done** `sys_fclose`: removed duplicate `args->FILE == NULL` check. |
| TODO-VFS-03 | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | `fread` should own offset advancement. After fixing BUG-VFS-04, audit each filesystem driver to ensure it does not also advance the offset independently, so `fd->offset` stays authoritative. |
| TODO-VFS-04 | [sys/fs/vfs/mount.c](sys/fs/vfs/mount.c) | `vfs_mount`: validate `device_find()` and `vfs_findFS()` return non-NULL before calling `vfs_addMount()`. Keeps the mount list clean and resolves BUG-VFS-08 as a side effect. |
| TODO-VFS-05 | [sys/fs/vfs/file.c](sys/fs/vfs/file.c), [mount.c](sys/fs/vfs/mount.c) | Replace unchecked `sprintf`/`strcpy` with `snprintf`/`strlcpy` throughout the VFS layer to prevent silent buffer overflows on long paths. |
| TODO-VFS-06 | [sys/fs/vfs/file.c:335](sys/fs/vfs/file.c#L335) | After fixing BUG-VFS-06, merge `kern_fseek` and `sys_fseek` into one shared helper to prevent future logic drift between the two. |
| TODO-VFS-07 | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | Complete stub syscalls: `sys_rename()`, `sysRmDir()`, `sysUnlink()` are empty or always-error. At minimum return `ENOSYS` so callers get a clear error rather than silent success. |
| TODO-VFS-08 | [sys/fs/vfs/stat.c](sys/fs/vfs/stat.c) | Replace hard-coded `0xDEADBEEF`/`0xBEEFDEAD` magic values in `sys_fstat` and `sys_fstatat` with real inode/filesystem data so tools like `ls` display correct information. |
| TODO-VFS-09 | [sys/fs/vfs/namei.c](sys/fs/vfs/namei.c), [inode.c](sys/fs/vfs/inode.c) | Remove or complete the large `#ifdef _IGNORE` blocks in pathname resolution and inode management. Dead code makes the real execution path hard to follow. |

---

## VMM Improvements (identified 2026-05-10)

Fix the items in [BUGS.md](BUGS.md) first.

| ID | File | Description |
|----|------|-------------|
| TODO-VMM-01 | [sys/vmm/unmappage.c](sys/vmm/unmappage.c) | `vmm_unmapPages`: after fixing BUG-VMM-04, audit all callers to ensure they don't pass ranges that cross page table boundaries without expecting the loop to handle the table transition correctly. Consider rewriting as repeated calls to `vmm_unmapPage` to reuse its bounds-safe logic and TLB flush. |
| TODO-VMM-02 | [sys/vmm/pagefault.c](sys/vmm/pagefault.c) | `vmm_pageFault`: the `pageFaultSpinLock` is held across the entire COW copy (including the `memcpy`-equivalent loop). For a busy system this serializes all page faults. Consider narrowing the critical section to just the PTE update once the copy is done. |
| ~~TODO-VMM-03~~ | [sys/vmm/paging.c:381](sys/vmm/paging.c#L381) | **Done** `vmm_mapFromTask`: `0x5A00000` replaced with `VMM_CHILD_PD_WINDOW` constant defined in `sys/include/vmm/vmm.h`. |
| TODO-VMM-04 | [sys/vmm/vmm_memory.c](sys/vmm/vmm_memory.c) | `freePage`: `systemVitals->freePages` is updated inside `vmmSpinLock` in some paths but the `adjustCowCounter` path updates it outside any lock on `systemVitals`. Consolidate so `freePages` is always updated under `vmmSpinLock`. |
| TODO-VMM-05 | [sys/vmm/freevirtualpage.c](sys/vmm/freevirtualpage.c) | `vmm_freeVirtualPage` is a stub (TODO comment, no implementation). Any code that expects to free individual virtual pages silently does nothing, leaking virtual address space. |
| TODO-VMM-06 | [sys/vmm/paging.c](sys/vmm/paging.c), [sys/vmm/vmm_memory.c](sys/vmm/vmm_memory.c) | PD[1] COW reference leak. `vmm_copyVirtualSpace` COW-shares PD[1] pages (0x400000–0x7FFFFF, kernel code region) on every fork, incrementing their counters. `vmm_cleanVirtualSpace` starts at `VMM_USER_START` (0x800000) and never decrements those references. One COW count leaks per process lifetime per page in that region. Fix: extend `vmm_cleanVirtualSpace` down to PD[1] (or handle PD[1] specially on exit), then simplify or remove `vmm_freeProcessPages`. Tracked as part of BUG-COW-03. |

---

## ld.so Improvements (identified 2026-05-10)

Fix the items in [BUGS.md](BUGS.md) first.

| ID | File | Description |
|----|------|-------------|
| TODO-LD-01 | [libexec/ld/main.c:58](libexec/ld/main.c#L58) | `ld`: `malloc(sizeof(FILE))` to create a fake fd struct is fragile — size of FILE varies. Use `calloc` and add a NULL check before using the result. |
| TODO-LD-02 | [libexec/ld/addlibrary.c:25](libexec/ld/addlibrary.c#L25) | Replace `sprintf(tmpFile, "sys:/lib/%s", lib)` and `sprintf(tmpLib->name, lib)` with `snprintf` to prevent buffer overflow on long library names. |
| TODO-LD-03 | [libexec/ld/addlibrary.c:48](libexec/ld/addlibrary.c#L48) | Add ELF magic validation after reading the header (`e_ident[0..3] == "\x7fELF"`, `e_machine == EM_386`). Currently any file is processed as a valid ELF. |
| TODO-LD-04 | [libexec/ld/addlibrary.c:111](libexec/ld/addlibrary.c#L111) | Validate `eShnum` and `ePhnum` from the ELF header are within reasonable bounds before using them as `malloc` sizes. A corrupt ELF could cause a massive allocation. |
| TODO-LD-05 | [libexec/ld/addlibrary.c:119](libexec/ld/addlibrary.c#L119) | Validate `eShstrndx < eShnum` before using it to index `linkerSectionHeader`. A malformed ELF header causes an out-of-bounds read. |
| TODO-LD-06 | [libexec/ld/findfunc.c:17](libexec/ld/findfunc.c#L17) | `ldFindFunc`: `libPtr->sym` defaults to 0 (null section) if `ldAddLibrary` never found a symtab. Add a check that `sym > 0` before using it to index `linkerSectionHeader`. |
| TODO-LD-07 | [libexec/ld/main.c:146](libexec/ld/main.c#L146) | `ld`: check `ldFindFunc` return value before writing it to `*reMap`. Writing 0x0 as a function address will crash on the first call to that symbol. |

---

## Scheduler / Task Switching Improvements (identified 2026-05-10)

Fix the crash items in [BUGS.md](BUGS.md) (BUG-SCHED-01 through BUG-SCHED-08) first.

| ID | File | Description |
|----|------|-------------|
| TODO-SCHED-01 | [sys/arch/i386/fork.c:120-123](sys/arch/i386/fork.c) | **Eliminate the FORK state spin-wait.** The child's TSS is fully initialized before `newProcess->state = FORK`. The spin-wait exists only to prevent the scheduler from switching to the child before the TSS is ready — but it already is. Set `newProcess->state = READY` directly (after moving `parent`/`children` up per BUG-SCHED-01) and remove the spin-wait loop. This saves two full context switches on every `fork()`. |
| ~~TODO-SCHED-02~~ | [sys/arch/i386/sched.c:160](sys/arch/i386/sched.c) | **Done — fixed as BUG-SCHED-03.** `schedNewTask()` now allocates a dedicated kernel stack and stores the base in `kTask_t.kernelStack`. |
| ~~TODO-SCHED-09~~ | [sys/arch/i386/systemtask.c](sys/arch/i386/systemtask.c) | **Done** `kernelStack` freed in `systemTask()` before `kfree(tmpTask)`. Fix placed here (not `endTask`) because esp0 still points into the stack while the task is dying; `systemTask` tears it down from a safe context. |
| TODO-SCHED-03 | [sys/arch/i386/timer.S:40](sys/arch/i386/timer.S) | **Reduce the scheduling quantum from ~1 second to 10–20 ms.** The hardcoded `movl $200,%ebx` check fires `sched()` every 200 ticks. At the standard PIT divisor (~100–200 Hz) this is 1–2 seconds per task. Change the divisor from 200 to 2 (at 100 Hz = 20 ms) or make `quantum` configurable per-task for priority classes. |
| TODO-SCHED-04 | [sys/arch/i386/sched.c:93](sys/arch/i386/sched.c) | **Add an explicit idle task to prevent infinite scheduler loop.** If no task is READY (all are blocked or dead), `sched()` spins forever via `goto schedStart`. Create an idle task during `sched_init()` that is always READY and executes `hlt` in a loop. The scheduler then always has something to switch to. |
| TODO-SCHED-05 | [sys/include/sys/tss.h:70](sys/include/sys/tss.h) | **Remove `io_space[8192]` from `struct tssStruct`.** `tss.io_map = 0x8000` tells the CPU the IOPB is past the TSS limit, so the CPU never reads `io_space`. Carrying 8 KB of dead data in every `kTask_t` wastes ~8 KB per task. Remove the field; the I/O bitmap offset of 0x8000 is still valid and continues to deny all ring-3 I/O port access. |
| TODO-SCHED-06 | [sys/arch/i386/fork.c:106](sys/arch/i386/fork.c), [sys/init/main.c:98](sys/init/main.c) | **Zero `tss.ldt` or commit to per-process LDTs.** Every task sets `tss.ldt = 0x18`, causing the CPU to load GDT[3] as the LDT on every task switch. No LDT entries are ever installed; the LDT is empty. Either set `tss.ldt = 0x0` (null selector, suppresses the LDT load) and remove GDT[3], or document clearly that this is a placeholder and add per-process LDT allocation. |
| ~~TODO-SCHED-07~~ | [sys/arch/i386/schedyield.S](sys/arch/i386/schedyield.S) | **Done** Deleted dead `schedyield.S` (`sched_yield_new` called `iret` as a normal C function — instant crash if ever reached). Removed from `sys/arch/i386/Makefile` OBJS. |
| TODO-SCHED-08 | [sys/arch/i386/sched.c:179](sys/arch/i386/sched.c) | **Stop allocating stdio `struct file` entries for kernel threads.** `schedNewTask()` unconditionally allocates `o_files[0..2]` (stdin/stdout/stderr) for every task, including kernel threads created via `execThread()`. Kernel threads have no use for these. Add a `flags` argument to `schedNewTask` to skip the stdio allocation for kernel threads, or create a separate `schedNewKernelTask()`. |

---

## Kernel Improvements (identified 2026-05-10)

Fix the crash/exploit items in [BUGS.md](BUGS.md) (BUG-KRN-01 through BUG-KRN-13) first.

| ID | File | Description |
|----|------|-------------|
| TODO-KRN-01 | [sys/kernel/descrip.c](sys/kernel/descrip.c) | Add `O_FILES` bounds check to all fd array accesses (`sys_close`, `fcntl`, `dup2`, `read`, `write`) and return `EBADF` on out-of-range. Currently these all trust userspace. |
| TODO-KRN-02 | [sys/fs/vfs/file.c:192](sys/fs/vfs/file.c#L192) | Replace all `sprintf` path-building in VFS/syscalls with `snprintf` using the buffer size. BUG-KRN-09 (`sys_chdir`) is the crash case; audit all other path concatenations in the same file. |
| TODO-KRN-03 | [sys/vmm/pagefault.c](sys/vmm/pagefault.c) | Narrow `pageFaultSpinLock` critical section — currently held across the entire COW `memcpy`. Should cover only the PTE update. (The `kpanic` lock-release part was fixed by BUG-KRN-13.) |
| TODO-KRN-04 | [sys/kernel/ld.c](sys/kernel/ld.c) | Audit all `sizeof(Elf_Shdr)` vs `sizeof(Elf_Phdr)` usages. After fixing BUG-KRN-06, add ELF field validation (bounds on `e_phnum`, `e_shnum`, `e_shstrndx`) before using them as allocation sizes or array indices. |
| TODO-KRN-05 | [sys/kernel/signal.c](sys/kernel/signal.c) | Add `NSIG` (or `128`) bounds check to all signal number array accesses in `sys_sigaction` and `sys_sigprocmask`. Return `EINVAL` for out-of-range signals. |

---

## Build System Improvements (identified 2026-05-11)

| ID | File | Description |
|----|------|-------------|
| TODO-BUILD-01 | [mk/ubix.prog.mk](mk/ubix.prog.mk) | **Replace the `obj/lib/*/*.o` glob with proper archive libraries.** Every statically-linked binary currently links against all compiled library objects via a wildcard, causing symbol conflicts (e.g. `lseek` clash between libc and the tcc shim) and bloated binaries. The fix is to run `ar rcs` at the end of each library's build to produce `libc.a`, `libm.a`, etc., then have each binary's Makefile declare only what it uses (`-lc`, `-lm`). Binaries with C++ or graphics dependencies (launcher, muffin) already have explicit extra link lines and model the desired pattern. |

---

## MPI Improvements (identified 2026-05-11)

BUG-MPI-01 through BUG-MPI-07 are all fixed. These are the remaining improvements.

| ID | File | Description |
|----|------|-------------|
| TODO-MPI-01 | [sys/mpi/system.c](sys/mpi/system.c) | Add mailbox cleanup on task exit. When a process dies (`endTask`), scan `mboxList` for mailboxes owned by that PID and free them along with any queued messages. Without this, the name is permanently reserved and all queued messages leak. |
| TODO-MPI-02 | [sys/mpi/system.c:189](sys/mpi/system.c) | Replace type `0x2` sync-send spin with a proper blocking mechanism. `sched_yield()` was added (fixes BUG-MPI-06), but the spin loop still burns CPU and can stall unrelated tasks. The correct fix is a per-mailbox semaphore that the sender sleeps on and the receiver signals when the queue drains. |
| TODO-MPI-03 | [sys/mpi/system.c](sys/mpi/system.c) | Add a maximum queue depth (e.g. 64 messages per mailbox). Return an error from `mpi_postMessage` when the limit is reached rather than silently exhausting kernel heap. |
| ~~TODO-MPI-04~~ | [sys/mpi/mpi_syscalls.c](sys/mpi/mpi_syscalls.c) | **Done** Removed debug `kprintf("mPM: %s"…)` from `sys_mpiPostMessage`. |
| TODO-MPI-05 | [lib/libc/sys/](lib/libc/sys/) | Add an assembly stub for `mpi_destroyMbox` (syscall 51). Currently only create/post/fetch have stubs; destroy can only be called via inline asm from userland. |
| ~~TODO-MPI-06~~ | [sys/include/mpi/mpi.h](sys/include/mpi/mpi.h) | **Done** Added `MPI_ASYNC` (0x1) and `MPI_SYNC` (0x2) constants to `mpi.h`. |
| TODO-MPI-07 | [bin/init/main.c](bin/init/main.c) | Re-enable the MPI receive loop in `init` (currently commented out). Once the crash bugs are fixed this loop is safe to restore and will allow other processes to send commands to PID 1. |

---

## Graphics / Display (identified 2026-05-11, updated 2026-05-13)

The kernel-side SDE (`sys/sde/`) was retired in May 2026. The full display
stack (phases 1–10) is complete. See [docs/design/display-plan.md](docs/design/display-plan.md)
and [docs/architecture/display.md](docs/architecture/display.md).

| ID | File | Description |
|----|------|-------------|
| TODO-GFX-01 | [tools/grub.cfg](tools/grub.cfg), [sys/init/start.S](sys/init/start.S) | **Use the GRUB/multiboot framebuffer instead of VM86 BIOS calls.** Add `set gfxpayload=800x600x24` to `grub.cfg` so GRUB sets VESA mode before handing off. Parse `mbi->framebuffer_*` fields from the multiboot info struct and pass LFB address + pitch + bpp directly instead of going through `biosCall`. Eliminates VM86 mode, works on real hardware without BIOS ROM quirks. |
| TODO-GFX-02 | [bin/views/views.cc](bin/views/views.cc) | **Mouse cursor shape.** views composites the cursor position but draws no visible cursor sprite. Add a 16×16 software cursor blit at the current mouse coordinates, saved/restored from the underlying desktop buffer. |
| TODO-GFX-03 | [bin/taskbar/taskbar.cc](bin/taskbar/taskbar.cc) | **Window list in taskbar.** Taskbar currently shows only the launcher button and clock. Add a center region that displays running window titles, with click-to-focus. Requires views to broadcast a window-list update message on CLAIM/RELEASE. |
