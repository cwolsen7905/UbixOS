# Disk Utility — design + build plan

A graphical disk-management app for uBixOS, modelled on **macOS Disk Utility**:
a left sidebar of drives/volumes, a detail panel with a coloured **capacity /
partition-layout bar**, and a toolbar of actions (Info → Mount → Erase →
Partition → First Aid).  MVP is read-only and grows toward full management.

> Product fit: uBixOS is console-first, graphical-*optional*. Disk Utility is a
> **desktop-profile app** (`views` + objGFX), like Settings/NetSurf — not part of
> the base/headless profile. A future text counterpart (`diskutil` CLI, the
> macOS analog) can share the same data layer.

## Status

| Phase | Item | Status |
|-------|------|--------|
| 0 | Data layer — `ubix_disk_query` native syscall + DTO + lib thunk | ⬜ |
| 0b | Raw block access on both arches (for later write ops) | ⬜ |
| 1 | **MVP** — read-only GUI: enumerate drives, select, info + layout bar | ⬜ |
| 2 | Mount / Unmount a volume | ⬜ |
| 3 | Erase / format (FAT32 + UbixFS pool) | ⬜ |
| 4 | Partition — edit the MBR (add/delete/resize) | ⬜ |
| 5 | First Aid (fsck), GPT, USB hotplug live updates, SMART/model | ⬜ |

---

## 1. Why a new data layer (not raw `/dev`)

The terrain map turned up three blockers to the "just read `/dev` and parse the
MBR" approach:

- **aarch64 has no `/dev` block nodes.** i386 `hd.c` registers `ad0` (`'b'`,
  1:0) + partitions (`'c'`, 1:N); aarch64 `virtio_blk.c` / `sys/dev/partition.c`
  register **none** (they resolve via the `g_device_find` hook, not devfs).
- **The legacy disk tools are stale.** `bin/fdisk` opens `"devfs:ad0"` — the
  retired pre-POSIX prefix — so it doesn't even run today.
- **No geometry/model from userland.** Sector count, model, and the kernel's
  already-parsed partition table aren't exposed; userland would re-read + re-parse
  sector 0 per drive, and still couldn't get capacity for an unpartitioned disk.

So the foundation is a **structured query syscall**, mirroring the
`ubix_pool_query` (native ABI syscall 67) that `ubpool`/`ubfs` already use. It is
cross-arch (queries the kernel block-device registry, not devfs), typed (a DTO,
not a blob), and extensible (add SMART/model fields later without touching the
ABI shape callers depend on).

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  bin/diskutil  (views + objGFX desktop app)                  │  UI layer
│   sidebar (drive tree) · detail panel · partition bar · bar  │
└───────────────┬─────────────────────────────────────────────┘
                │ ubix_disk_query(buf, max)         (read; Phase 1)
                │ ubix_pool_query(...)  (67)         (UbixFS volumes)
                │ mount()/unmount()                  (Phase 2)
                │ raw open()/write() on /dev/<disk>  (Phase 3-4)
┌───────────────▼─────────────────────────────────────────────┐
│  lib/ubix_api/ubixdisk.c  — UBIX_NATIVE_THUNK(ubix_disk_query,68) │  data layer
│  include/api/ubix_disk.h  — struct ubix_disk_info DTO + flags │
├──────────────────────────────────────────────────────────────┤
│  sys: sys_disk_query — walk the block-device registry, fill   │
│       one DTO per whole-disk + per-partition entry.           │
│   i386: ubx_device list (hd.c)   aarch64: g_blk_dev + g_parts │
└──────────────────────────────────────────────────────────────┘
```

### 2.1 The DTO (`include/api/ubix_disk.h`)

One flat record per entry; whole disks and their partitions both appear in the
array, linked by index so the UI can build the tree. Typed, fixed-size, no
catch-all blobs (per the project's DTO discipline).

```c
#define UBIX_DISK_MAX     16
#define UBIX_DISK_NAMELEN 32

/* entry kind */
#define UBIX_DK_DISK      0   /* a whole drive            */
#define UBIX_DK_PART      1   /* a partition on a drive   */

/* flags */
#define UBIX_DF_REMOVABLE 0x1 /* USB / hotplug            */
#define UBIX_DF_MOUNTED   0x2 /* partition is mounted     */
#define UBIX_DF_READONLY  0x4

struct ubix_disk_info {
	char      name[UBIX_DISK_NAMELEN];   /* "ad0", "ad0s1", "vtblk0", "vtblk0s3" */
	uint8_t   kind;                      /* UBIX_DK_DISK | UBIX_DK_PART          */
	uint8_t   parent;                    /* index of the owning disk (PART only) */
	uint16_t  major;
	uint16_t  minor;
	uint8_t   mbr_type;                  /* MBR type byte (PART): 0x0C/0x82/0x9C */
	uint8_t   _pad[3];
	uint64_t  lba_start;                 /* first LBA (PART; 0 for a whole disk) */
	uint64_t  sectors;                   /* length in sectors                    */
	uint32_t  sector_size;               /* 512                                  */
	uint32_t  flags;                     /* UBIX_DF_*                            */
	char      fstype[16];                /* "fat","ubixfs","swap","" (best-effort)*/
	char      mountpoint[64];            /* if mounted, else ""                  */
	char      model[40];                 /* drive model/ID (whole disk; "" ok)   */
};

