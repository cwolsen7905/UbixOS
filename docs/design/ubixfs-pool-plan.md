# UbixFS — pooled copy-on-write filesystem (lite-ZFS) plan

> Supersedes `ubixfs2-plan.md` (the BeFS-style design). Decision (2026-06-07):
> build a **lite ZFS** — a storage *pool* with *datasets* (a POSIX filesystem and
> a raw block volume), copy-on-write, per-block checksums, and compression.
> Language: **C** (portable across host tool + kernel + eventual GRUB; SOLID via
> opaque handles + ops-vtables — no C++ runtime dependency). Snapshots are
> **deferred but hooked** (see below) so they're never impossible to add.

## Status Matrix

Legend: ✅ done & verified · 🟡 partial / in progress · ⬜ not started

| Step | Item | Status | Notes |
|------|------|--------|-------|
| 1 | Spec + Fletcher4 (`include/fs/ubixfs/ondisk.h`) | ✅ | size-asserted on-disk structs |
| 2 | vdev + SPA (label, uberblock ring, txg commit, allocator) | ✅ | host `spa_test` incl. corrupt-uberblock crash recovery |
| 3 | DMU (objects, CoW blkptr tree, sparse holes) | ✅ | host `dmu_test` (3-level tree, CoW overwrite) |
| 4 | DSL (MOS, datasets, fs + volume kinds) | ✅ | host `dsl_test`; uberblock→MOS→dataset→objset indirection (snapshot hook) |
| 5 | ubfs_fs POSIX layer (files/dirs/perms/symlink) | ✅ | host `fs_test`; inode in dnode bonus |
| 6 | Host CLI (`ubfs` mkpool/mkdir/cp/ls/rm + `cpr`) | ✅ | `bmake test` runs all host harnesses |
| 7 · K1 | Core compiles + in-kernel self-test (RAM vdev) | ✅ | aarch64; freestanding compat shims |
| 7 · K2 | Read-only VFS mount (loopback `/pool.img`) | ✅ | aarch64; `VFS_TYPE_UBIXFS 0x55` |
| 7 · K3 | Write path + txg sync (persists across reboot) | ✅ | aarch64; fixed `virtio_blk_write` + `write_fat` |
| 7 · K4 | i386 parity (driver + core link into i386 kernel) | ✅ | replaces dead v1 BeFS driver |
| 7 · K5/M1 | Raw bcache vdev (pool on its own MBR partition) | ✅ | i386 (`ad0s3`); dispatches on `mp->device` |
| 7 · K5/M2 | Populate pool with the world; run a binary off it | ✅ | i386; real-world binaries served |
| 7 · K5/M3 | Mount the pool as `/` (hybrid: FAT `/boot`) | ✅ | i386 desktop boots off the pool.  FAT shrunk 448→33 MB (/boot only), pool 128→550 MB root.  Fixed: boot-stack PD sync (2→16 pages in `vmm_create_virtual_space`) + `ubfs_vfs_close(void*)` per the VFS contract |
| 7 · K5/M4 | aarch64 raw root (MBR + multi-device virtio-blk) | ⬜ | follows i386; needs partition + multi-dev virtio-blk |
| 8 | Snapshots, GRUB module, ACLs, RAID/mirror | ⬜ | format hooks already in place (`birth_txg`, indirection, free chokepoint) |

In-OS `ubpool`/`ubfs` admin commands (zpool/zfs analog) exist via native ABI
syscall 67. See `project_ubixfs_kernel_driver` in memory for the running state.

---

## Vision

- A **pool** built on one block device (vdev); room reserved for more vdevs later.
- **Datasets** allocate from the pool:
  - a **filesystem** dataset (POSIX: perms, owners, times, symlinks, hardlinks,
    rename, sparse files, xattrs→ACLs);
  - a **volume** — a raw block volume (e.g. backing **swap**).
- Per-dataset **properties**: `compression` (off | lz4), `recordsize` (the
  "different block sizes"), `atime` (on/off).

## The soul (what makes it lite-ZFS, not just an FS)

