# BUGS

Known bugs in UbixOS. See [TODO.md](TODO.md) for improvements and enhancements.

---

## Legacy (pre-2016)

- **ENV not implemented** — userland environment variables (`getenv`/`setenv`) are missing.
- **Temperamental keyboard driver** — AT keyboard driver occasionally drops or repeats input.
- **UFS file size hack** — UFS driver has a workaround to return the correct file size rather than reading it properly from the inode.
- **ld.so hardcoded library path** — the runtime dynamic linker forces `sys:/lib/` and has no way to override it.

---

## VFS (identified 2026-05-10, fixed 2026-05-10)

| ID | File | Description |
|----|------|-------------|
| ~~BUG-VFS-01~~ | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | **FIXED** `fopen`: `path[0] == "."` compares a `char` to a `char*` — always false, so relative paths using `"."` never resolve to cwd. Changed to `'.'`. |
| ~~BUG-VFS-02~~ | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | **FIXED** `fopen`: `spinUnlock` called in the "file not found" branch without ever acquiring the lock. Removed the spurious unlock. |
| ~~BUG-VFS-03~~ | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | **FIXED** `fopen`: `kfree(tmpFd->buffer)` in the not-found path where `buffer` was never allocated. Removed the `kfree`. |
| ~~BUG-VFS-04~~ | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | **FIXED** `fread`: `fd->offset` was never advanced (line was commented out). Now advances by actual bytes read (`i`). |
| ~~BUG-VFS-05~~ | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | **FIXED** `fputc`: `vfsWrite(fd, (char*)ch, ...)` passed the `int` value of `ch` as a buffer address. Now uses `&c` where `c` is a `char`. |
| ~~BUG-VFS-06~~ | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | **FIXED** `kern_fseek`: `offset + whence` was adding the whence constant directly. Now uses a proper switch for `SEEK_SET`/`SEEK_CUR`. |
| ~~BUG-VFS-07~~ | [sys/fs/vfs/mount.c](sys/fs/vfs/mount.c) | **FIXED** `vfs_mount`: NULL dereference after `kmalloc` failure. Now returns immediately on allocation failure. |
| ~~BUG-VFS-08~~ | [sys/fs/vfs/mount.c](sys/fs/vfs/mount.c) | **FIXED** `vfs_mount`: dangling pointer when `vfsInitFS` fails. Now validates fs type before adding to mount list, and unlinks `mp` before freeing if init fails. |

---

## VMM (identified 2026-05-10, fixed 2026-05-10)

| ID | File | Description |
|----|------|-------------|
| ~~BUG-VMM-01~~ | [sys/vmm/vmm_memory.c](sys/vmm/vmm_memory.c) | **FIXED** `vmm_findFreePage`: loop `i <= numPages` was off-by-one, reading one past end of `vmmMemoryMap`. Changed to `i < numPages`. |
| ~~BUG-VMM-02~~ | [sys/vmm/pagefault.c](sys/vmm/pagefault.c) | **FIXED** `vmm_pageFault`: double `spinUnlock` when page table was missing. Restructured if/else into early-return branches so each path unlocks exactly once. |
| ~~BUG-VMM-03~~ | [sys/vmm/pagefault.c](sys/vmm/pagefault.c) | **FIXED** `vmm_pageFault`: COW path now NULL-checks `vmm_getFreeVirtualPage` result before copying. Unlocks and calls `endTask` on failure. |
| ~~BUG-VMM-04~~ | [sys/vmm/unmappage.c](sys/vmm/unmappage.c) | **FIXED** `vmm_unmapPages`: rewrote as a loop over `vmm_unmapPage` calls, inheriting its PT-boundary safety, TLB flush, and free/keep flag logic. |
| ~~BUG-VMM-05~~ | [sys/vmm/paging.c](sys/vmm/paging.c) | **FIXED** `vmm_mapFromTask`: added NULL check on `schedFindTask(pid)` result before dereferencing `child->tss.cr3`. Returns NULL on failure. |
| ~~BUG-VMM-06~~ | [sys/vmm/vmm_memory.c](sys/vmm/vmm_memory.c) | **FIXED** `adjustCowCounter`: added bounds check — logs error and returns -1 if `baseAddr / PAGE_SIZE` is outside `[0, numPages)`. |

