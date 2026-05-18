# UbixOS Native FAT 12/16/32 Implementation Plan

## Background

The current FAT driver (`sys/fs/fat/`) is built on **fat_io_lib** by Ultra-Embedded.com,
licensed under **GPL v2**.  UbixOS uses a BSD-style license; including GPL code in a
statically linked kernel makes the combined binary GPL — incompatible with the project's
stated license.

Rather than swap in a third-party replacement (FatFs, etc.), we will write our own
implementation incrementally, replacing fat_io_lib piece by piece as we touch the FAT
subsystem for other fixes.  Each phase leaves the kernel in a bootable state; the old
library is removed only when Phase 5 is complete.

---

## Design Principles

- **Own the code** — no contrib libraries in the FAT layer; every line is ours to read,
  fix, and extend.
- **VFS seam is the contract** — the nine glue functions in `fat.c` are the only interface
  the rest of the kernel sees.  Nothing outside `sys/fs/fat/` changes until Phase 5.
- **Incremental, always bootable** — Phases 1–4 build new files alongside the old library;
  Phase 5 does the cutover and removes fat_io_lib.
- **Correct flush semantics from day one** — `fat_file_flush` commits both the data sector
  *and* the directory entry size.  (The existing bug — `fl_fflush` not updating the
  directory — is what broke logd; we patch fat_io_lib for now and get it right natively.)

---

## New Source Layout

```
sys/fs/fat/
    fat_internal.h      — shared types: fat_fs, fat_file, fat_dir_iter
    fat_bpb.c / .h      — BPB parse, geometry, FAT-type detection
    fat_sector.c / .h   — raw sector read/write, one-sector cache
    fat_table.c / .h    — FAT entry read/write, cluster alloc/free
    fat_dir.c / .h      — directory entry read/find/create/delete + LFN
    fat_file.c / .h     — open/read/write/seek/flush/close
    fat_vfs.c           — replaces fat.c: same VFS glue signatures
    Makefile            — updated to compile new files, drop old ones
```

Everything from fat_io_lib (`fat_filelib.*`, `fat_access.*`, `fat_cache.*`, `fat_write.*`,
`fat_table.*`, `fat_string.*`, `fat_misc.*`, `fat_format.*`) is deleted at Phase 5.

---

## Core Data Structures (`fat_internal.h`)

```c
#define FAT_TYPE_12  12
#define FAT_TYPE_16  16
#define FAT_TYPE_32  32

/* Mounted filesystem instance — one per mount point */
struct fat_fs {
    struct vfs_mountPoint *mp;
    uint8_t   type;                  /* 12, 16, or 32 */
    uint32_t  bytes_per_sector;      /* always 512 on UbixOS */
    uint8_t   sectors_per_cluster;
    uint32_t  fat_begin_lba;         /* absolute sector of first FAT */
    uint32_t  fat_sectors;           /* sectors per FAT copy */
    uint8_t   fat_copies;
    uint32_t  root_lba;              /* FAT12/16: fixed root region start */
    uint32_t  root_sectors;          /* FAT12/16: size of root region */
    uint32_t  root_cluster;          /* FAT32: cluster 2 = root */
    uint32_t  data_lba;              /* first data sector (cluster 2) */
    uint32_t  total_clusters;
    uint32_t  fsinfo_lba;            /* FAT32 only */
    uint32_t  free_cluster_hint;     /* FAT32 FSINFO next-free */
};

/* Open file state — stored in fileDescriptor_t.res */
struct fat_file {
    struct fat_fs *fs;
    uint32_t  start_cluster;
    uint32_t  cur_cluster;
    uint32_t  file_size;
    uint32_t  position;
    uint32_t  dir_sector;            /* sector holding directory entry */
    uint16_t  dir_offset;            /* byte offset within that sector */
    uint8_t   mode;                  /* FAT_MODE_R / _W / _A */
    uint8_t   size_dirty;            /* directory entry needs update */
    uint8_t   buf[512];              /* write-behind buffer */
    uint8_t   buf_dirty;
    uint32_t  buf_lba;               /* sector currently in buf */
};

/* Directory iterator — stored in kDIR_t.dirHandle */
struct fat_dir_iter {
    struct fat_fs *fs;
    uint32_t  cluster;               /* 0 = root for FAT12/16 */
    uint32_t  sector_in_cluster;
    uint16_t  entry_offset;          /* within sector, 0–480 step 32 */
    char      lfn[256];
    uint8_t   lfn_seq;
    uint8_t   lfn_checksum;
};

/* Raw 32-byte FAT directory entry (on-disk layout) */
struct fat_raw_dirent {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  nt_res;
    uint8_t  crt_time_tenth;
    uint16_t crt_time, crt_date, acc_date;
    uint16_t clus_hi;
    uint16_t wrt_time, wrt_date;
    uint16_t clus_lo;
    uint32_t file_size;
} __attribute__((packed));

/* Cluster sentinel values */
#define FAT_CLUSTER_FREE  0x00000000u
#define FAT_CLUSTER_BAD   0x0FFFFFF7u
#define FAT_CLUSTER_EOC   0x0FFFFFFFu   /* >= this = end of chain */

/* File open modes */
#define FAT_MODE_R  0x01
#define FAT_MODE_W  0x02
#define FAT_MODE_A  0x04
```

