# UbixFS — adjustable recordsize (and the foundation for compression)

> Decision (2026-06-21): make `recordsize` a real, tunable **dataset property**.
> ZFS semantics: each file freezes its block size at creation (`dnode.datablksz`),
> so changing the dataset property only affects **newly created files** — existing
> files keep their recordsize and stay readable, no migration. Settable on the fly
> via the `ubfs` tool (and the in-OS `ubfs`/`ubpool` admin later).
>
> **Max recordsize: 1 MiB** (ZFS `large_blocks` ceiling). Allowed values are the
> powers of two from 4 KiB to 1 MiB: 4K, 8K, 16K, 32K, 64K, 128K, 256K, 512K, 1M.
>
> This is also the **foundation for lz4 compression** (see
> `ubixfs-mac-browser-plan.md` history / the compression discussion): once a leaf
> can be a variable-size, multi-block contiguous extent, compression is a small
> addition on top (store `psize` ≤ `lsize`, allocate `ceil(psize/4K)` of the run).
> Format reference: `include/fs/ubixfs/ondisk.h`; pool design:
> `docs/design/ubixfs-pool-plan.md`.

## Status Matrix

Legend: ✅ done & verified · 🟡 partial / in progress · ⬜ not started

| Step | Item | Status | Notes |
|------|------|--------|-------|
| 0 | This plan | ✅ | `docs/design/ubixfs-recordsize-plan.md` |
| 1 · spa | Contiguous-run allocator (`alloc_run`/`free_run`) | ✅ | `ubfs_spa.c`; first-fit run scan; `alloc_run(1)`→`alloc_block`; `bp_free` frees the run |
| 2 · dmu | Honor `dnode.datablksz` as the leaf size (variable, multi-block extents) | ✅ | `ubfs_dmu.c` leaf path (`leaf_write_cow`/`leaf_read_verify`/`tree_read_leaf`); metadata stays 4K; verified small file in 1M dataset uses 2 blocks (no tail waste) |
| 3 · dmu | Heap leaf buffers where needed (no 4K stack assumption) | ✅ | per-call scratch: stack for ≤4K (hot path, no alloc), `malloc`/`kmalloc` only for >4K records |
| 4 · dsl | Per-file recordsize at creation + `ubfs_dsl_set_recordsize` | ✅ | `make_node` stamps `datablksz` from `ubfs_fs.recordsize`; setter rewrites `dp.recordsize`; CLI mount wires `ubfs_fs_set_recordsize` from the property |
| 5 · fmt | Validation + `feature_incompat` large-blocks bit | ✅ | `ubfs_recordsize_valid` (pow2 4K–1M); bit set authoritatively in `leaf_write_cow` (any >4K record); `ubfs_pool_open` refuses unknown incompat bits |
| 6 · cli | `ubfs set recordsize=<n>` + `ubfs get` | ✅ | host tool; verified set/get round-trip + 300K copy through 128K records.  In-OS `ubfs`/`ubpool` (syscall 67) still TODO |
| 7 · test | Host harness: large-record write/read, no-tail-waste, feature gate, reopen | ✅ | `tools/ubixfs/recordsize_test.c` (in `bmake test`); default suite still green |
| 8 · later | lz4 compression layered on this | ⬜ | separate follow-on; codec seam + compress in `leaf_write_cow` (store `psize` < `lsize`, allocate `ceil(psize/4K)`) |

**Status: recordsize complete and tunable on the fly.** Built + verified
host-side; both 64-bit kernels (aarch64 + x86_64) link with the changed core.
Remaining: mirror `set`/`get` in the in-OS admin tools (step 6 tail), then lz4
(step 8).

---

## Semantics (ZFS-aligned)

- `recordsize` is a dataset property in `ubfs_dataset_phys.recordsize` (already
  present, today a vestigial `UBFS_BLOCK_SIZE`).
- At **file creation**, `ubfs_fs_create` copies the dataset's *current* recordsize
  into the new file's `dnode.datablksz`. That value is immutable for the file's
  life. (Directories and other metadata objects always use 4 KiB.)
- Setting the property later changes only what new files inherit. Existing files —
  including every file in every pool written before this feature — keep
  `datablksz = 4096` and read back unchanged. **Backward compatible.**

## Why it's more than "honor a field"

A block pointer holds one DVA, so a leaf larger than 4 KiB must occupy a **single
contiguous run** of 4 KiB blocks (`ceil(datablksz / 4096)` of them). That drives:

1. **Contiguous-run allocator** (step 1). Today `ubfs_alloc_block` is first-fit on
   a 1-bit-per-4K-block bitmap. Add `ubfs_alloc_run(n)` (find `n` consecutive free
   blocks) and `ubfs_free_run(blk, n)`; route the existing single-block path
   through `alloc_run(1)`. Free stays the single chokepoint (snapshot hook #3).
2. **No 4 KiB stack buffers** (step 3). The DMU uses `uint8_t block[BS]` on the
   stack throughout; a 1 MiB leaf can't (kstacks are 64 KiB). Leaf-sized work
   moves to heap (`malloc` on host, `kmalloc` in-kernel via the core's compat
   shim). The no-compression path can stream per-4K sub-block and fold fletcher4
   incrementally to avoid a full 1 MiB buffer; compression (step 8) will need the
   whole-record buffer, so the seam is designed for both.

## Allocation & on-disk details

- A data leaf's `blkptr`: `dva.offset` = first block of the run; `lsize` =
  logical bytes (= `datablksz`, except the last leaf which may be short);
  `psize` = on-disk bytes (= `lsize` until compression); `fill` = blocks in the
  run. Read reads `ceil(psize/4096)` blocks from `dva.offset`; checksum (fletcher4)
  is computed over the on-disk bytes.
- Indirect blocks, the dnode array (metadnode), objset headers, and directories
  remain exactly 4 KiB — recordsize applies to **regular-file data objects** only.
- **Fragmentation:** large runs can fail on a near-full pool even with enough free
  blocks. v1 behaviour: if `alloc_run(n)` fails, fall back to a smaller leaf for
  that write (record gets split) rather than ENOSPC — logged, not silent. A real
  best-fit/space-map allocator is a later pass (already flagged in the pool plan).

## Format compatibility

- A pool containing any file with `datablksz > 4096` sets the **large-blocks
  `feature_incompat` bit** in `ubfs_vdev_config.feature_incompat`. `ubfs_pool_open`
  refuses a pool whose `feature_incompat` has bits it doesn't understand — so old
  builds cleanly reject a new pool instead of misreading it. Pools that only ever
  used 4 KiB stay readable by old code.

## CLI surface (step 6)

```
ubfs set <img> recordsize=128k [dataset]   # change the dataset property
ubfs get <img> [dataset]                    # show recordsize (+ other props)
ubfs ls  <img> <path>                       # (unchanged; per-file size as today)
```
New files created by subsequent `ubfs cp` / in-OS writes inherit the new value;
existing files are untouched. Mirror in the in-OS `ubfs`/`ubpool` (native ABI
syscall 67) after the host path is proven.

## Estimate

~4–6 days for variable recordsize end-to-end (allocator + DMU + DSL property +
CLI + tests), dual-arch (host core + freestanding kernel). lz4 (step 8) is a
small follow-on once this lands.
