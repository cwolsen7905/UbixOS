# UbixFS v2 — carry-forward plan (POSIX-permission native filesystem)

> Goal: a native UbixOS filesystem with real POSIX ownership/permissions
> (`uid`/`gid`/`mode` + times, ACL-ready), to replace FAT as the place where
> permissions actually mean something. FAT/exFAT can't store any of this; UFS/FFS
> can but isn't mountable on the macOS dev host. UbixFS v2 is *ours* — we own the
> format and its tooling, so the macOS-mount gap is replaced by a host tool we
> control, and GRUB reads it directly.

## Heritage: it already exists (dormant)

`sys/fs/ubixfsv2/` holds a **BeFS-style** design (Dominic Giampaolo's Be File
System — allocation groups, `blockRun` extents, `dataStream` direct/indirect/
double-indirect, B+tree directories, inline `smallData`, a journal/log area).
It is currently **all commented out** and was built as a **host-side prototype**
(`main.cpp` formats a RAM-drive and makes a dir). Critically, the inode already
carries `uid`, `gid`, `mode`, `flags` and an `attributes` extent — POSIX perms
(and BeFS-style extended attributes → ACLs later) were the intent from day one.

We carry that design forward, clean it up, and finish it.

## The core architectural rule: one format, three readers

The on-disk format will be read/written by **three independent codebases**. The
discipline that keeps this sane: **freeze the format in one shared, dependency-free
C header and a pure-C core library; everything else is a thin adapter.**

```
            include/fs/ubixfs2/format.h     <- on-disk structs (THE spec, plain C)
                        |
            lib/ubixfs2_core/  (pure C)     <- format logic: superblock parse,
              read layer  (all 3)              B+tree walk, path lookup, inode
              write layer (kernel + tool)      read, dataStream resolve; alloc,
                        |                       B+tree insert, journal (write)
        +---------------+-----------------+-----------------+
        |               |                 |
   kernel driver    host tool          GRUB module
   (sys/fs/ubixfs2) (tools/mkubixfs)   (grub fs/ubixfs2.c)
   VFS adapter,     file-backed dev,   grub_fs adapter,
   read+write       format+populate,   READ-ONLY
                    runs on macOS
```

- **`ubixfs2_core` is block-I/O-agnostic**: it never calls `read()`/`malloc()`
  directly — the caller supplies a `read_block(ctx, blkno, buf)` (and `write_block`
  for the write layer) callback and any scratch buffers. That single library is
  what the kernel, the host tool, and GRUB all link, so the format logic exists
  **once**. (GRUB links only the read layer.)
- The format header is plain C with fixed-width types and explicit
  little-endian (i386) — usable verbatim from C, C++, and GRUB.

This is the SOLID move: the *policy* (format) is one thing; the *mechanisms*
(where blocks come from) are injected per consumer.

## On-disk format (carried forward + finalized)

Block size **4096**. Little-endian. Allocation groups (AGs) with a block bitmap.
Extents via `blockRun {AG, start, len}`. Sketch (finalized from the existing
`ubixfsInode`/`diskSuperBlock`):

```c
/* include/fs/ubixfs2/format.h — the single source of truth */
#define UBIXFS2_MAGIC1   0x0A0A0A0A
#define UBIXFS2_BLOCK    4096
#define UBIXFS2_NDIRECT  64
#define UBIXFS2_NAMELEN  256

typedef struct { int32_t ag; uint16_t start; uint16_t len; } blockrun_t; /* extent */

typedef struct {                 /* block 0 (or 1) of the volume */
    char       name[32];
    int32_t    magic1, byte_order, block_size, block_shift;
    int64_t    num_blocks, used_blocks;
    uint32_t   blocks_per_ag, ag_shift, num_ags, last_used_ag;
    int32_t    flags;            /* CLEAN / DIRTY */
    blockrun_t log;              /* journal area (phase 6) */
    int64_t    log_start, log_end;
    blockrun_t root_dir;         /* inode addr of "/" */
    int32_t    magic2;
    /* pad to one block */
} ubixfs2_super_t;

typedef struct {
    blockrun_t direct[UBIXFS2_NDIRECT];
    blockrun_t indirect, double_indirect;
    int64_t    size;
} ubixfs2_stream_t;

typedef struct {                 /* the inode (BeFS-style, perms built in) */
    int32_t    magic;
    blockrun_t inode_num, parent;
    char       name[UBIXFS2_NAMELEN];
    uint32_t   uid, gid;         /* POSIX ownership */
    uint32_t   mode;             /* POSIX type+perm bits (S_IF* | rwxrwxrwx) */
    uint32_t   flags, type;
    int64_t    atime, mtime, ctime;   /* ADDED: were commented out before */
    blockrun_t attributes;       /* extended attrs / ACLs (phase 7) */
    ubixfs2_stream_t blocks;     /* file data extents */
    uint32_t   ref_count;
    char       small_data[3200]; /* inline tiny files / dir B+tree root */
} ubixfs2_inode_t;
```

Directories are **B+trees** mapping `name -> inode_num` (the existing `btree.cpp`
is the starting point; `bTreeHeader` carried forward). Tiny dirs/files live inline
in `small_data` (BeFS trick) to avoid extra block I/O.

**Cleanups vs the dormant code:**
- Drop the `uPtr` union that mixed on-disk `inodeAddr` with in-memory pointers —
  on-disk structs hold only `blockrun_t`/offsets; in-memory handles are a separate
  struct in the core lib. (That union was the main format smell.)
- Add `atime/mtime/ctime` (POSIX requires them; they were commented out).
- Make endianness explicit; no `__attribute__((packed))` reliance for layout —
  fixed-width fields, sizes asserted with `_Static_assert`.

## The three adapters

