Lane: review/ubfs-qwen36 · Model: qwen3.6-35b-a3b · Date: 2026-10-11 · Scope: read-only review; no code changed.

## Files read

| File | Purpose |
|------|---------|
| `sys/fs/ubixfs/ubfs_vfs.c` | Target: VFS glue for UbixFS pooled copy-on-write filesystem |
| `sys/include/fs/ubixfs/ubfs_vfs.h` | Public API header for the VFS driver |
| `include/api/ubfs_pool.h` | Native syscall struct `ubix_pool_info` |
| `include/fs/ubixfs/ondisk.h` | On-disk format: ubfs_inode, ubfs_dirent, ubfs_dnode, etc. |
| `lib/ubixfs_core/ubfs_fs.h` | POSIX filesystem layer (ubfs_fs_t) |
| `lib/ubixfs_core/ubfs_dsl.h` | Dataset/Snapshot Layer |
| `lib/ubixfs_core/ubfs_spa.h` | Storage Pool Allocator, ubfs_vdev_io_t |
| `lib/ubixfs_core/ubfs_dmu.h` | Copy-on-write object engine |
| `lib/ubixfs_core/ubfs_dir.h` | Directory name→object map helpers |
| `lib/ubixfs_core/fletcher.h` | Fletcher-4 checksum |
| `sys/include/fs/vfs/vfs.h` | fileSystem struct with VFS function-pointer table |
| `sys/include/fs/vfs/file.h` | fileDescriptor_t, kDIR_t, struct kdirent |
| `sys/include/fs/vfs/mount.h` | vfs_mountPoint struct |
| `sys/include/fs/vfs/bcache.h` | Buffer cache for raw vdev |
| `sys/include/ubixos/spinlock.h` | struct spinLock, spinLock / spinUnlock |
| `docs/standards/kernel-rules.md` | Kernel rules K-1..K-61 |
| `docs/standards/review-checklist.md` | Review checklist |

## Findings

### Critical

| # | Function | Line | Problem | Why it matters | Suggested fix |
|---|----------|------|---------|----------------|---------------|
| C1 | `pool_relative` | 270–281 | Mount-point prefix is stripped without requiring a boundary delimiter. If the mount point is `/a` and a path `/abc` is passed, `strncmp` matches and only `/a` is stripped, leaving `bc` as the relative path. This silently routes requests to the wrong object. | Two UbixFS mounts with overlapping mount-point names (e.g. `/data` and `/data-archive`) would corrupt each other's paths. An attacker with directory-creation rights on one mount can redirect lookups on another. | After the `strncmp` match, require that either the remaining path is empty (exact-match prefix, replaced by "/") or that `path[mlen] == '/'`. |

### High

| # | Function | Line | Problem | Why it matters | Suggested fix |
|---|----------|------|---------|----------------|---------------|
| H1 | `ubfs_follow_symlink` | 478–518 | Declares `char cur[1024]`, `char target[1024]`, and inside the loop `char dir[1024]` — 3 KB on the kernel stack per invocation. Combined with the `ubfs_inode_t` (104 bytes), function prologue, and the 8 KB ring-0 stack limit (K-10), a deep symlink chain can overflow. | Kernel stack overflow corrupts the caller's stack frame, potentially crashing the task or leaking kernel data. | Move the three buffers to static storage or `kmalloc`. 1024 bytes is well within kmalloc's comfortable range. |
| H2 | `ubfs_vfs_initfs_locked` | 392–408 | Raw vdev mount copies `mp->device` (a `struct ubx_device*`) into `m->rawdev` without taking a reference. The device pointer becomes dangling if the underlying device is unregistered while the UbixFS mount is live. | Use-after-free of the device structure inside `raw_read`/`raw_write` / `bcache_read`/`bcache_write`. Can panic or corrupt memory. | Add a reference count to `struct ubfs_mount` and take it in `ubfs_vfs_initfs_locked`; drop it in an unmount hook. (Unmount hook is K3, but the reference-count discipline should be designed now.) |

### Medium

| # | Function | Line | Problem | Why it matters | Suggested fix |
|---|----------|------|---------|----------------|---------------|
| M1 | `ubfs_lock` | 95–107; `ubfs_unlock` |110–119 | Comment states the lock is a "sleeping (yielding) lock — the holder does disk I/O" but the implementation calls `spinLock`/`spinUnlock`, which are true spinning locks (busy-wait). K-22 prohibits sleeping while holding a spinlock. | The comment misleads future maintainers into believing the lock yields. In practice UbixFS's cooperative scheduling means spinning during I/O is safe (no preemption, interrupts masked on aarch64), but the semantics are undocumented and could change. | Rename to document it as a spinning lock, or replace `spinLock`/`spinUnlock` with a yielding lock primitive (e.g. a sleep/wakeup pair or mutex) that genuinely yields on I/O. |
| M2 | `ubfs_vfs_initfs_locked` | 404 | `m->rawdev = mp->device` — no validation that `mp->device` is non-NULL (the NULL check at line 398 catches a NULL pointer but doesn't verify the device object is still alive at use time). | If the device structure is freed between mount and I/O, `raw_read` dereferences a dangling pointer. | Add a reference count on the device (see H2). |
| M3 | `ubfs_follow_symlink` | 488 | `ubfs_fs_getattr` return value is not checked. On failure (negative errno), the value is implicitly cast to `uint64_t` and bitwise-ANDed with `UBFS_S_IFMT`, producing a meaningless mode. The function falls through to `UBFS_S_IFLNK` check. | On a corrupted or truncated file, getattr could return -ENODEV, which cast to uint64_t is `0xFFFFFFFFFFFFFFFF`. ANDed with `UBFS_S_IFMT` (0170000) gives 0170000, which matches `UBFS_S_IFLNK`. A symlink cycle check would be silently skipped, potentially entering an infinite loop that the hop counter eventually catches — but the intermediate behavior is undefined. | Add `if (ubfs_fs_getattr(...) < 0) return (-1);` before the mode check. |

### Low

| # | Function | Line | Problem | Why it matters | Suggested fix |
|---|----------|------|---------|----------------|---------------|
| L1 | `ubfs_vfs_write_locked` | 733–734 | `fd->size = (u_int32_t)(offset + size)` truncates a 64-bit result to 32 bits. If `offset + size` exceeds `UINT32_MAX` (~4 GB), the stored size wraps. | `fd->size` drives stat output and read-bound logic. A wrapped size reports a smaller file than exists, confusing tools like `ls`. | Compare and store using `uint64_t`; cast to `u_int32_t` only for the `fd->size` field if the VFS contract requires it, but check for overflow first. |
| L2 | `ubfs_vfs_open` | 647 | `fd->ino = (u_int32_t)obj` truncates 64-bit object IDs to 32 bits. | The inode number is visible to userland via `stat`. Object numbers > 4 billion would wrap. | Acceptable if the current pool supports ≤ 2^32 objects. If pools can exceed that, `ino` should be widened or a separate 64-bit inode field used. |
| L3 | `ubfs_vfs_readdir_locked` | 882 | `ent->d_ino = (u_int32_t)de.obj` same 32-bit truncation as L2. | Same as L2 — user-visible inode numbers are truncated. | Same as L2. |

## Summary by severity

| Severity | Count |
|----------|-------|
| Critical | 1 |
| High | 2 |
| Medium | 3 |
| Low | 3 |
| **Total** | **9** |