## COW / Fork Memory (identified 2026-05-10, fixed 2026-05-10)

| ID | File | Description |
|----|------|-------------|
| ~~BUG-COW-01~~ | [sys/vmm/copyvirtualspace.c](sys/vmm/copyvirtualspace.c) | **FIXED** COW PTEs retained `PAGE_WRITE` — CPU allowed writes without faulting, so COW never fired. Both parent and child PTEs now have `PAGE_WRITE` masked out when marked COW (`(PAGE_DEFAULT & ~PAGE_WRITE) \| PAGE_COW` and `(KERNEL_PAGE_DEFAULT & ~PAGE_WRITE) \| PAGE_COW`). Parent PTEs also had `\|= PAGE_COW` changed to clear the write bit at the same time. |
| ~~BUG-COW-02~~ | [sys/vmm/pagefault.c](sys/vmm/pagefault.c) | **FIXED** COW resolution remapped the new page with `memAddr & 0xFFF` (the in-page byte offset of the fault address) as the PTE flags instead of `PAGE_DEFAULT`. Wrong bits meant the new page could end up marked write-through, cache-disabled, dirty, or still COW. Changed to `PAGE_DEFAULT`. |
| ~~BUG-COW-03~~ | [sys/kernel/endtask.c](sys/kernel/endtask.c) | **FIXED (partial)** `endTask` now calls `vmm_cleanVirtualSpace(VMM_USER_START)` before `sched_yield()`, while the dying task is still `_current` and `PT_BASE_ADDR` reflects its own page tables. This decrements COW counters for shared pages and frees private pages in the user region before the scheduler switches away — matching the FreeBSD/Linux approach of tearing down the address space from the dying task's context. TODO: extend to the kernel-mapped low region (PD index 1) and kernel stack, after which `vmm_freeProcessPages` in systemTask can be simplified or removed. |
| ~~BUG-COW-04~~ | [sys/vmm/copyvirtualspace.c](sys/vmm/copyvirtualspace.c) | **FIXED** Inner page-table loop in the user-space COW region used `PD_ENTRIES` instead of `PT_ENTRIES` as the bound. Both are 1024 (same value) so no runtime effect, but semantically wrong. Changed to `PT_ENTRIES`. |
| ~~BUG-COW-06~~ | [sys/vmm/paging.c](sys/vmm/paging.c) | **FIXED** `vmm_cleanVirtualSpace`: the `else` branch for non-COW present pages zeroed the PTE without calling `freePage()` — the physical page was never returned to the free pool, leaking one page per mapped non-COW page every time a process called `exec`. Replaced the commented-out open-coded free block with `freePage(pageTableSrc[y] & 0xFFFFF000)`. |
| ~~BUG-COW-05~~ | [sys/vmm/page_fault.S](sys/vmm/page_fault.S) | **FIXED** `_vmm_pageFault`: after `call trap` returned, the stub used `call _popFS` to reach the cleanup path. `call` pushes a return address on the stack, shifting the entire `pop %gs/%fs/%es/%ds; popa` sequence off by one slot — GS and FS got kernel code/stack addresses, and all general-purpose registers were misassigned. Before BUG-COW-01 (making COW pages read-only) this path was dead: `trap()` never returned normally from a COW fault. Once COW faults became real, every write to a shared page corrupted the returning task's register state. Fixed by replacing `call _popFS` with `add $0x4,%esp; jmp _popFS` to discard the frame-pointer argument without pushing an extra word. |

---

## ld.so (identified 2026-05-10)

