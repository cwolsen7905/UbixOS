# VFS lookup performance plan (W-039)

Status: steps 1 and 5 (in part) done; profile and numbers 2026-10-14.  Goal:
parallel in-OS compiles scale with CPUs.  On 2026-10-12 a clean in-OS aarch64
kernel build took about 9 minutes at -j4, and five different files compiled no
faster in parallel (16 s) than one after another (14 s).

## Where the time goes (profile, 2026-10-14)

### How it was measured

- Workloads, run inside UbixOS on a fresh copy of a `PROFILE=base` image with
  `/usr/src` staged, 4 vCPUs, 2 GB:
  - a clean `bmake -j4 kernel` in `/usr/src`;
  - `ls -R` (twice) and `ls -lR` of a 1001-entry tree (10 directories of 99
    empty files) in `/tmp`.
- aarch64 runs under HVF on Apple Silicon; x86_64 runs under TCG, so its times
  are about ten times longer and only comparable with each other.
- Wall time is measured on the host from the serial console, so it includes
  about 0.1 s of prompt round trip (a bare `true` takes 0.1-0.2 s).
- The host was shared with other work.  The first runs ran at a load average
  of 50-90 on 10 cores.  The before/after pairs were then repeated back to
  back on a quiet host (load average under 7 for aarch64, under 3 for x86_64).
  The tables give both.  Treat differences under about 20% in `ls` as noise.
- Where the time goes: a throw-away kernel patch (not committed).
  - It counts, per VFS operation, the calls, the cycles spent waiting for
    `vfs_io_lock` and the cycles it is held.
  - It counts the cycles in each path syscall (`statx`, `open`/`openat`,
    `readlink`, `access`, `unlink`) and the cycles `fclose` spends waiting for
    `fdTable_lock` and in the filesystem's close.
  - It prints them when `/.vfsprof` is opened.
  - Cycles are `machine_cycles()`: CNTVCT at 24 MHz on aarch64, the TSC at
    1 GHz under TCG.  Syscall cycles are summed over all CPUs, so with 4 CPUs
    they can add up to 4x the elapsed time.

### Before (master at e0d7182fa)

| | aarch64 | x86_64 |
|-|---------|--------|
| `bmake -j4 kernel`, quiet host | 138.6 s | 980.4 s |
| `bmake -j4 kernel`, loaded host | 202.5 s | 980.9 s |
| `ls -R`, 1001 entries, quiet / loaded | 1.24-1.32 / 1.12-1.44 s | 8.2-8.9 / 11.2-12.6 s |
| `ls -lR`, 1001 entries, quiet / loaded | 1.57 / 1.62-1.84 s | 10.8 / 16.5 s |

The profiled `bmake -j4 kernel` (165 s on aarch64 and 974 s on x86_64, loaded
host):

| | aarch64 | x86_64 |
|-|---------|--------|
| `vfs_io_lock` held, share of wall time | 96% | 92% |
| `readlink` calls (all fail: not a link) | 41,109 | 37,995 |
| cycles in `readlink`, all CPUs | 7.25e9 (302 s) | 2,052e9 (2,052 s) |
| cycles in `statx` / `open` / `access` | 1.18e9 / 1.34e9 / 0.18e9 | 397e9 / 350e9 / 33e9 |
| `fopen` calls (found / missing) | 60,387 / 7,402 | 55,424 / 7,450 |
| lock held by `fopen` / `fwrite` / `vfs_readlink` | 1.22e9 / 1.45e9 / 0.60e9 | 484e9 / 47e9 / 270e9 |
| `fclose`: waiting for `fdTable_lock` / in the FS close | 0.31e9 / 2.12e9 | 48e9 / 441e9 |

What this says:

- **The global lock is saturated.**  `vfs_io_lock` is held for more than 90%
  of the build on both arches, so VFS work is effectively single-threaded.
- **Uncached lookups for `readlink` are the largest single cost.**
  - musl's `realpath()` calls `readlink()` once per path component, and clang
    canonicalises every file it opens.
  - Every one of the roughly 40,000 calls fails with EINVAL ("not a link").
    Each made up to three full path walks from the root, under the lock:
    `vfs_readlink`, then an `fopen` + `fclose` to tell EINVAL from ENOENT, and
    `vfs_opendir` as well when the path was missing.
  - On both arches `readlink` takes more syscall time than `statx`, `open` and
    `access` together: about half of all CPU time on x86_64.
- Missing-path probes come next: clang's header search (`statx` and `open` of
  candidates; 4,921 `statx` ENOENTs) costs two walks per miss (`fopen`, then
  `vfs_opendir`).
- Writes hold the lock for long: on aarch64, 7,760 `fwrite` calls held it for
  1.45e9 cycles, about 7.8 ms each.  That time is inside UbixFS.
- `ls -R` / `ls -lR`: `statx` takes about two thirds of the time; each call is
  a full path walk (`fopen` + `fclose`).  `ls` makes no `readlink` calls.

## What was done (2026-10-14): a VFS lookup cache

`sys/fs/vfs/file.c` caches two facts per canonical absolute path on a UbixFS
mount:

- **missing**: nothing exists at the path, not even a dangling symlink;
- **not a link**: something exists at the path and it is not a symlink.

Neither fact depends on a file's contents, size or times, so writes never
invalidate an entry.

- `readlink` answers EINVAL or ENOENT from the cache.  It now tells the two
  apart itself (`vfs_readlink` returns -EINVAL or -ENOENT), so `sys_readlink`
  no longer probes with `fopen` + `vfs_opendir`.
- `access` asks `vfs_readlink` first and only follows actual symlinks.
- A read-only `fopen`, and `vfs_opendir`, of a path cached as missing fail at
  once.  That covers `stat`, `open` and `access` of missing paths.
