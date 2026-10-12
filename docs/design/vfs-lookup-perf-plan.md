# VFS lookup performance plan (W-039)

Status: plan, 2026-10-12.  Goal: parallel in-OS compiles scale with CPUs.  Today a
clean in-OS aarch64 kernel build takes about 9 minutes at -j4, and five different
files compile no faster in parallel (16 s) than one after another (14 s).

## What is slow

Measured with a lock-contention counter in `spinLock` and by reading the code
paths:

- **No cache above the 512-byte sector cache.** A path lookup starts at the root
  every time.  Each component is a linear directory scan that makes one
  `ubfs_dmu_read` per 256-byte entry.  Each `ubfs_dmu_read` re-walks the
  metadnode tree to find the directory's dnode, then walks the directory's own
  tree.  Every 4 KB block read through that walk is copied out of `bcache` and
  re-verified with `ubfs_fletcher4`.
  - Finding entry 50 of a 100-entry directory in a pool of about 10k objects
    costs on the order of 300 verified block reads.
- **Lookups are multiplied.**
  - `stat`, `access` and `faccessat` are `fopen` + `fclose`, and retry as
    `opendir` for a directory.
  - `readlink` is up to three full lookups.
  - musl's `realpath` calls `readlink` once per path component, and clang
    canonicalises every header path.
- **One lock for all of it.**
  - `vfs_io_lock` (sys/fs/vfs/file.c) serialises every VFS operation, and it is
    held across the disk reads above.
  - `fclose` holds the global `fdTable_lock` while it waits for `g_ubfs_lock`,
    so every `fopen` and `fclose` queues behind UbixFS I/O.
  - The top contended callers were `fopen`, `vfs_readlink`, `ubfs_vfs_close`,
    `fread` and `fclose`.

## Plan, in order

Each step is measured with the same in-OS benchmarks: five files sequential vs
parallel, and a clean `bmake -j4 kernel`.

1. **Verified block cache in UbixFS** (lib/ubixfs_core/ubfs_dmu.c).
   - Cache decoded, checksum-verified 4 KB blocks keyed by block pointer:
     address + `birth_txg` + checksum word.
   - UbixFS is copy-on-write: a block pointer always names the same bytes.
     The cache therefore needs no invalidation.  A freed block reused later gets
     a new `birth_txg` and checksum, so it is a different key.
   - This removes the repeated fletcher4 and the eight-sector `bcache` gather
     for metadnode, indirect and directory blocks.
   - Size: a few MB, with clock eviction.
2. **Read directories by block, not by entry** (ubfs_dir.c).
   - Scan the 16 dirents of each 4 KB leaf from one read, instead of one
     `ubfs_dmu_read` per entry.
   - Look up the directory's dnode once per scan, not once per entry.
3. **Stop holding `fdTable_lock` across `vfsClose`** (sys/fs/vfs/file.c `fclose`).
   Unlink the descriptor under the lock, then close it outside.  This removes
   the `fdTable_lock` → `g_ubfs_lock` stall that blocks every `fopen`.
4. **One lookup per `stat`/`access`/`readlink`.**
   - Add a VFS "lookup + getattr" operation (UbixFS already has `getattr`).
   - Use it for these calls instead of the open/close and opendir probes.
5. **Name cache** (parent object, name) → (object, type), only if steps 1-4
   are not enough.
   - Invalidation hooks: `ubfs_fs_create` (open with create, and truncate as
     unlink + create), `ubfs_unlink_path` (unlink and rmdir), mkdir, symlink,
     and object free.
   - UbixFS has no rename or link: `sys_rename` copies the file, then unlinks
     the source.
6. **Finer locking**, last.  Replace `vfs_io_lock` with a reader/writer lock so
   lookups and reads run concurrently and only namespace changes and writes are
   exclusive.  Steps 1-4 shrink the time spent under the lock; this step lets
   readers overlap.

## Not in scope

- Symlinks in the middle of a path.  Only the final component is followed
  today, and that stays as it is.
- Honouring the dirfd in `openat` and `fstatat`.  It is ignored today and is
  tracked separately.
- The pool code path for read-only loopback pools (`fread`-backed).