1. **Copy-on-write** — never overwrite a live block; write new, then atomically
   swing the root. ⇒ the on-disk state is *always* consistent; power loss rewinds
   to the last committed transaction. **No journal needed.**
2. **Checksummed block pointers** — every blkptr carries its target's checksum,
   up to a single self-validating root (a Merkle tree). ⇒ integrity + future scrub.
3. **Uberblock + transaction groups (txg)** — writes batch into a txg; flipping
   one uberblock in a ring atomically commits it (and is how we recover on mount).

Everything else (datasets, volumes, snapshots, sparse files, compression) hangs off
these three.

## Layered architecture (= the build order; each layer is one SOLID responsibility)

```
ubfs_fs (POSIX files/dirs/perms)        VOL (raw block volume)     <- consumers
DSL (datasets, properties, [snapshots later])
DMU (objects, CoW, block-ptr trees, checksums, compression)
SPA (the pool: uberblock ring, txg commit, block allocation)
vdev (block device + labels; v1 = a single partition)
```

Each layer is a C module exposing an **opaque handle + an `ops` struct (vtable)**;
upper layers depend on the interface, not the impl. Block I/O is **injected**
(`read_block`/`write_block` callbacks) — the same dependency-inversion the existing
VFS (`struct fileSystem`) and the v0 core already use. SOLID mapping:

| SOLID | Here |
|---|---|
| SRP | one module per layer (`vdev.c`/`spa.c`/`dmu.c`/`dsl.c`/`fs.c`/`volume.c`) |
| DIP | block I/O injected as callbacks; layers depend on the layer-below's `ops` |
| ISP/LSP | opaque handle + `ops` vtable per layer |
| OCP | compression & checksum are **pluggable strategy tables** (add LZ4/zstd/Fletcher/SHA without touching callers) |
| DTO + validate-at-boundary | on-disk structs are typed DTOs; **verify checksum on read** |

## On-disk format

The frozen spec lives in **`include/fs/ubixfs/ondisk.h`** (structs only, plain C,
little-endian, `_Static_assert`-sized). Summary:

- **vdev label** at fixed offsets (front + back copies) → find the pool; holds pool/
  vdev GUIDs, ashift, size, and an **uberblock ring**.
- **uberblock** → `txg`, timestamp, and `rootbp` (a blkptr to the **MOS**). The
  valid uberblock with the highest `txg` wins on mount.
- **blkptr** → `dva` (vdev + block offset, 64-bit → no size cap), **`birth_txg`**,
  logical/physical size, compression id, checksum id + 256-bit checksum, level/type.
- **dnode** (an object) → block size (variable!), indirection levels, up to 3
  blkptrs, and a **bonus buffer** holding the **inode** (POSIX attrs) for FS objects.
- **objset** → a metadnode whose data is the dnode array; the MOS and each
  dataset's filesystem are object sets.
- **dataset** (in the MOS) → blkptr to its objset + properties + type (FS | volume).
- **directory** → object whose data maps name → object-id (v1 linear/micro; a
  scalable hash is a later phase).

## Snapshots: deferred, but the hooks are mandatory now

v1 frees old blocks immediately on CoW (no snapshots). These three hooks keep
snapshots a purely *additive* future feature (skip any one and they become a
format break):

1. **`birth_txg` in every blkptr** — the field a snapshot needs to decide whether
   a block is its or free-able. Stored from day one (v1 ignores it).
2. **Indirection uberblock → MOS → dataset → objset** — never point the uberblock
   straight at the filesystem. A snapshot is just another (read-only) dataset
   pointing at a frozen objset root.
3. **A single block-free chokepoint** — every free routes through one function;
   snapshots-v2 changes only that function to "free unless a snapshot references
   it (by `birth_txg`)".

## Strategies (OCP — pluggable, registered in tables)

- **Checksum:** Fletcher4 (cheap) first; room for SHA-256 later. (Each block's
  algo is recorded in its blkptr.)