- A failed read-only `fopen` records "missing" only after a `vfsReadlink`
  check, so a dangling symlink is never cached as missing.
- Invalidation: every namespace change goes through `file.c`.
  - Create (`fopen` for writing), mkdir and symlink forget the path.
  - Unlink and rmdir forget the path and everything below it.
  - Rename empties the cache.
  - Mounts: an entry is consulted only while the path's current mount is
    UbixFS, so a devfs, procfs or FAT mount over a cached path hides its
    entries.  Not yet handled: a second UbixFS pool mounted over a path that
    has entries.  The fix is to empty the cache on mount and unmount
    (`sys/fs/vfs/mount.c`), which needs a prototype in
    `sys/include/fs/vfs/file.h`.
- A lookup that goes to the filesystem notes a sequence number first and
  inserts its result only if no invalidation happened in between.  A result
  read before a concurrent change is therefore never cached after it.
- Size: 2048 entries (512 sets x 4 ways, round robin), paths up to 215 bytes,
  448 KB of kernel BSS.  Longer paths are not cached.
- Only UbixFS mounts: devfs and procfs change behind the VFS, and FAT's `fopen`
  rejects directories.

### After

| | aarch64 before | aarch64 after | x86_64 before | x86_64 after |
|-|----------------|---------------|---------------|--------------|
| `bmake -j4 kernel`, quiet host | 138.6 s | 86.4 s (-38%) | 980.4 s | 483.1 s (-51%) |
| `bmake -j4 kernel`, loaded host | 202.5 s | 103.4 s | 980.9 s | 515.7 s |
| `ls -R`, 1001 entries, quiet / loaded | 1.24-1.32 / 1.12-1.44 s | 1.54-1.56 / 0.97-1.01 s | 8.2-8.9 / 11.2-12.6 s | 7.6-7.9 / 11.1-13.8 s |
| `ls -lR`, 1001 entries, quiet / loaded | 1.57 / 1.62-1.84 s | 1.86 / 1.36 s | 10.8 / 16.5 s | 10.7 / 20.1 s |

Profiled build after the change (aarch64 127 s on a loaded host; x86_64 475 s
on a quiet one):

| | aarch64 | x86_64 |
|-|---------|--------|
| `readlink` calls that reached the filesystem | 6,644 of 41,109 | 6,422 of 37,995 |
| cycles in `readlink`, all CPUs | 1.99e9 (was 7.25e9) | 428e9 (was 2,052e9) |
| `fopen` calls | 28,630 (was 67,789) | 26,659 (was 62,874) |
| `vfs_io_lock` held, share of wall time | 96% | 94% |
| lock held by `fwrite` (share of wall time) | 1.63e9 (53%) | 32e9 (7%) |
| lock held by `fopen` (share of wall time) | 0.73e9 (24%) | 281e9 (59%) |

- `ls -R` and `ls -lR` do not change beyond noise: the quiet and loaded runs
  move in opposite directions.  `stat` needs attributes, so it still walks the
  path once per file.
- The global lock stays saturated.  On aarch64 writes now hold it most; on
  x86_64 (TCG, where computing is slow and I/O is relatively cheap) the path
  walks in `fopen` still do.

## Plan, in order

Each step is measured with the same in-OS benchmarks as above.

1. **Done (#24): verified block cache in UbixFS** (lib/ubixfs_core/ubfs_dmu.c).
   - Caches decoded, checksum-verified 4 KB blocks keyed by block pointer:
     address + `birth_txg` + checksum.
   - UbixFS is copy-on-write: a block pointer always names the same bytes, so
     the cache needs no invalidation.
   - Clean `bmake -j4 kernel` on aarch64: 9.1 min -> 74 s on an idle host.
2. **Next: the lock is still held over 90% of the build** (sys/fs/ubixfs,
   lib/ubixfs_core).
   - On aarch64, `fwrite` holds it for about half of that, at roughly 8 ms
     per call.  Find out whether every write commits a txg (`ubfs_commit`) and
     batch it.
   - On x86_64 the path walks of `fopen` (`stat`, `open`) still dominate.
     That is step 5.
3. **Read directories by block, not by entry** (ubfs_dir.c).
   - Scan the 16 dirents of each 4 KB leaf from one read, instead of one
     `ubfs_dmu_read` per entry.
   - Look up the directory's dnode once per scan, not once per entry.
4. **Stop holding `fdTable_lock` across `vfsClose`** (sys/fs/vfs/file.c
   `fclose`).  Unlink the descriptor under the lock, then close it outside.
   Measured cost before the lookup cache: 0.31e9 cycles waiting for
   `fdTable_lock` on aarch64, 48e9 on x86_64.
5. **One lookup per `stat`.**
   - Add a VFS "lookup + getattr" operation (UbixFS already has `getattr`) and
     use it instead of the `fopen`/`fclose` probe.  This is what `ls -R` needs.
   - `readlink` and `access` are already handled by the lookup cache.
6. **Name cache, the rest of it**: (parent object, name) -> object, so a
   lookup that misses the path cache still skips most of the walk.  Needs
   object-level invalidation hooks in UbixFS.
7. **Finer locking**, last.  Replace `vfs_io_lock` with a reader/writer lock so
   lookups and reads run concurrently and only namespace changes and writes are
   exclusive.  UbixFS serialises on its own `g_ubfs_lock` underneath, so that
   lock must be split as well before readers can overlap.

## Not in scope

- Symlinks in the middle of a path.  Only the final component is followed
  today, and that stays as it is.
- Honouring the dirfd in `openat` and `fstatat`.  It is ignored today and is
  tracked separately.
- The pool code path for read-only loopback pools (`fread`-backed).
