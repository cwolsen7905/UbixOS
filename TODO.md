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
| TODO-VFS-01 | [sys/fs/vfs/file.c:380](sys/fs/vfs/file.c#L380) | `fgetc`: remove leftover debug `kprintf("[%s:%i]", __FILE__, __LINE__)` — fires on every character read. |
| TODO-VFS-02 | [sys/fs/vfs/file.c:290](sys/fs/vfs/file.c#L290) | `sys_fclose`: remove duplicate `NULL` check — `args->FILE == NULL` is checked twice in a row. |
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
| TODO-VMM-03 | [sys/vmm/paging.c:381](sys/vmm/paging.c#L381) | `vmm_mapFromTask`: hardcoded virtual address `0x5A00000` for the child page directory window. This should be a named constant and validated against the kernel virtual memory layout map. |
| TODO-VMM-04 | [sys/vmm/vmm_memory.c](sys/vmm/vmm_memory.c) | `freePage`: `systemVitals->freePages` is updated inside `vmmSpinLock` in some paths but the `adjustCowCounter` path updates it outside any lock on `systemVitals`. Consolidate so `freePages` is always updated under `vmmSpinLock`. |
| TODO-VMM-05 | [sys/vmm/freevirtualpage.c](sys/vmm/freevirtualpage.c) | `vmm_freeVirtualPage` is a stub (TODO comment, no implementation). Any code that expects to free individual virtual pages silently does nothing, leaking virtual address space. |