| ID | File | Description |
|----|------|-------------|
| ~~BUG-LD-01~~ | [libexec/ld/addlibrary.c](libexec/ld/addlibrary.c) | **FIXED** `ldAddLibrary`: added `memset(tmpLib, 0, sizeof(ldLibrary))` after malloc so all field checks read defined values. |
| ~~BUG-LD-02~~ | [libexec/ld/addlibrary.c](libexec/ld/addlibrary.c) | **FIXED** `ldAddLibrary`: `fopen` NULL check changed from `linkerFd->fd == 0x0` to `linkerFd == 0x0`. On failure now frees `tmpLib` and returns NULL instead of calling `exit`. |
| ~~BUG-LD-03~~ | [libexec/ld/addlibrary.c](libexec/ld/addlibrary.c) | **FIXED** `ldAddLibrary`: section loop split into two passes — pass 1 loads dynstr and symtab, pass 2 applies relocations. Eliminates NULL dereference when REL appears before SYMTAB in the section table. |
| ~~BUG-LD-04~~ | [libexec/ld/addlibrary.c](libexec/ld/addlibrary.c) | **FIXED** `ldAddLibrary`: R_386_32 double relocation removed. Now a single `*reMap += output + dynValue`. |
| ~~BUG-LD-05~~ | [libexec/ld/addlibrary.c](libexec/ld/addlibrary.c) | **FIXED** `ldAddLibrary`: `while (1)` on unhandled relocation type replaced with log + `break`. |
| ~~BUG-LD-06~~ | [libexec/ld/main.c](libexec/ld/main.c) | **FIXED** `ld`: `lib_s` grown to 64 entries (defined as `LIB_S_MAX` in `ld.h`). Bounds check added before each `lib_s[lib_c++]` write. |
| ~~BUG-LD-07~~ | [libexec/ld/main.c](libexec/ld/main.c) | **FIXED** `ld`: guard added — returns 0x0 with an error message if `rel == 0` (no SHT_REL section found) before attempting to use it as a section index. |
| ~~BUG-LD-08~~ | [libexec/ld/findfunc.c](libexec/ld/findfunc.c) | **FIXED** `ldFindFunc`: removed backwards NULL check, bad printf (3 args/2 specifiers), and unreachable `break` after `return`. |

---

## Scheduler / Fork (identified 2026-05-10)

### Crashes / Correctness

| ID | File | Description |
|----|------|-------------|
| ~~BUG-SCHED-01~~ | [sys/arch/i386/fork.c:120-125](sys/arch/i386/fork.c) | **FIXED** `parent` and `children++` moved to before `newProcess->state = FORK`. Also fixed BUG-SCHED-02 in the same edit: spin-wait now reads through `volatile kTask_t *`. |
| ~~BUG-SCHED-02~~ | [sys/arch/i386/fork.c:122](sys/arch/i386/fork.c) | **FIXED** Spin-wait condition changed to `((volatile kTask_t *)newProcess)->state == FORK`, matching the `volatile` workaround already used in `fork_copyProcess`. Fixed alongside BUG-SCHED-01. |
| ~~BUG-SCHED-03~~ | [sys/arch/i386/fork.c:85](sys/arch/i386/fork.c) | **FIXED** `schedNewTask()` now allocates a dedicated 4096-byte kernel stack per task via `kmalloc`, stores the base in `kTask_t.kernelStack`, and sets `tss.esp0`/`tss.ss0` there. `fork.c`, `execFile`, and `execThread` no longer override `esp0` — all tasks get their own ring-0 stack from the point of allocation. |
| ~~BUG-SCHED-04~~ | [sys/kernel/syscall_posix.c:67](sys/kernel/syscall_posix.c) | **FIXED** Removed the `while(1) kprintf("MFR")` debug block. Syscall 89 (`getgroups`) now falls through to the normal `SYSCALL_NOTIMP` path and returns `EINVAL`. |
| ~~BUG-SCHED-05~~ | [sys/arch/i386/i386_exec.c:311](sys/arch/i386/i386_exec.c) | **FIXED** Changed `&&` to `\|\|` in both ELF magic checks in `i386_exec.c` (lines 309 and 571 — `execFile` and `sys_execve`). |