---

## Phase 1 — BPB Parsing + Filesystem Geometry

**Files:** `fat_bpb.h`, `fat_bpb.c`

**Goal:** read sector 0, verify the BPB signature, and populate `struct fat_fs`.  No file
I/O yet.  The new files are compiled but not linked into the kernel — Phase 5 wires them in.

```c
int fat_bpb_parse(struct fat_fs *fs);
```

**Steps inside `fat_bpb_parse`:**

1. Read sector 0 via `fs->mp->device->dev_blk_ops->read`.
2. Verify bytes 510–511 are `0x55 0xAA`.
3. Extract from the BPB:
   - `BytsPerSec`, `SecPerClus`, `RsvdSecCnt`, `NumFATs`, `RootEntCnt`
   - `FATSz16` / `FATSz32`, `TotSec16` / `TotSec32`
   - `RootClus` (FAT32), `FSInfo` (FAT32)
4. Compute derived fields:
   ```
   fat_begin_lba  = partition_offset + RsvdSecCnt
   root_lba       = fat_begin_lba + NumFATs * FATSz
   root_sectors   = (RootEntCnt * 32 + BytsPerSec - 1) / BytsPerSec
   data_lba       = root_lba + root_sectors          (FAT12/16)
                  = fat_begin_lba + NumFATs * FATSz32  (FAT32)
   total_clusters = (TotalSectors - data_lba) / SecPerClus
   ```
5. Determine FAT type using Microsoft's cluster-count method:
   - `total_clusters < 4085`  → FAT12
   - `total_clusters < 65525` → FAT16
   - else                     → FAT32

**Risk:** Low — read-only, no writes.

---

## Phase 2 — Sector Cache + FAT Table Operations

**Files:** `fat_sector.h`, `fat_sector.c`, `fat_table.h`, `fat_table.c`

### Sector layer

```c
int fat_sector_read (struct fat_fs *fs, uint32_t lba, void *buf);
int fat_sector_write(struct fat_fs *fs, uint32_t lba, const void *buf);
```

Thin wrappers over `dev_blk_ops->read/write`.  A single 512-byte global cache (one slot)
protected by the existing `fat_acquire`/`fat_release` mutex is sufficient for the initial
implementation.

### FAT table layer

```c
uint32_t fat_cluster_next       (struct fat_fs *fs, uint32_t cluster);
uint32_t fat_cluster_alloc      (struct fat_fs *fs, uint32_t after);
int      fat_cluster_free_chain (struct fat_fs *fs, uint32_t start);
int      fat_cluster_write_entry(struct fat_fs *fs, uint32_t cluster, uint32_t value);
uint32_t fat_cluster_to_lba    (struct fat_fs *fs, uint32_t cluster);
```

