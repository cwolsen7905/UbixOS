# UbixFS — pooled copy-on-write filesystem (lite-ZFS) plan

> Supersedes `ubixfs2-plan.md` (the BeFS-style design). Decision (2026-06-07):
> build a **lite ZFS** — a storage *pool* with *datasets* (a POSIX filesystem and
> a raw block volume / zvol), copy-on-write, per-block checksums, and compression.
> Language: **C** (portable across host tool + kernel + eventual GRUB; SOLID via
> opaque handles + ops-vtables — no C++ runtime dependency). Snapshots are
> **deferred but hooked** (see below) so they're never impossible to add.

## Vision

- A **pool** built on one block device (vdev); room reserved for more vdevs later.
- **Datasets** allocate from the pool:
  - a **filesystem** dataset (POSIX: perms, owners, times, symlinks, hardlinks,
    rename, sparse files, xattrs→ACLs);
  - a **zvol** — a raw block volume (e.g. backing **swap**).
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

Everything else (datasets, zvols, snapshots, sparse files, compression) hangs off
these three.

## Layered architecture (= the build order; each layer is one SOLID responsibility)

```
ZPL (POSIX files/dirs/perms)        ZVOL (raw block volume)     <- consumers
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
| SRP | one module per layer (`vdev.c`/`spa.c`/`dmu.c`/`dsl.c`/`zpl.c`/`zvol.c`) |
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
  blkptrs, and a **bonus buffer** holding the **znode** (POSIX attrs) for FS objects.
- **objset** → a metadnode whose data is the dnode array; the MOS and each
  dataset's filesystem are object sets.
- **dataset** (in the MOS) → blkptr to its objset + properties + type (FS | zvol).
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
zvol; LZ4 compression; variable recordsize; POSIX (perms, times+atime-toggle,
symlinks, hardlinks, rename, sparse, large files). **Deferred:** snapshots/clones
(hooked), RAID-Z/mirror, dedup, ZIL (sync = txg flush), ARC tuning, send/receive,
multi-vdev, xattrs/ACLs (znode reserves the slot).

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
4. **DSL** — MOS + datasets; create a filesystem dataset and a zvol.
5. **ZPL + ZVOL** — POSIX layer on objects; raw volume on one object.
6. **Host CLI** (`tools/ubixfs/`) — `mkpool`, `create` (fs/zvol), `cp` in/out,
   `ls` — the harness.
7. **Kernel driver** (`sys/fs/ubixfs/`) — reuse the same C core; hybrid boot
   (FAT `/boot`, kernel mounts the pool). Coordinate the build with the
   cross-arch agent.
8. **Later:** snapshots, GRUB module, ACLs, RAID/mirror.

## Code layout & cleanup

New code (all C): `include/fs/ubixfs/`, `lib/ubixfs_core/`, `tools/ubixfs/`,
later `sys/fs/ubixfs/`. **Symbol prefix `ubfs_` / `UBFS_`** (not `ufs_` — that
would clash with the existing FreeBSD-style `sys/fs/ufs/`). **Cleanup (deferred):** drop the dormant
`sys/fs/ubixfsv2/` (C++ BeFS), the v0 BeFS artifacts (`include/fs/ubixfs2/`,
`lib/ubixfs2_core/`, `tools/ubixfs2/`), and reconcile the old v1 `sys/fs/ubixfs/`
at the kernel phase.
