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

## VMM (identified 2026-05-10)

| ID | File | Description |
|----|------|-------------|
| BUG-VMM-01 | [sys/vmm/vmm_memory.c:234](sys/vmm/vmm_memory.c#L234) | `vmm_findFreePage`: loop condition `i <= numPages` is off-by-one — reads `vmmMemoryMap[numPages]` which is one past the end of the array. Silent memory corruption on every page allocation. Fix: `i < numPages`. |
| BUG-VMM-02 | [sys/vmm/pagefault.c:86](sys/vmm/pagefault.c#L86) | `vmm_pageFault`: double `spinUnlock` when the faulting address has no page table. The `if` branch at line 86 calls `spinUnlock` then `endTask`. After the if/else block, lines 140-146 unconditionally call `spinUnlock` again. If `endTask` returns before a context switch the lock is released twice, corrupting spinlock state for all future page faults. |
| BUG-VMM-03 | [sys/vmm/pagefault.c:101](sys/vmm/pagefault.c#L101) | `vmm_pageFault`: COW path does not check if `vmm_getFreeVirtualPage` returned NULL before writing `dst[i] = src[i]`. If the system is out of virtual pages this immediately faults at address 0 inside the fault handler — unrecoverable. |
| BUG-VMM-04 | [sys/vmm/unmappage.c:110](sys/vmm/unmappage.c#L110) | `vmm_unmapPages`: the inner loop `for (y = tI; y < (tI + pages); y++)` has no bound check against `PT_ENTRIES` (1024). A range that crosses a page table boundary writes past index 1023 into the next page table. Also missing the CR3 TLB flush that `vmm_unmapPage` performs, leaving stale entries in the TLB after the unmap. |
| BUG-VMM-05 | [sys/vmm/paging.c:386](sys/vmm/paging.c#L386) | `vmm_mapFromTask`: `schedFindTask(pid)` result is used immediately at line 391 (`child->tss.cr3`) with no NULL check. If the target pid has already exited or is invalid, this dereferences NULL. |
| BUG-VMM-06 | [sys/vmm/vmm_memory.c:310](sys/vmm/vmm_memory.c#L310) | `adjustCowCounter`: computes `vmmMemoryMapIndex = baseAddr / PAGE_SIZE` with no bounds check against `numPages`. A corrupt PTE or high physical address (e.g. MMIO range) passed in from the page fault handler produces an out-of-bounds write into the memory map array. |