**FAT12 entry layout** (the tricky case):
- Entry for cluster `n` starts at byte offset `n + n/2` in the FAT.
- If `n` is even: use bits 11–0 of the 16-bit word at that offset.
- If `n` is odd: use bits 15–4 of the 16-bit word at that offset.

FAT16: 16-bit entry at offset `n * 2`.  FAT32: 28-bit entry (mask `0x0FFFFFFF`) at offset
`n * 4`.  All three share `fat_cluster_next` via a `switch (fs->type)`.

**`fat_cluster_alloc`:**
1. Scan FAT from `free_cluster_hint` (FAT32) or cluster 2 (FAT12/16).
2. Find first entry equal to `FAT_CLUSTER_FREE`.
3. Write `FAT_CLUSTER_EOC` to the new entry.
4. Write `FAT_CLUSTER_EOC` to `after` (extending the chain) or leave as chain head.
5. Update `free_cluster_hint`.

**Risk:** Medium — FAT12 bit-packing requires care; write a unit-testable helper.

---

## Phase 3 — Directory Read + LFN

**Files:** `fat_dir.h`, `fat_dir.c`

```c
int fat_dir_iter_open (struct fat_fs *fs, uint32_t cluster, struct fat_dir_iter *it);
int fat_dir_iter_next (struct fat_dir_iter *it, char *name_out,
                       struct fat_raw_dirent *entry_out,
                       uint32_t *entry_sector, uint16_t *entry_offset);

int fat_dir_find      (struct fat_fs *fs, uint32_t dir_cluster, const char *name,
                       struct fat_raw_dirent *out,
                       uint32_t *entry_sector, uint16_t *entry_offset);

int fat_dir_create_entry(struct fat_fs *fs, uint32_t dir_cluster, const char *name,
                         uint8_t attr, uint32_t start_cluster,
                         uint32_t *entry_sector, uint16_t *entry_offset);
int fat_dir_delete_entry(struct fat_fs *fs, uint32_t entry_sector, uint16_t entry_offset);
int fat_dir_update_size (struct fat_fs *fs, uint32_t entry_sector, uint16_t entry_offset,
                         uint32_t new_size);

int fat_path_resolve  (struct fat_fs *fs, const char *path,
                       uint32_t *dir_cluster_out,
                       struct fat_raw_dirent *file_entry_out,
                       uint32_t *entry_sector, uint16_t *entry_offset);
```

### LFN handling in `fat_dir_iter_next`

- Attribute `0x0F` with cluster fields zero = LFN entry.
- `seq & 0x1F` = position (1-based); `seq & 0x40` set = last (outermost) entry in the run.
- Each LFN entry carries 13 UTF-16LE code points across three fields (`name1[5]`,
  `name2[6]`, `name3[2]`); strip the null terminator and trailing `0xFFFF` padding.
- Entries arrive in descending sequence order; reconstruct `lfn[]` at offset
  `(seq - 1) * 13`.
- Verify the checksum (sum-with-rotation of the 11 SFN bytes) against the following SFN
  entry; fall back to the 8.3 name on mismatch.

### Path traversal in `fat_path_resolve`

Split on `/`, resolve each component with `fat_dir_find` starting from the root cluster,
handle `.` (stay) and `..` (walk `clus_hi:clus_lo` of `..` entry).

**Creating a new file:** `fat_dir_create_entry` scans the directory for a free slot
(first byte of `name` is `0xE5` = deleted, or `0x00` = never used), writes an SFN entry
plus LFN entries if the name exceeds 8.3.

**SFN generation:** uppercase, strip illegal characters, truncate to 8+3, append a numeric
tail (`~1`, `~2` …) if a collision exists in the directory.

**Risk:** Medium — LFN sequence ordering, checksum, and SFN tail generation.

---

## Phase 4 — File I/O

**Files:** `fat_file.h`, `fat_file.c`

```c
struct fat_file *fat_file_open (struct fat_fs *fs, const char *path, uint8_t mode);
int              fat_file_read (struct fat_file *f, void *buf, uint32_t size, uint32_t *got);
int              fat_file_write(struct fat_file *f, const void *buf, uint32_t size);
int              fat_file_seek (struct fat_file *f, uint32_t pos);
int              fat_file_flush(struct fat_file *f);
void             fat_file_close(struct fat_file *f);
```