### 1. Host tools — `tools/ubixfs2/` mtools-style CLI suite (the test harness)
Rather than a single makefs-style "rebuild the whole image" tool, a suite of
**mtools-style individual-file commands** that operate on an image file directly
(unprivileged, no mount — the property that makes `mtools` work for FAT):

| mtools | UbixFS2 | op |
|--------|---------|----|
| `mformat` | `u2fs mkfs <img> <size>` | format |
| `mdir`    | `u2fs ls <img> <path>`   | read dir |
| `mcopy` (out) | `u2fs cp <img>:<path> <hostfile>` | read file |
| `mcopy` (in)  | `u2fs cp <hostfile> <img>:<path>` | write file |
| `mmd`     | `u2fs mkdir <img> <path>` | create dir |
| `mdel`    | `u2fs rm <img> <path>`    | delete |

Because they're thin wrappers over `ubixfs2_core`, **building them IS building +
testing the FS read+write layer on the host** — format / copy-in / ls / copy-out /
rm / diff, all on macOS in milliseconds under `lldb`, before any kernel or GRUB.
The suite is the harness that de-risks every later phase.

`tools/mkimage.sh` keeps its current loop-of-copies shape — `mcopy -i img f ::/d`
becomes `u2fs cp f img:/d` — so the existing incremental workflow (copy in *and*
out) is preserved on a perm-capable FS. Sets `uid/gid/mode` per file. Links
`ubixfs2_core` (read+write).

**v0 simplifications (lifted in later phases):** single allocation group; one inode
per 4 KB block; **directories stored inline** (fixed-size entries in `small_data`,
B+tree deferred until a dir overflows); **direct extents only** (indirect/double
deferred); no journal (clean/dirty flag). Enough to prove the format + perms end to
end on the host.

### 2. GRUB module — read-only `ubixfs2.c`
GRUB needs only to **read the kernel** off the volume, so a read-only module:
implement GRUB's `grub_fs` ops (`.dir` iterate, `.open`, `.read`, `.close`,
`.label`, `.uuid`) by walking superblock → root B+tree → path → inode →
`dataStream`. Build it into the GRUB image via `grub-mkimage` (add `ubixfs2` to
the module list in `tools/mkimage.sh`'s GRUB step). It links the **read layer** of
`ubixfs2_core` (compiled into the GRUB module — GPLv3 there, which is fine: it
lives in the GRUB build). This is what lets `/boot/kernel` live on a UbixFS v2
partition instead of FAT.

### 3. Kernel driver — `sys/fs/ubixfs2/` (C, VFS function-pointer adapter)
The kernel VFS is **C function pointers** (`vfsRegisterFS(struct fileSystem)`,
FAT is `vfsType 0xFA`); the dormant code is a C++ `vfs_abstract` class, which
doesn't match. Carry forward the *design*, but implement the kernel driver in
**C** as a thin adapter over `ubixfs2_core`: register a `fileSystem` with
`vfsType` (say `0xF2`) and wire `read`/`write`/`open`/`readdir`/`mkdir`/`stat`/
`unlink`/`rename` to the core. `stat` returns the inode's `mode/uid/gid/times`.

## POSIX permissions — the other half (FS-agnostic kernel work)

Storing perms is useless without enforcement, and that part is largely
independent of which FS supplies the bits (the VFS already has a
`permission(inode, mask)` hook in `namei.c`):
- `stat`/`fstat` surface `st_mode`/`st_uid`/`st_gid`/times from the inode.
- `open`/`exec`/`access` check `mode` against the caller's `uid`/`gid` via the
  `namei` `permission()` hook (`MAY_READ`/`WRITE`/`EXEC`).
- New syscalls: `chmod`/`fchmod`, `chown`/`fchown`, `umask` (exists), honoring the
  x-bit in exec.
- Tasks already carry `uid`/`gid`; root (uid 0) bypasses checks.

Do this in parallel — it's what makes the feature real, and it lights up the
moment any perm-capable FS is mounted.

## Phased rollout (each phase independently testable)

1. **Freeze the format** — `include/fs/ubixfs2/format.h` + `_Static_assert`
   sizes. Revive `ubixfs2_core` read+write from the dormant C++ as **pure C**.
2. **Host tool** `tools/mkubixfs` (format + populate). *Testable entirely on
   macOS* — format an image, populate a tree, dump it back, verify perms. No
   kernel, no GRUB. Highest-confidence first step.
3. **GRUB read-only module** — boot a kernel from a UbixFS v2 partition (prove
   the format end-to-end through a second independent reader).
4. **Kernel driver — read path** — mount a `mkubixfs`-built image read-only,
   `ls`/`cat`/exec from it.
5. **Kernel perms enforcement** — `chmod`/`chown`/`access` + open/exec checks.
6. **Kernel driver — write path** — allocation, B+tree insert, then the journal
   (last; clean/dirty flag + fsck until then).
7. **Make it root** — `tools/mkimage.sh` builds a UbixFS v2 root partition with
   correct perms; GRUB loads the kernel from it (or keep a tiny FAT `/boot` as a
   hybrid for the macOS dev loop — decide at this phase). ACLs via the
   `attributes` extent come after.

## Open decisions

- **Hybrid `/boot` or full UbixFS v2 root?** GRUB can read either; the only thing
  a FAT `/boot` buys is macOS-mountability of the boot bits. Lean: full UbixFS v2
  root once the GRUB module is solid (simpler one-FS story); revisit if the dev
  loop hurts.
- **Journal now or later?** Defer to phase 6 — bootstrap non-journaled (clean/
  dirty flag) so we get a working FS fast, add journaling once the rest is proven.
- **Endianness/portability** of the format header — fix little-endian now so the
  GRUB module and host tool agree byte-for-byte.
```
