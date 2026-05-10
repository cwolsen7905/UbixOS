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