### `fat_file_read`

Translate `f->position` into `(cluster, sector_in_cluster, byte_in_sector)`.  For
sequential reads keep `f->cur_cluster` valid — only call `fat_cluster_next` when crossing
a cluster boundary.  Copy into the caller's buffer one sector at a time; update
`f->position`.

### `fat_file_write`

Same cluster walk.  For partial sectors: read the sector into `f->buf`, modify, mark
`buf_dirty`.  For full sectors: write directly.  Allocate new clusters via
`fat_cluster_alloc` when the position reaches the end of the current chain.  Set
`f->size_dirty = 1` whenever `position > file_size`.

### `fat_file_flush` — correct by design

```c
int fat_file_flush(struct fat_file *f) {
    if (f->buf_dirty) {
        fat_sector_write(f->fs, f->buf_lba, f->buf);
        f->buf_dirty = 0;
    }
    if (f->size_dirty) {
        fat_dir_update_size(f->fs, f->dir_sector, f->dir_offset, f->file_size);
        f->size_dirty = 0;
    }
    return 0;
}
```

Both data *and* the directory entry are committed on every flush.  This is the fix for the
`fl_fflush` bug that kept logd's log file empty.

### Open modes

- `FAT_MODE_R`: find the file; fail if not found.
- `FAT_MODE_W`: find or create; truncate (free chain, set size 0) if found.
- `FAT_MODE_A`: find or create; seek to EOF after open.

**`fat_file_close`:** flush, then free the `struct fat_file` via `kfree`.

**Risk:** Medium — cluster chain walk for seeks into large files, partial-sector
read-modify-write.

---

## Phase 5 — VFS Glue + Cutover

**Files:** `fat_vfs.c` (replaces `fat.c`), updated `Makefile`

```c
static struct fat_fs  _fatfs;
static volatile int   fat_busy = 0;

static void fat_acquire(void) {
    while (__sync_lock_test_and_set(&fat_busy, 1))
        sched_yield();
}
static void fat_release(void) {
    __sync_lock_release(&fat_busy);
}

int fat_initialize(struct vfs_mountPoint *mp) {
    _fatfs.mp = mp;
    return fat_bpb_parse(&_fatfs) == 0 ? 1 : 0;
}

int open_fat(const char *path, fileDescriptor_t *fd) {
    uint8_t mode = (fd->mode & fileRead)   ? FAT_MODE_R :
                   (fd->mode & fileAppend) ? FAT_MODE_A : FAT_MODE_W;
    fat_acquire();
    struct fat_file *f = fat_file_open(&_fatfs, path, mode);
    fat_release();
    if (!f) return 0;
    fd->res  = f;
    fd->size = f->file_size;
    fd->ino  = f->start_cluster;
    return 1;
}

int read_fat(fileDescriptor_t *fd, char *data, off_t offset, long size) {
    struct fat_file *f = fd->res;
    uint32_t got = 0;
    fat_acquire();
    fat_file_seek(f, (uint32_t)offset);
    fat_file_read(f, data, (uint32_t)size, &got);
    fat_release();
    return (int)got;
}

int write_fat(fileDescriptor_t *fd, char *data, off_t offset, long size) {
    struct fat_file *f = fd->res;
    fat_acquire();
    fat_file_write(f, data, (uint32_t)size);
    fat_file_flush(f);
    fat_release();
    return (int)size;
}

int fat_opendir(const char *path, kDIR_t *dir) {
    struct fat_dir_iter *it = kmalloc(sizeof(struct fat_dir_iter));
    if (!it) return 0;
    uint32_t cluster;
    fat_acquire();
    int ok = fat_path_to_dir_cluster(&_fatfs, path, &cluster);
    fat_release();
    if (!ok) { kfree(it); return 0; }
    fat_dir_iter_open(&_fatfs, cluster, it);
    dir->dirHandle = it;
    return 1;
}

int fat_readdir(kDIR_t *dir, struct kdirent *ent) {
    struct fat_dir_iter *it = dir->dirHandle;
    struct fat_raw_dirent raw;
    char name[256];
    uint32_t sec; uint16_t off;
    fat_acquire();
    int r = fat_dir_iter_next(it, name, &raw, &sec, &off);
    fat_release();
    if (r != 0) return -1;
    ent->d_ino  = ((uint32_t)raw.clus_hi << 16) | raw.clus_lo;
    ent->d_type = (raw.attr & 0x10) ? KDT_DIR : KDT_REG;
    strncpy(ent->d_name, name, 255);
    ent->d_name[255] = '\0';
    return 0;
}

int fat_closedir(kDIR_t *dir) {
    if (dir->dirHandle) { kfree(dir->dirHandle); dir->dirHandle = NULL; }
    return 0;
}

int mkdir_fat(char *path, void *fd) {
    /* allocate a cluster, write . and .. entries, create directory entry */
    fat_acquire();
    int r = fat_dir_mkdir(&_fatfs, path);
    fat_release();
    return r;
}

int rmdir_fat(char *path, void *fd) {
    fat_acquire();
    int r = fat_dir_rmdir(&_fatfs, path);
    fat_release();
    return r;
}

int unlink_fat(const char *path) {
    fat_acquire();
    int r = fat_dir_unlink(&_fatfs, path);
    fat_release();
    return r;
}
```

