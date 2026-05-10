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