### Races

| ID | File | Description |
|----|------|-------------|
| ~~BUG-SCHED-06~~ | [sys/arch/i386/sched.c:150-151](sys/arch/i386/sched.c) | **FIXED** The `asm("sti")` before `ljmp` was load-bearing: without it, `ljmp` saves EFLAGS with `IF=0` (from the preceding `cli`) into the outgoing task's TSS, causing that task to resume with interrupts permanently disabled (breaking keyboard/timer). Fixed by saving `prevTask = _current` before the scheduler update, then setting `prevTask->tss.eflags |= 0x200` (IF bit) after `spinUnlock` and before `ljmp`. The outgoing task now always resumes with interrupts enabled, with no `sti` race window. |
| ~~BUG-SCHED-07~~ | [sys/arch/i386/timer.S:50](sys/arch/i386/timer.S) | **FIXED** Added `test %ebx,%ebx; jz done` guard before the `div %ebx` in the quantum check. Timer interrupts that fire before `vitals_init()` sets `quantum` now skip the divide entirely instead of faulting. |

### Correctness

| ID | File | Description |
|----|------|-------------|
| ~~BUG-SCHED-08~~ | [sys/arch/i386/fork.c:81](sys/arch/i386/fork.c) | **FIXED** Removed `newProcess->term->owner = newProcess->id` from both `sys_fork` and `fork_copyProcess`. The child inherits the terminal pointer but does not take ownership. Terminal ownership is unchanged across `fork()`; it is the responsibility of `setsid()`/`tcsetpgrp()` to transfer it explicitly. |

---

## Kernel (identified 2026-05-10)

### Crashes / Exploitable

| ID | File | Description |
|----|------|-------------|
| ~~BUG-KRN-01~~ | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | **FIXED** `sys_lseek` and `sys_fchdir`: moved NULL check before `fdd->fd` dereference; returns -1 early on invalid fd. |
| ~~BUG-KRN-02~~ | [sys/kernel/kern_pipe.c](sys/kernel/kern_pipe.c) | **FIXED** `sys_pipe2`: added NULL check on `kmalloc` result before `memset`; returns -1 on OOM. |
| ~~BUG-KRN-03~~ | [sys/kernel/descrip.c](sys/kernel/descrip.c) | **FIXED** `close` and `fcntl`: added `fd < 0 \|\| fd >= O_FILES` bounds check before array access; returns -1 on bad fd. |
| ~~BUG-KRN-04~~ | [sys/kernel/elf.c](sys/kernel/elf.c) | **FIXED** ELF magic check: changed `&&` to `\|\|` so any wrong byte rejects the file. |
| ~~BUG-KRN-05~~ | [sys/kernel/signal.c](sys/kernel/signal.c) | **FIXED** `sys_sigaction`: added bounds check `sig < 1 \|\| sig >= sizeof(sigact)/sizeof(sigact[0])` before indexing `td->sigact[]`. |
| ~~BUG-KRN-06~~ | [sys/kernel/ld.c](sys/kernel/ld.c) | **FIXED** `fread` for program headers: changed `sizeof(Elf_Shdr)` to `sizeof(*programHeader)` so stride always matches the declared pointer type. |

### Heap Leaks

| ID | File | Description |
|----|------|-------------|
| ~~BUG-KRN-07~~ | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | **FIXED** `fopen`: added `kfree(tmpFd)` before returning NULL when mount point is not found. |
| ~~BUG-KRN-08~~ | [sys/kernel/ld.c](sys/kernel/ld.c) | **FIXED** `SHT_REL` handler: added `relSymTab == NULL` guard — skips relocation processing with a log message if `SHT_DYNSYM` has not yet been seen. |

### Buffer Overflows