- **Compression:** `off` first, then **LZ4**; per-dataset `compression` property,
  per-block algo recorded in the blkptr (so mixed compression is fine).

## Scope

**In v1:** single vdev; CoW + checksums + uberblock/txg; DMU objects; FS dataset +
volume; LZ4 compression; variable recordsize; POSIX (perms, times+atime-toggle,
symlinks, hardlinks, rename, sparse, large files). **Deferred:** snapshots/clones
(hooked), RAID-Z/mirror, dedup, ZIL (sync = txg flush), ARC tuning, send/receive,
multi-vdev, xattrs/ACLs (inode reserves the slot).

## Build order (host-first — develop & fuzz the CoW engine on macOS, like u2fs)

1. ✅ **Spec + strategies** — `ondisk.h` (+ size asserts); **Fletcher4**. **Done.**
2. ✅ **vdev + SPA** — file-backed vdev, label, uberblock ring, txg commit, bitmap
   allocator (`lib/ubixfs_core/ubfs_spa.c`). **Done & verified** (`spa_test`:
   format → commit txgs → reopen at last txg → alloc/free persists → **corrupt
   newest uberblock → falls back to previous txg**, i.e. CoW crash recovery).
3. ✅ **DMU** — objects/dnodes, the CoW radix blkptr-tree (grows with the object),
   Fletcher-verified reads, sparse holes (`lib/ubixfs_core/ubfs_dmu.c`). **Done &
   verified** (`dmu_test`: 200 KB file → 3-level tree → read-back exact → CoW
   middle-overwrite consistent → sparse holes → sync/commit/reopen intact).
   *(Compression still pass-through — LZ4 is a later, per-block strategy.)*