**`fat_init`** is identical to the current one — build the `fileSystem` struct with the
same callback pointers and call `vfsRegisterFS`.

**Makefile cutover:** remove all `fat_io_lib` `.c` entries, add the six new files.  The
GPL library source stays in the tree under `contrib/fat_io_lib_archive/` for reference
until we are confident the new driver is stable, then deleted.

---

## Phase Sizing

| Phase | New lines (est.) | Risk |
|---|---|---|
| 1 BPB parse | ~150 | Low — read-only |
| 2 FAT table | ~250 | Medium — FAT12 bit packing |
| 3 Directory | ~400 | Medium — LFN sequencing, SFN tails |
| 4 File I/O | ~350 | Medium — cluster chain walk |
| 5 VFS glue | ~150 | Low — mechanical mapping |
| **Total** | **~1 300** | |

Replaces ~4 000 lines of GPL code with ~1 300 lines we own, understand, and can extend.

---

## Status

| Phase | Status | Files | Notes |
|---|---|---|---|
| 1 — BPB parse | Done | `fat_internal.h`, `fat_bpb.h`, `fat_bpb.c` | Compiles clean; not yet called |
| 2 — Sector cache + FAT table | Done | `fat_sector.h/c`, `fat_clust.h/c` | `fat_table.h/c` name taken by fat_io_lib; renamed to fat_clust. Compiles clean; not yet called. |
| 3 — Directory + LFN | Done | `fat_dir.h`, `fat_dir.c` | mkdir/rmdir/unlink included here. Compiles clean; not yet called. |
| 4 — File I/O | Done | `fat_file.h`, `fat_file.c` | Compiles clean; not yet called. |
| 5 — VFS glue + cutover | Done | `fat_vfs.c`, `Makefile` | fat_io_lib removed from build. Also patched `file.c`, `vfs_calls.c`, `shutdown.c` to remove fl_* calls. fat_io_lib source still in tree for reference. |

---

## Future Extensions (post-Phase 5)

- **Multi-sector cache** — replace the single-slot sector cache with an LRU buffer (8–16
  slots) to reduce disk I/O on sequential access.
- **FAT mirroring** — write both FAT copies (currently only FAT copy 0 is updated).
- **`ftruncate` support** — expose via a new `vfsTruncate` VFS callback.
- **`O_SYNC` mode** — per-fd flag to skip the write buffer entirely.
- **FAT12 floppy support** — test with a 1.44 MB image; the type-12 cluster code should
  work already.
- **Timestamps** — write correct `wrt_time`/`wrt_date` using the kernel's RTC.
- **FSINFO writeback** — update the FAT32 FSINFO sector's `Free_Count` and `Nxt_Free`
  fields on cluster alloc/free.
