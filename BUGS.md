# BUGS

Known bugs in UbixOS. See [TODO.md](TODO.md) for improvements and enhancements.

---

## Legacy (pre-2016)

- **ENV not implemented** — userland environment variables (`getenv`/`setenv`) are missing.
- **Temperamental keyboard driver** — AT keyboard driver occasionally drops or repeats input.
- **UFS file size hack** — UFS driver has a workaround to return the correct file size rather than reading it properly from the inode.
- **ld.so hardcoded library path** — the runtime dynamic linker forces `sys:/lib/` and has no way to override it.

---

## VFS (identified 2026-05-10)

| ID | File | Description |
|----|------|-------------|
| BUG-VFS-01 | [sys/fs/vfs/file.c:421](sys/fs/vfs/file.c#L421) | `fopen`: `path[0] == "."` compares a `char` to a `char*` — always false, so relative paths using `"."` never resolve to cwd. Fix: use `'.'`. |
| BUG-VFS-02 | [sys/fs/vfs/file.c:525](sys/fs/vfs/file.c#L525) | `fopen`: `spinUnlock` called in the "file not found" branch without ever acquiring the lock (lock is only taken in the success branch at line 504). Corrupts spinlock state. |
| BUG-VFS-03 | [sys/fs/vfs/file.c:523](sys/fs/vfs/file.c#L523) | `fopen`: `kfree(tmpFd->buffer)` in the not-found path, but `buffer` was never allocated there (it is `NULL` from `memset`). Remove the `kfree`. |
| BUG-VFS-04 | [sys/fs/vfs/file.c:315](sys/fs/vfs/file.c#L315) | `fread`: `fd->offset += size * nmemb` is commented out. Consecutive reads re-read from the same offset unless the filesystem driver advances its own cursor. The `fd->offset` must be advanced here. |
| BUG-VFS-05 | [sys/fs/vfs/file.c:363](sys/fs/vfs/file.c#L363) | `fputc`: `vfsWrite(fd, (char*)ch, ...)` passes the `int` value of `ch` as a buffer address. Should be `(char*)&ch`. Current code is undefined behavior and likely a crash. |
| BUG-VFS-06 | [sys/fs/vfs/file.c:336](sys/fs/vfs/file.c#L336) | `kern_fseek`: `tmpFd->offset = offset + whence` adds the `whence` constant (0/1/2) directly to the offset value. `sys_fseek` has the correct switch-statement logic; `kern_fseek` must be fixed to match. |
| BUG-VFS-07 | [sys/fs/vfs/mount.c:52](sys/fs/vfs/mount.c#L52) | `vfs_mount`: if `kmalloc` returns `NULL`, the error is printed but execution continues to `sprintf(mp->mountPoint, ...)` which dereferences the NULL `mp`. Must `return` after the error print. |
| BUG-VFS-08 | [sys/fs/vfs/mount.c:82](sys/fs/vfs/mount.c#L82) | `vfs_mount`: `vfs_addMount(mp)` adds `mp` to the mount list before `vfsInitFS` is called. If `vfsInitFS` fails, `mp` is `kfree`'d but remains linked in `systemVitals->mountPoints` as a dangling pointer. |