| ID | File | Description |
|----|------|-------------|
| ~~BUG-KRN-09~~ | [sys/fs/vfs/file.c](sys/fs/vfs/file.c) | **FIXED** `sys_chdir`: replaced unbounded `sprintf` with `snprintf(..., sizeof(cwd), ...)`. Added `snprintf` to kernel (`sys/lib/kprintf.c`) and declared in `sys/include/string.h`. |
| ~~BUG-KRN-10~~ | [sys/kernel/gen_calls.c](sys/kernel/gen_calls.c) | **FIXED** `sys_getlogin`: clamps `namelen` to `sizeof(_current->username)` before `memcpy`. |

### Logic Bugs

| ID | File | Description |
|----|------|-------------|
| ~~BUG-KRN-11~~ | [sys/kernel/gen_calls.c](sys/kernel/gen_calls.c) | **FIXED** `sys_setpgid`: changed `schedFindTask(pid)` to `schedFindTask(args->pid)` so the correct process is looked up. |
| ~~BUG-KRN-12~~ | [sys/vmm/pagefault.c](sys/vmm/pagefault.c) | **FIXED** COW/data-segment fault: `vmm_findFreePage` result is now checked for NULL; kills the task cleanly on OOM instead of mapping zero page writable. |
| ~~BUG-KRN-13~~ | [sys/vmm/pagefault.c](sys/vmm/pagefault.c) | **FIXED** `pageFaultSpinLock` now released before all `kpanic` calls (null address path and permission-fault path). |

---

## MPI (identified 2026-05-11)

### Crashes

| ID | File | Description |
|----|------|-------------|
| ~~BUG-MPI-01~~ | [sys/mpi/system.c:248](sys/mpi/system.c) | **FIXED** `mpi_destroyMbox`: unconditional `mbox->prev->next` and `mbox->next->prev` — NULL dereference when mailbox is at the head or tail of the list. Added NULL checks; updates `mboxList` head when removing the first entry. |
| ~~BUG-MPI-02~~ | [sys/mpi/system.c:79](sys/mpi/system.c) | **FIXED** `mpi_createMbox`: `mbox->msgLast` never initialized after `kmalloc`. Second post dereferences uninitialized pointer. Now explicitly sets `mbox->msg = mbox->msgLast = 0x0` after allocation. |
| ~~BUG-MPI-03~~ | [sys/mpi/system.c:165](sys/mpi/system.c) | **FIXED** `mpi_postMessage` and `mpi_spam`: empty-queue append set `mbox->msg` but not `mbox->msgLast`. Both now set in the empty-queue branch. |
| ~~BUG-MPI-04~~ | [sys/mpi/system.c:220](sys/mpi/system.c) | **FIXED** `mpi_fetchMessage`: `msgLast` not cleared when queue drains. Added `if (mbox->msg == 0x0) mbox->msgLast = 0x0` after dequeue. |

### Correctness

| ID | File | Description |
|----|------|-------------|
| ~~BUG-MPI-05~~ | [sys/mpi/system.c:81](sys/mpi/system.c) | **FIXED** `mpi_createMbox`: `sprintf(mbox->name, name)` with no bounds check. Replaced with `strncpy(..., sizeof(mbox->name) - 1)` and explicit NUL terminator. |
| ~~BUG-MPI-06~~ | [sys/mpi/system.c:175](sys/mpi/system.c) | **FIXED** `mpi_postMessage` type `0x2` synchronous wait: changed spin condition from `mbox->msgLast != 0x0` (never cleared, infinite spin) to `mbox->msg != 0x0` (cleared by fetchMessage), and added `sched_yield()` inside the loop. |
| ~~BUG-MPI-07~~ | [sys/mpi/system.c:79,158,117](sys/mpi/system.c) | **FIXED** `mpi_createMbox`, `mpi_postMessage`, `mpi_spam`: `kmalloc` return unchecked. All three paths now NULL-check and return an error (or `continue` in the spam loop) on allocation failure. |
