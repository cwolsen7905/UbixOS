# UbixFS macOS Browser — graphical pool/image inspector plan

> Decision (2026-06-20): build a native **macOS** GUI app that opens a UbixFS
> pool image (raw pool, or a full disk image with an MBR partition), enumerates
> its datasets, traverses the POSIX filesystem inside each, and copies files
> **in and out** — the host-side "Disk Utility / Finder" for UbixFS images,
> the GUI sibling of the existing `ubfs` host CLI (`tools/ubixfs/ubfs.c`).
>
> **Why this is cheap:** the entire on-disk format already lives in the portable,
> kernel-free C11 core `lib/ubixfs_core/` (spa → dmu → dsl → dir → fs), is
> dependency-injected over a block-I/O vtable (`ubfs_vdev_io_t`), and is already
> host-compiled + test-covered (`spa_test`/`dmu_test`/`dsl_test`/`fs_test`, and
> the `ubfs` CLI). This project is therefore **GUI + one vdev shim + one
> enumeration helper** — *no on-disk format work*. See
> `docs/design/ubixfs-pool-plan.md` for the format itself.

## Status Matrix

Legend: ✅ done & verified · 🟡 partial / in progress · ⬜ not started

| Step | Item | Status | Notes |
|------|------|--------|-------|
| 0 | This plan | ✅ | `docs/design/ubixfs-mac-browser-plan.md` |
| 1 · core | `ubfs_dsl_foreach` enumeration helper (over MOS dir obj 1) | ✅ | `lib/ubixfs_core/ubfs_dsl.{c,h}`; host tools rebuild clean |
| 2 · shim | `ubfs_vdev_image.c` — file-backed vdev (raw pool) | ✅ | `tools/ubixfs-mac/UbixFSKit/ubfs_vdev_image.{c,h}` |
| 3 · shim | MBR parse → partition base offset in the vdev | ✅ | type 0x9C preferred + label-validating fallback; verified base=1 MiB on a wrapped image |
| 4 · bridge | `UbixFSKit` static lib + modulemap (core + shim) | ✅ | `bmake lib` → `libubixfskit.a`; plain `cc`, no cross toolchain |
| 5 · bridge | `UbixFSSession` Obj-C facade + `UbixFSEntry` DTO | ✅ | typed DTO; `objc_test` passes the full read/write round trip |
| 6 · gui | App shell + open-image (NSOpenPanel) | ✅ | **AppKit** (see toolkit note); `main.swift` + `AppDelegate` + menu |
| 7 · gui | Sidebar **directory tree** (datasets → dirs) | ✅ | `NSOutlineView`, lazy-loaded subdirs, two-way synced with the listing; spans datasets safely |
| 8 · gui | File browser (table: readdir + getattr) | ✅ | name/size/mode columns; dirs-first sort; symlink icon |
| 9 · gui | **Copy out** (export via NSSavePanel) | ✅ | `ubfs_fs_read` → `NSData` → host file |
| 10 · gui | **Copy in** (import via NSOpenPanel) | ✅ | `create`+`write` → `dsl_sync_dataset`+`dsl_sync`; + New Folder, Delete (button + context menu) |
| 11 · gui | QuickLook / metadata pane | ⬜ | polish; reuse getattr DTO |
| 12 | Read-write safety: open read-only by default, explicit "Enable writing" | ✅ | opens read-only; "Writing" toggle reopens read-write |

**Status: MVP complete (steps 0–10, 12).** A working read-write AppKit browser:
open image → pick dataset → traverse → import/export/mkdir/delete. Only step 11
(QuickLook/metadata polish) remains.

> **Toolkit note (2026-06-20):** the plan originally specified SwiftUI, but this
> machine has only the Command Line Tools — not Xcode.app — and SwiftUI's
> `@State`/`@StateObject` are now compiler *macros* implemented by the
> `SwiftUIMacros` plugin, which ships **only** with Xcode. Bare `swiftc` cannot
> expand them. The UI was therefore built in **AppKit** (no macros → compiles +
> runs on CLT). The presentation layer is the only thing affected: the C core
> helper, the vdev shim, and the `UbixFSSession` bridge are UI-agnostic, so a
> later switch to SwiftUI/Qt is just a new view layer over the same session. The
> originally-written SwiftUI views are preserved (not compiled) under
> `tools/ubixfs-mac/UbixFSBrowser/swiftui-future/`.

## Build & run

```sh
cd tools/ubixfs-mac
sh build-app.sh           # → build/UbixFS Browser.app  (builds libubixfskit.a + the app)
sh build-app.sh run       # build, then open it
# A populated demo pool is at build/sample.pool (open it from the app).
# C-layer regression tests:
cd UbixFSKit && bmake test
```

---

## Why the format work is already done (verification)

- **Multi-dataset:** the pool's dataset directory is a normal directory object
  (`UBFS_MOS_DIR_OBJ == 1`) in the MOS. The CLI only looks single-dataset because
  it hardcodes `ubfs_dsl_lookup(&dsl, "root", …)` (`ubfs.c:81`). Enumerating all
  datasets is `ubfs_dir_foreach(&dsl.mos, UBFS_MOS_DIR_OBJ, cb, arg)` over an
  existing primitive — wrapped as `ubfs_dsl_foreach` (step 1).