int ubix_disk_query(struct ubix_disk_info *buf, uint32_t max); /* count, or -errno */
```

### 2.2 Kernel side (`sys_disk_query`)

- Slot **68** in the native table (`sys/kern/syscalls.c`), args struct in
  `sysproto.h`, handler **duplicated per-arch** like `sys_ubfs_query`
  (i386 `sys/kern/syscall.c`, aarch64 `sys/arch/aarch64/kern/syscall_md.c`).
- Enumeration is the one arch-divergent bit (same split the partition layer hit):
  - **i386:** walk the `ubx_device` registry (whole disk `ad0` + the `ad0sN`
    partition devices `hd.c` registered); type byte from the cached MBR.
  - **aarch64:** the whole disk = `g_blk_dev`; partitions = the `g_parts[]`
    `ubp_partition` array `sys/dev/partition.c` already builds (it has
    `lba_start`/`lba_count`/`type`/`minor` — exactly the DTO fields).
  - Factor a small `md_enumerate_disks(cb)` hook so the shared filler is one
    function; cross-reference fstype/mountpoint against `systemVitals->mountPoints`
    (the same walk `ubfs_vfs_query` does).

### 2.3 Phase 0b — raw block access (deferred until write ops)

Erase/Partition (Phases 3-4) need to read/write raw sectors of a drive. That
means a **block device node** the app can `open()`/`pwrite()`:
- i386 already has `ad0` (`/dev/ad0`); fix the path (POSIX `/dev`, not `devfs:`).
- aarch64 needs devfs nodes for `vtblk0` + `vtblk0sN` — **the same gap that
  blocks fstab `/boot` on aarch64** (see the bootstrap-convergence work). Adding
  them here closes both. Whole disk + partitions registered via `devfs_makeNode`
  from `virtio_blk` after the MBR parse.

Read-only MVP (Phases 1-2) needs **none** of this — it runs entirely on
`ubix_disk_query` + `mount`/`unmount`.

## 3. UI design (macOS Disk Utility-inspired)

```
┌───────────────────────────────────────────────────────────────────────┐
│  Disk Utility                                                      [x]  │
├───────────────────────────────────────────────────────────────────────┤
│ [ⓘ Info] [▲ Mount] [⌫ Erase] [▦ Partition] [✚ First Aid]      ◄toolbar │
├──────────────────┬────────────────────────────────────────────────────┤
│ Internal         │   ▣  ad0                                            │
│  ▾ ad0           │       640 MB · MBR · IDE                            │
│      ad0s1  /boot│                                                     │
│      ad0s2  swap │   ┌───────────────────────────────────────────┐    │
│    ▸ ad0s3  /    │   │██ /boot ██│░ swap ░│███████ UbixFS  / █████│    │  ◄partition
│  ▾ vtblk0        │   └───────────────────────────────────────────┘    │    bar
│      vtblk0s3 /  │     33 MB FAT32   64 MB    550 MB UbixFS pool       │
│                  │                                                     │
│                  │   Name .......... ad0s3                            │
│                  │   Type .......... UbixFS pool (0x9C)               │
│                  │   Capacity ...... 550 MB                           │
│                  │   Used / Free ... 39 MB / 511 MB                   │  ◄detail
│                  │   Mount point ... /                                │    grid
│                  │   Scheme ........ Master Boot Record               │
│                  │   ┌─────────────────────────────────────────┐      │
│                  │   │████████ used ████████│      free         │      │  ◄capacity
│                  │   └─────────────────────────────────────────┘      │    bar
└──────────────────┴────────────────────────────────────────────────────┘
```

- **Sidebar** (left): drive tree. Disks expandable to their partitions.
  **First drive selected by default** (your spec); click any row to select.
- **Toolbar**: action buttons; greyed out until applicable (Mount enabled only
  on an unmounted partition, Erase on a partition, etc.).
- **Partition bar**: the signature visual — one coloured segment per partition,
  width proportional to size, free space hatched. Colour by `mbr_type`:
  FAT32 `0x0C` blue, swap `0x82` grey, UbixFS `0x9C` green, free space striped.
- **Detail grid**: name/type/capacity/used-free/mountpoint/scheme for the
  selection.
- **Capacity bar** (bottom): used-vs-free for the selected *volume* (used/free
  from `ubix_pool_query` for UbixFS, from FAT BPB later for FAT).

All of it is objGFX — and the **widgets are reusable, not app-private**. Every
visual element the Disk Utility needs is built as a new objGFX primitive/widget
in `lib/objgfx/` (header in `include/objgfx/`) so any app gets them for free, the
same way `ogSurface` / `ogScalableFont` are shared today. The Disk Utility is the
*first consumer*, not the owner. Candidate additions (each generic, no disk
knowledge):

- **`ogSegmentBar`** — a horizontal bar split into proportional, individually
  coloured/labelled segments (the partition-layout bar; also a generic
  stacked-bar chart). The capacity used/free bar is the 2-segment case.
- **`ogButton`** — a labelled, optionally-icon, enabled/hover/pressed button +
  hit-test, for toolbars and dialogs.
- **`ogListView`** / row renderer — a scrollable, selectable list of rows with
  optional indent (the sidebar tree) + hit-test → selected index.
- **`ogPanel` / divider / key-value grid helpers** — the detail-panel chrome.

These extend objGFX incrementally as phases need them; `bin/diskutil` stays thin
(data + layout + event routing), drawing via the shared widgets. Mouse
hit-testing follows `bin/views/settings`' click-dispatch pattern, but the
hit-tested widgets themselves live in objGFX.

> Principle: **no proprietary UI in the app.** If a phase needs a new visual
> element, it lands in objGFX (documented in `docs/apps/objgfx-reference.md`)
> before the app uses it.

## 4. Phases

**Phase 0 — data layer (no UI).** `include/api/ubix_disk.h` DTO; `sys_disk_query`
(slot 68, both arches) walking the block registry; `lib/ubix_api/ubixdisk.c`
thunk. Verify with a throwaway CLI dump (`bin/ubdisk`? or reuse `ubpool`-style)
so the data is proven before any pixels.

**Phase 1 — MVP read-only GUI.** `bin/diskutil` (views app): sidebar tree from
`ubix_disk_query`, default-select first disk, click to select; detail grid +
partition bar + capacity bar (used/free via `ubix_pool_query` for UbixFS
volumes). Toolbar drawn but only **Info** live (others greyed). Window
resize-aware. Staged in the desktop profile (`mkimage*` `DESKTOP_BINS`).
**Taskbar integration:** add a **Utilities** category to the taskbar launcher
(alongside Games) and place Disk Utility in it — so it launches from the menu,
not just a shell. (The taskbar app-menu lives in `bin/views/` taskbar code; this
is the first Utilities entry, so it also establishes the category.)

**Phase 2 — Mount / Unmount.** Toolbar Mount/Unmount → `mount()`/`unmount()` on
the selected partition (the fstab/automountd plumbing already exists); refresh
the tree. Lowest-risk write action.

**Phase 3 — Erase / format.** Confirm dialog → format the partition: FAT32 (a
`mkfs.fat`-equivalent) or a fresh UbixFS pool (`ubfs mkpool` core primitive
already exists — surface it as a syscall or a helper). Needs Phase 0b raw access.

**Phase 4 — Partition.** Edit the MBR: add/delete/resize partitions, write
sector 0. The riskiest action — strong confirmation, never touch the mounted
root. Needs Phase 0b + an MBR-writer (reuse `sys/dev/partition.h` structs).

**Phase 5 — Advanced.** First Aid (fsck per FS), GPT support (the parser exists,
`sys/fs/common/gpt.c`), USB hotplug **live updates** (subscribe to the
`automountd` storage MPI events — `MPI_STORAGE_APPEARED/DEPARTED`), drive
model/serial + SMART (ATA IDENTIFY surfaced into the DTO `model` field).

## 5. Decisions / open questions

- **Name:** `bin/diskutil` (GUI). A future CLI can share `ubix_disk_query`.
- **Data layer = syscall, decided** (vs raw `/dev` parse) — cross-arch + typed +
  gives geometry. Cost: one new native syscall + per-arch enumeration glue.
- **MVP is strictly read-only** — no way to lose data while the UI/data layer
  settle. All destructive actions gated behind later phases + confirmations.
- **GPT vs MBR:** MVP reports the MBR scheme (what uBixOS images use). GPT
  read-support is Phase 5 (parser already present); GPT *write* is far future.
- **Capacity/used-free:** trivial for UbixFS (`ubix_pool_query`); FAT needs a BPB
  read (Phase 1 can show capacity-only for FAT, used/free TBD).

## 6. Cross-arch + multi-agent notes

- Build + verify on **both** arches (i386 desktop, aarch64 desktop) every phase.
- New files only (`bin/diskutil/`, `include/api/ubix_disk.h`,
  `lib/ubix_api/ubixdisk.c`) plus additive edits to `syscalls.c`/`sysproto.h`/the
  two arch syscall handlers + `bin/Makefile` SUBDIRS + `mkimage*` `DESKTOP_BINS`.
  Those last three are **shared files** — coordinate per `AGENTS-COORD.md` (the
  `[aural]` session also edits `bin/Makefile`/`mkimage*`).
- Wrap full `world`/`image` builds in `tools/buildlock.sh`.
```
