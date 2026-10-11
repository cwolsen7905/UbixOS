# Review: sys/fs/ubixfs/ubfs_vfs.c + sys/fs/ubixfs/ubfs_kshim.c

Reviewer: claude (read-only review; no code changed).

## Files read

Under review (read in full):

- `sys/fs/ubixfs/ubfs_vfs.c` 1-692
- `sys/fs/ubixfs/ubfs_kshim.c` 1-32

Context (read, nothing reported in them):

- `sys/include/fs/ubixfs/ubfs_vfs.h` 1-49
- `lib/ubixfs_core/ubfs_fs.h` 1-70; `ubfs_fs.c` ~203-282 (write/read/readlink/getattr/readdir/unlink)
- `lib/ubixfs_core/ubfs_spa.h` 1-60; `ubfs_spa.c` ~113-265 (compute_layout, pool_format, pool_open)
- `lib/ubixfs_core/ubfs_dir.h` 28-29; `ubfs_dir.c` 93-125 (foreach/count)
- `lib/ubixfs_core/ubfs_dsl.h`, `ubfs_dmu.h` (grep for close/free only)
- `sys/fs/ubixfs/compat/stdlib.h` 1-20
- `sys/fs/vfs/file.c` 600-690, 880-940 (fread/fwrite/fclose); `sys/fs/vfs/mount.c` 90-108; `sys/fs/vfs/vfs.c` 83-127; `sys/include/fs/vfs/vfs.h` 60-100; `sys/include/fs/vfs/file.h` 60-110
- `sys/lib/kmalloc.c` 165-215
- `sys/kern/syscall.c` 250-265, `sys/arch/aarch64/kern/syscall_md.c` 110-122
- `sys/init/main.c` 255-290, 380-395; `sys/arch/aarch64/kern/boot.c` 215-230

## Summary

The glue is small and mostly correct on the happy path, and the open/close/readdir
handle ownership has already been fixed once (fd->res zeroing, vfsClose taking the
handle). The serious problems are at the trust boundaries: `malloc()` truncates
`size_t` to 32 bits so a crafted pool label can overflow the heap (or panic the
kernel), `ubfs_vfs_query` writes through an unvalidated user pointer with an
unclamped count, and `read`/`write` pass unvalidated `long size` / `off_t offset`
straight into the core, where `off + len` can wrap. There is also no locking
anywhere in the driver while the kernel is SMP, and the "consume-once"
`g_raw_next` / `g_mount` globals make the boot-time mount retry loop and
multi-pool bounds checks behave wrongly. Directory listings silently truncate at
256 entries, and `pool_relative` strips a prefix without a path-component
boundary.

## Findings

### 1. [high] ubfs_kshim.c:22 — `malloc` truncates `size_t` to `u_int32_t`

```c
return (kmalloc((u_int32_t)len));
```