- **Per-fs traversal + metadata:** `ubfs_fs_readdir`, `ubfs_fs_getattr`,
  `ubfs_fs_lookup`, `ubfs_fs_read/_write`, `ubfs_fs_readlink` already exist
  (`lib/ubixfs_core/ubfs_fs.h`) — the full surface the GUI needs.
- **Partition offset** is the *only* genuine addition, and it lives entirely in
  the vdev shim: `ubfs_pool_open` reads the label at **block 0 of the vdev**
  (`ubfs_spa.h`); the CLI's vdev (`ubfs.c:36-46`) is a bare `pread`/`pwrite` at
  `blk * UBFS_BLOCK_SIZE`. A raw pool image works as-is; a full disk image needs
  the shim to read the MBR, find the UbixFS partition, and add a base block
  offset to every read/write — exactly the kernel's `parOffset` / `ad0s3` logic
  in `sys/dev/partition.c`. ~40 lines, zero core changes.

## Architecture (layered; each layer = one SOLID responsibility)

```
tools/ubixfs-mac/
├── UbixFSKit/                     ← C/Obj-C bridge framework (the seam)
│   ├── module.modulemap           exposes the core headers to Swift
│   ├── ubfs_vdev_image.c          file-backed vdev + MBR offset (steps 2,3)
│   ├── UbixFSSession.{h,m}        Obj-C facade over core calls (step 5)
│   └── UbixFSEntry.{h,m}          typed DTO: name,isDir,size,mode,uid,gid,mtime
└── UbixFSApp/                     ← SwiftUI (steps 6-11)
    ├── PoolDocument.swift         FileDocument wrapping a UbixFSSession
    ├── DatasetSidebar.swift       List(datasets)
    ├── FileBrowserView.swift      outline/table driven by readDir
    └── Transfer.swift             drag in → write, export → read
```

The bridge links `lib/ubixfs_core/*.c` (unchanged) + `ubfs_vdev_image.c` into a
static lib; Swift links it via the modulemap. Because the core already builds
with plain `cc -std=c11` (proven by `tools/ubixfs/Makefile`), there is **no
cross-compiler and no kernel** in this build.

### Bridge contract (what Swift sees)

```objc
@interface UbixFSEntry : NSObject          // boundary DTO, not a dictionary
@property(readonly) NSString *name;
@property(readonly) BOOL      isDir;
@property(readonly) uint64_t  size;
@property(readonly) uint32_t  mode;        // POSIX mode bits
@property(readonly) uint64_t  mtime;
@end

@interface UbixFSSession : NSObject
- (instancetype)initWithImagePath:(NSString *)p readOnly:(BOOL)ro error:(NSError **)e;
- (NSArray<NSString *> *)datasets;                                  // step 1
- (BOOL)openDataset:(NSString *)name error:(NSError **)e;          // dsl_lookup+open+fs_init
- (NSArray<UbixFSEntry *> *)readDir:(NSString *)path error:(NSError **)e;
- (NSData *)readFileAtPath:(NSString *)path error:(NSError **)e;    // step 9
- (BOOL)writeData:(NSData *)d toPath:(NSString *)p mode:(uint32_t)m error:(NSError **)e; // step 10
@end
```

## Core call mapping (no new format logic)

| GUI action | Core calls |
|------------|-----------|
| Open image | vdev shim → `ubfs_pool_open` → `ubfs_dsl_open` |
| List datasets | `ubfs_dsl_foreach` (new) → `ubfs_dir_foreach(MOS, obj 1)` |
| Select dataset | `ubfs_dsl_lookup` → `ubfs_dsl_open_dataset` → `ubfs_fs_init` |
| List a directory | `ubfs_fs_lookup` → `ubfs_fs_readdir` + `ubfs_fs_getattr` per entry |
| Copy out | `ubfs_fs_lookup` → `ubfs_fs_read` (clamps to size) |
| Copy in | `ubfs_fs_create` → `ubfs_fs_write` → `ubfs_dsl_sync_dataset` → `ubfs_dsl_sync` |
| Follow symlink | `ubfs_fs_readlink` |

## Risks / notes

- **Write atomicity:** write-back must go through `ubfs_dsl_sync_dataset` +
  `ubfs_dsl_sync` (the uberblock flip) or changes are lost — the CLI's
  `mount_pool`/sync path (`ubfs.c:81-93`) is the reference. Default the app to
  **read-only**; require explicit opt-in to write (step 12).
- **v1 format limits inherited from the core:** directories don't shrink (removed
  entries are tombstoned), no hardlink/rename yet — fine for a browser; surface
  them honestly in the UI rather than faking.
- **Disk-image vs pool-image:** detect at open — if block 0 isn't a valid pool
  label, try parsing it as an MBR and retry at each partition's offset.
- **Distribution:** lives under `tools/` (host tooling, like `tools/ubixfs/`).
  Could ship as a standalone `.app` or stay an Xcode project in-tree; decide at
  step 6. Not part of `bmake world` (it's a host app, not OS userland).

## Effort estimate

- Steps 1–3 (core helper + vdev shim + MBR): ~0.5 day
- Steps 4–5 (bridge + facade): ~0.5 day
- Steps 6–9 (SwiftUI read-only browser + export): ~1–2 days
- Steps 10–12 (write-back + polish + safety): ~1 day

**~2–3 days for a polished read-write app; ~1.5 days for read-only MVP.**