4. ✅ **DSL** — MOS + datasets (`lib/ubixfs_core/ubfs_dsl.c`): the
   uberblock→MOS→dataset→objset indirection (snapshot hook #2), a name→object
   dataset directory, and `filesystem` + `volume` dataset kinds. **Done &
   verified** (`dsl_test`: pool → fs dataset + 16 MB volume → write each → sync →
   reopen → look up by name → read both back).
5. ✅ **ubfs_fs (POSIX layer)** — files/dirs/perms over a filesystem objset
   (`lib/ubixfs_core/ubfs_fs.c` + shared `ubfs_dir.c`); inode (mode/uid/gid/times/
   size/nlink) in the dnode bonus. **Done & verified** (`fs_test`: mkroot, mkdir,
   create files w/ perms, 200 KB file, symlink/readlink, getattr, readdir, sync +
   reopen intact, unlink). *(VOL = thin block-device wrapper over a volume's one
   object — volume R/W already works via the DMU. Remaining fs ops — hardlink,
   rename, chmod/chown, object reclamation — are the next slice.)*
6. ✅ **Host CLI** (`tools/ubixfs/ubfs.c` + `Makefile`) — the mtools-style tool:
   `mkpool` (formats a pool + a default `root` filesystem dataset), `mkdir`
   (mkdir -p), `cp` in/out (`img:/path` ref syntax; in-mode taken from the host
   file), `ls` (perms/owner/size), `rm`. Each command is a self-contained
   open → op → (sync + commit) cycle, so it edits an existing image incrementally
   like mtools. **Done & verified** end-to-end (mkpool → nested mkdir → cp in
   text + 5 KB binary → ls → cp out byte-identical → rm). `bmake test` builds and
   runs all four milestone harnesses (spa/dmu/dsl/fs) + the CLI. *(`create`
   fs/volume + volume cp are a thin follow-up once VOL lands; v1 exposes the one
   default fs dataset, which is all `mkimage.sh FS=ubixfs` needs.)*
7. **Kernel driver** (`sys/fs/ubixfs/`) — reuse the *same* C core unmodified;
   hybrid boot (FAT `/boot`, kernel mounts the pool). Built in verifiable
   increments (aarch64 first — the primary forward target — then i386):

   - **K1 — core compiles + runs in the kernel (in-kernel self-test). ✅ DONE
     (aarch64).** The freestanding-build crux: `AARCH64_KCFLAGS` is
     `-nostdinc -ffreestanding`, so the core's `<stdint.h>/<stddef.h>/<stdlib.h>`
     are absent. Solution: thin compat shims in `sys/fs/ubixfs/compat/` (stdint/
     stddef map to kernel `<sys/types.h>`; `stdlib.h` declares `malloc/free`,
     glued to `kmalloc/kfree` in `ubfs_kshim.c`), `<string.h>` from `sys/include`.
     A dedicated Makefile loop compiles the 6 core files + shim + self-test with
     `-Isys/fs/ubixfs/compat -Ilib/ubixfs_core`. The self-test
     (`ubixfs_selftest.c`, adapted from `tools/ubixfs/fs_test.c`) drives a
     **RAM-backed vdev**: format → dsl create → mkroot → mkdir/create/write →
     read-back → readdir → sync + reopen, logging PASS/FAIL to the console. Proves
     the lite-ZFS core is kernel-viable with no disk in play — de-risks K2/K3.
   - **K2 — read-only VFS mount (file-backed/loopback). ✅ DONE (aarch64).** A
     `struct fileSystem` driver (`VFS_TYPE_UBIXFS 0x55`, `sys/fs/ubixfs/ubfs_vfs.c`):
     a `ubfs_vdev_io_t` adapter that reads 4 KB pool blocks from a **backing image
     file** (`/pool.img`) through the kernel file API (`fopen`/`fread`), so no
     block-device or partition plumbing is touched — ubixfs over a loopback file
     over FAT. `vfsInitFS` = `fopen → pool_open → dsl_open → lookup("root") →
     open_dataset → fs_init` (handle in `mp->fsInfo`); `vfsOpenFile/Read` +
     `vfsOpenDir/ReadDir/CloseDir` map onto `ubfs_fs_lookup/read/getattr/readdir`.
     `mkimage-arm.sh` stages a host-built `/pool.img`; the kernel mounts it at
     `/pool` and the boot reaches `Login:`/`vlogin` (base + desktop, both green).
     *Raw-partition path (bcache/virtio-blk vdev on a second disk) still waits on
     multi-device virtio-blk — same `ubfs_vdev_io_t`, different backing.*
   - **K3 — write path + sync. ✅ DONE (aarch64).** `vfsWrite/MakeDir/Unlink` →
     `ubfs_fs_write/mkdir/unlink` + txg commit (`dsl_sync_dataset`/`dsl_sync`,
     commit-per-mutation, proven non-destructive so handles stay live). Verified by
     a two-boot test: boot 1 writes `/pool/boot.log` + commits; boot 2 reads the
     prior-boot marker back — **persistence across reboot**, boot reaches `Login:`.
     Two real fixes landed with it: **`virtio_blk_write`** (was an unimplemented
     stub — now a `VIRTIO_BLK_T_OUT` mirror of the read path) and **`write_fat`**
     error propagation (it ignored `fat_file_write`'s return → a failed device
     write read back as success).

     **Root-cause note (the bug that gated K2/K3 for a while):** mounting a pool
     faulted the physical page allocator (`vmmMemoryMap`/`numPages` overwritten with
     block data → `vmm_find_free_pages_contig` faults). It was **not** an FS bug:
     the **16 KB aarch64 boot stack** (`ldscript.aarch64`, placed right above the
     BSS globals and growing down toward `__bss_end`) was too small for the
     lite-ZFS core's block-tree traversal, which nests several 4 KB `block[BS]`
     stack frames — a deep `ubfs_fs_lookup`/write chain grew the SP past
     `__bss_end` into `vmmMemoryMap`, and a `backing_read`/`backing_write` filled
     that stack buffer with block bytes. **Fix: boot stack 16 KB → 64 KB.** (The
     per-thread kstack is 8 KB — a related risk for when the FS runs in a process
     context, K4+.) Localised with `kprintf` checkpoints + a `backing_read` buffer
     guard that exposed every block buffer sitting at a low `0x4028xxxx` stack
     address.
   - **K4 — i386 parity. ✅ DONE.** The driver + core compile + link into the
     **i386** kernel: `sys/fs/ubixfs/Makefile` builds the new driver (`ubfs_vfs.c`
     + `ubfs_kshim.c` + the 6 `lib/ubixfs_core` files via `.PATH` + the
     compat/core/include `-I`s), replacing the dead v1 BeFS driver
     (`thread/ubixfs/directory/block/dir_cache.c`, still in-tree but unbuilt);
     `sys/compile/Makefile` re-enables the `obj/sys/ubixfs/*.o` link glob;
     `sys/init/main.c` registers + mounts `/pool` rw after the FAT root (guarded
     by a `/pool.img` probe); `mkimage.sh` stages `/pool.img` like `mkimage-arm.sh`.
     Verified end to end (fresh `bmake image`): i386 boots → mounts → reads
     `/pool/hello.txt` → writes `/pool/boot.log` + commits → continues to the VESA
     desktop, and a second boot reads the prior marker (persistence). **Two stack
     fixes** (the portable core nests 4 KB `block[BS]` frames — the same class of
     bug fixed on aarch64): `start.S` boot stack 4 KB → 64 KB, and — the real one —
     `vmm_paging.c` mapped only 8 KB (2 pages) for the i386 `kmain` stack at the
     top of VA (`ESP=0xFFFFFFFF`); `ubfs_pool_open`'s 4 KB `cfgblk` frame
     underflowed it → triple-fault. Now 64 KB (16 pages, `0xFFFF0000..0xFFFFFFFF`).
   - **K5 — mountable root (a ubixfs pool as `/`, off FAT). IN PROGRESS (i386
     first).** The loopback file-backed pool *cannot* be `/` (it needs a host FS
     underneath for `fopen`), so the root needs the pool on a **raw partition**,
     read via `bcache`/the block device directly. i386 is the tractable first
     target: its IDE driver (`sys/pci/hd.c`) already parses the MBR and registers a
     **separate `ubx_device` per partition** (each with its own `parOffset`), so a
     raw `bcache` vdev just reads partition-relative LBAs (`bcache_read(dev, blk*8
     + s, …)`, 8×512 B per 4 KB block) with no multi-device work. aarch64 has a
     single virtio-blk device with no MBR — it needs MBR/partition + multi-device
     virtio-blk support first, so it follows. Increments:
     - **M1 — raw bcache vdev.** `ubfs_vfs_initfs` dispatches on `mp->device`:
       non-NULL → raw vdev (`bcache_read/write` over the partition device);
       NULL → the existing loopback (`fopen /pool.img`). Add a 3rd MBR partition
       (a raw pool) in `mkimage.sh`, `dd` a host-built pool into it, mount it at
       `/poolraw` read-only, verify reads.
     - **M2 — populate a pool with the world** (`bin/lib/libexec/etc` via the host
       `ubfs cp`), mount it read-write at a mountpoint, run a binary off it.
     - **M3 — mount the pool as `/`** (hybrid: FAT `/boot` for the kernel +
       `/pool.img`-free; pool partition = root); init/login/world run off the pool.
       Watch the 8 KB per-thread kstack — the FS now runs in process context.
     - **M4 — aarch64**: MBR partitioning + multi-device virtio-blk, then the same.
8. **Later:** snapshots, GRUB module, ACLs, RAID/mirror.

## Code layout & cleanup

New code (all C): `include/fs/ubixfs/`, `lib/ubixfs_core/`, `tools/ubixfs/`,
later `sys/fs/ubixfs/`. **Symbol prefix `ubfs_` / `UBFS_`** (not `ufs_` — that
would clash with the existing FreeBSD-style `sys/fs/ufs/`). **Cleanup (deferred):** drop the dormant
`sys/fs/ubixfsv2/` (C++ BeFS), the v0 BeFS artifacts (`include/fs/ubixfs2/`,
`lib/ubixfs2_core/`, `tools/ubixfs2/`), and reconcile the old v1 `sys/fs/ubixfs/`
at the kernel phase.