`ubfs_pool_open` allocates `p->bitmap_blocks * UBFS_BLOCK_SIZE` where
`bitmap_blocks` is derived from the on-disk label's `config.asize`
(`ubfs_spa.c:237-239`, no check against `io->size_blocks`). A crafted/corrupt
label with `asize = 2^32/4096*BITS_PER_BLOCK + small` makes the byte count exceed
2^32; the cast wraps it to a few bytes, `kmalloc` succeeds, and the following loop
(`ubfs_spa.c:242-244`) reads `bitmap_blocks` blocks into it: heap overflow from a
mountable image (the raw-device path mounts whatever is on partition 3 at boot).
Even without wrap, a large-but-<4 GB request reaches `kmalloc`'s
`kpanic("vmm_get_free_malloc_page returned NULL")` path instead of returning NULL,
so the documented "NULL if heap exhausted" contract does not hold.
Fix: in `malloc`, return NULL if `len > 0xFFFFFFE0u` (don't cast), and have the
core / `ubfs_vfs_initfs` reject `asize > io.size_blocks` before allocating.

### 2. [medium-high] ubfs_vfs.c:625-657 — `ubfs_vfs_query` writes through an unvalidated user pointer with an unclamped count

```c
int ubfs_vfs_query(void *buf, int max)
...
memset(&out[n], 0, sizeof(out[n]));
strncpy(out[n].pool, cfg->name, ...);
```

Called from `sys_ubfs_query` (`sys/kern/syscall.c:260`, `syscall_md.c:120`) with
`args->buf` / `(int)args->max` straight from the caller. There is no
`copyout`/range check: a bad pointer faults in kernel mode (panic), and a pointer
into kernel space gets overwritten with pool records. `max` is only bounded by the
number of mounted pools, so a short user buffer is overrun by `n` records.
Fix: build records in a kernel buffer and `copyout` (or validate `buf` with the
user-range check) and clamp `max` to the user buffer size.

### 3. [medium] ubfs_vfs.c:396-404 and 412-430 — read/write pass unvalidated `size`/`offset` to the core

```c
return ((int)ubfs_fs_read(&f->m->fs, f->obj, (uint64_t)offset, data, (uint64_t)size));
...
if (ubfs_fs_write(&f->m->fs, f->obj, (uint64_t)offset, data, (uint64_t)size) < 0)
```

`size` is `long` and `offset` is `off_t`; negatives become huge `uint64_t`. In the
core `ubfs_fs_read`, `if (off + len > in.size)` wraps: with `off = 5`,
`size = -1` (len = 2^64-1) `off + len == 4`, the clamp is skipped and
`ubfs_dmu_read` is asked for ~2^64 bytes into `data` (kernel buffer overflow).
`ubfs_fs_write` likewise wraps `off + len` so `in.size` is not extended while the
DMU write runs with a huge length. Separately the result is narrowed via `(int)`,
so a legal request > INT_MAX returns a negative "error".
Fix: in the glue, reject `size < 0`, `offset < 0`, and `(uint64_t)offset + size`
overflow (return -1/EINVAL), and clamp `size` to INT_MAX.

### 4. [medium] ubfs_vfs.c:253-260, 379-385 (and main.c:380-395) — `g_raw_next` is one-shot, so mount retries silently become loopback mounts

```c
g_raw_next = 0;   /* in the raw branch of ubfs_vfs_initfs */
```

`sys/init/main.c` calls `ubfs_vfs_set_raw(1)` once, then retries
`vfs_mount(1, 3, ..., VFS_TYPE_UBIXFS, "/", "rw")` up to 8 times on failure
without re-arming it. The first `initfs` consumed the flag, so every retry takes
the loopback branch and tries `fopen("/pool.img")` on a root that isn't mounted
yet: the "cold-boot dataset-read race" retry loop can never succeed.
Conversely, if `vfs_mount` fails before reaching `initfs`, the flag stays set and
the next unrelated loopback mount becomes raw. The flag also isn't tied to the
mount (two-step set-then-mount is racy if anything else mounts concurrently).
Fix: make the backend an explicit mount argument (e.g. derive from `mp->device`
or an option string), or don't clear the flag in `initfs` and clear it in the
caller after `vfs_mount`.

### 5. [medium] ubfs_vfs.c:146-157, 175-186, 90, 303-304 — bounds check uses the global `g_mount`, not the mount owning `ctx`

```c
struct ubfs_mount *m = g_mount;
...
if (m != 0 && blk >= m->io.size_blocks)
```

During `initfs`, `g_mount` still points at the previously mounted pool (or NULL
for the first), so every block read performed by `ubfs_pool_open` /
`ubfs_dsl_open` is checked against the wrong mount's `size_blocks` (or not at
all). With `/` as a raw pool (cap 2^28 blocks) and `/poolraw`/loopback mounted
afterwards, a loopback image of N blocks is checked against the *other* mount's
size, so reads past the end of the image are not rejected by this check (they
become a short `fread`), and a smaller-than-previous image is wrongly accepted.
`raw_read`/`raw_write` have no bounds check at all (see #7).
Fix: pass the mount (or its `size_blocks`) via `ctx` (e.g. a small struct holding
fd + size) instead of consulting `g_mount`.

### 6. [medium] ubfs_vfs.c:456-464, 496-498 — directory listing silently truncated at `UBFS_DIR_MAX`

```c
if (d->count >= UBFS_DIR_MAX)
	return (0);
```

`.` and `..` consume two of 256 slots; any directory with more than 254 live
entries (e.g. a populated `/bin` or `/lib`) returns a truncated listing with
success from `opendir` and no error indication. `ubfs_fs_readdir`'s return value
(line 498) is also ignored, so a DMU read error mid-directory also yields a
silently partial listing. Callers that use readdir to decide emptiness or to
delete/copy trees will lose entries.
Fix: grow the array (count first via `ubfs_dir_count`, or realloc), or make the
list incremental by index; propagate readdir errors from `opendir`.

### 7. [medium] ubfs_vfs.c:204, 221 — `raw_read`/`raw_write` truncate `blk` to 32 bits and are not range-checked

```c
u_int32_t base = (u_int32_t)blk * UBFS_SECTORS_PER_BLOCK;
```

`blk` is 64-bit; the cast and the multiply are done in `u_int32_t`. For
`blk >= 2^29` the sector number wraps and the access lands on a *lower* LBA. A
pool whose `asize` (untrusted, from the label; `size_blocks` is set to a 2^28
"cap" that nothing enforces) or a block pointer is corrupt can therefore read or,
worse, `raw_write` over unrelated low sectors (e.g. the label/bitmap or another
partition's data, since LBAs are partition-relative but the cap is not the real
partition size). Also `bcache_read`'s LBA parameter width should be checked.
Fix: compute in `uint64_t`, reject `blk >= io.size_blocks` (using the real device
size where available), and fail rather than wrap.

### 8. [medium] ubfs_vfs.c:126-137 — `pool_relative` strips the mount prefix without a component boundary

```c
if (mlen > 1 && strncmp(path, mp->mountPoint, mlen) == 0)
{
	path += mlen;
```

The function's own comment says unlink/rmdir get an already-relative path. With
`mountPoint = "/pool"`, deleting the pool-relative path `/pool_data` (or a
directory `/pool/sub` that lives inside the pool) is rewritten to `_data` /
`/sub`, i.e. a different object is looked up (and removed if it exists); `/poolx`
becomes `x` with no leading slash. Applies to mkdir (line 558) and
unlink/rmdir (line 580).
Fix: only strip when `path[mlen]` is `'/'` or `'\0'`; better, don't guess —
have the VFS tell the driver whether the path is absolute or mount-relative.

### 9. [medium] ubfs_vfs.c:347-355 — truncate-on-open is unlink + create with no rollback, and ignores the unlink result

```c
ubfs_fs_unlink(&m->fs, path);
if (ubfs_fs_create(&m->fs, path, 0644, 0, 0, &obj) < 0)
	return (0);
ubfs_commit(m);
```

If `create` fails (ENOSPC, directory-entry limit) after `unlink` succeeded, the
existing file is gone (unlink is committed on the next commit by any other
operation) and the open returns failure: data loss on a failed `fopen("w")`.
The `ubfs_commit(m)` return value is ignored here and at line 345, so a failed
commit still lets the open succeed. The unlink also does not check that the
target is a regular file: `fopen(dir, "w")` on an empty directory deletes the
directory and replaces it with a file. The mode/uid/gid of the existing file are
reset to 0644/0/0.
Fix: create the replacement first (or add a core truncate), check
`ubfs_commit`, and refuse truncate on non-regular objects.

### 10. [medium] ubfs_vfs.c:100-105, 423-426, 558-560 — no locking; every mutation does a full txg commit

There is no lock around `m->fs`, `m->dsl`, `m->os`, `fs.now`, `g_mount`, the open
path, `vfsSync`, or `ubfs_vfs_query` (which also walks `systemVitals->mountPoints`
and reads `m->dp.used` concurrently with writers). The core's dnode/bitmap/object
set state is not designed for concurrent callers, so two threads in
read/write/open/mkdir/unlink (or `sync` + a write) can interleave inside the DMU
or inside `ubfs_commit`'s two-step sync (`dsl_sync_dataset` then `dsl_sync`), on
SMP. `ubfs_vfs_write` also calls `ubfs_commit` on *every* write call (a full
dataset + MOS + uberblock write per `write(2)`), so a 4 KB-at-a-time copy
amplifies writes heavily.
Fix: add a per-mount mutex/spinlock (or reuse the VFS's) around all entry points
and consider batching commits (commit on sync/close or timer).

### 11. [medium] ubfs_vfs.c:158-166, 187-189 — shared backing-fd offset set then used without exclusion

```c
fd->offset = (off_t)(blk * UBFS_BLOCK_SIZE);
size_t got = fread(buf, 1, UBFS_BLOCK_SIZE, fd);
```

`fd->offset` is shared mutable state on the one backing descriptor and the
`fread`/`fwrite` helpers read it back. Two concurrent core calls (see #10)
interleave "set offset / read" and one reads or **writes** the other's block
(`backing_write` at 187-188 can put data into the wrong block of the pool).
Also, `fread` returns the driver's `int` result as `size_t` and adds it to
`fd->offset`; on error that is a huge value (harmless here only because the
offset is always reset).
Fix: serialise via the per-mount lock in #10, or use a positional read API.

### 12. [low] ubfs_vfs.c:374, 364, 428-429, 519 — 32-bit truncation of size and inode numbers

```c
fd->size = (u_int32_t)in.size;
fd->ino = (u_int32_t)obj;
if ((u_int32_t)(offset + size) > fd->size) fd->size = (u_int32_t)(offset + size);
ent->d_ino = (u_int32_t)d->ents[d->idx].obj;
```

Files >= 4 GiB get a wrapped `fd->size` (and the write path's size update can
shrink it); the 64-bit `di_size` is set correctly, but VFS consumers using
`fd->size` see the wrapped value. Object numbers > 2^32 alias in `d_ino`/`ino`.
Likewise `m->backing->size / UBFS_BLOCK_SIZE` (line 280) uses a `u_int32_t`
`size`, so a loopback image >= 4 GiB gets `size_blocks` too small and
`backing_read` (line 153) rejects valid blocks. `di_nlink` is cast to `int16_t`.
Fix: clamp (saturate) rather than wrap, and note the 4 GiB limit for loopback
images or use a 64-bit size.

### 13. [low] ubfs_vfs.c:339-346 — `"r+"` on a missing file creates it

```c
if (!m->writable || (fd->mode & (fileWrite | fileTrunc)) == 0)
	return (0);
if (ubfs_fs_create(...
```

Any open with `fileWrite` on a writable mount creates the file when missing,
including `fopen(path, "r+")` (POSIX: must fail) and opens that only intend
`O_WRONLY` without `O_CREAT`. Also the new file is created before the allocation
of `f` (line 357); if `kmalloc` fails the empty file stays behind.
Fix: create only when the VFS passes an explicit create flag.

### 14. [low] ubfs_vfs.c:520, 569-583 — dirent type and unlink/rmdir contract

```c
ent->d_type = (d->ents[d->idx].type == UBFS_DT_DIR) ? KDT_DIR : KDT_REG;
```

Symlinks (`UBFS_DT_LNK`) are reported as regular files. `ubfs_vfs_unlink` is
registered for both `vfsUnlink` and `vfsRemDir` (lines 675, 677) with no type
check: `unlink()` on an empty directory and `rmdir()` on a regular file both
succeed. The core only drops the directory entry (no reclamation), so removing a
file that is still open leaves the open handle writing to an orphaned object.
Fix: map `UBFS_DT_LNK` to the symlink tag; add a type check per entry point
(separate wrappers for unlink vs rmdir).

### 15. [low] ubfs_vfs.c:591-596, 89-90 — `vfsSync` only commits the most-recently mounted pool; `g_mount` never cleared

```c
if (g_mount == 0 || !g_mount->writable)
	return (0);
return (ubfs_commit(g_mount));
```

With `/` on the pool and a second pool at `/poolraw`, a sync only flushes the
last one (the others rely on the eager commits). There is no unmount hook, so if
a pool mount is ever torn down after `g_mount` was set (e.g. `vfs_mount` failing
after `initfs`, or a future umount), `g_mount` dangles and `backing_read` /
`vfsSync` use freed memory.
Fix: iterate `systemVitals->mountPoints` for UBIXFS mounts in sync; clear/guard
`g_mount` on teardown.

### 16. [low] ubfs_vfs.c:188-189, 100-105 — no flush barrier between block writes and uberblock

`backing_write` writes via `fwrite` onto the underlying FS file (page/FAT cache)
and `ubfs_commit` never asks that FS (or the bcache for the raw vdev) to flush
before the core writes the uberblock; there is also no sync of the backing file
on `vfsSync`. After a power loss the uberblock can reach disk before the data
blocks it points to, defeating the CoW consistency guarantee.
Fix: flush the backing file / `bcache` between the data write phase and the
uberblock write (needs a barrier hook in the vdev io struct), and on `vfsSync`.

### 17. [low] ubfs_vfs.c:605-607, 42 — `g_backing` path update is non-atomic and consumed unlocked

`ubfs_vfs_set_backing` does a `strncpy` into the global used by
`ubfs_vfs_initfs` (lines 270, 273, 285); a concurrent mount can observe a torn
path. Same set-then-mount two-step as #4. Low impact (boot-time only today).
Fix: pass the path with the mount.

## Not checked

- The internals of the core (`ubfs_dsl_*`, `ubfs_dmu_*`, `ubfs_dir_*`,
  `ubfs_fs_lookup`/`split_parent` path-length handling): used only to confirm
  the call contracts above. In particular I did not verify whether
  `ubfs_dsl_open`/`ubfs_dsl_open_dataset` allocate state that leaks on the
  failure path at ubfs_vfs.c:291-299 (there is no `ubfs_dsl_close`/free API in
  `ubfs_dsl.h`, which suggests a leak of anything they allocate).
- `bcache_read`/`bcache_write` locking and LBA width, and whether the VFS
  syscall layer already validates `size`/`offset` before reaching
  `ubfs_vfs_read`/`write` (findings #3, #12 assume it does not).
- `vfs_mount` / `fopen` mode parsing for `fileWrite|fileTrunc|fileAppend`
  beyond what was needed for #9/#13 (append handling is not implemented in the
  driver at all: `fileAppend` is never consulted).
- Whether the VFS layer holds a global lock around driver calls (I found none in
  the paths read, but did not audit `sys/fs/vfs/` exhaustively).
- Nothing was built or run; all findings are from reading the source.
