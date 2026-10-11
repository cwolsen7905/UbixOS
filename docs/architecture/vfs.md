# UbixOS VFS Architecture

**Source:** `sys/fs/vfs/`

---

## Overview

All filesystem calls route through the VFS layer. Concrete drivers (FAT, ubixfs,
devfs, procfs, etc.) register a vtable of function pointers; callers always go
through VFS dispatch — never call driver functions directly from outside
`sys/fs/vfs/`.

---

## Path Format and Mountpoints

UbixOS uses **POSIX absolute paths** — `/bin/sh`, `/etc/userdb`, `/dev/tty`. The
older `<mountpoint>:/path` (`sys:/bin/…`) convention has been removed.

Mount points are themselves POSIX paths. At boot:

| Mount point | Driver | Source |
|-------------|--------|--------|
| `/` | UbixFS pool on the virtio-blk (or SD) partition if it mounts and holds `/lib/libc.so`; otherwise the FAT32 partition (`vtblk0s1`) | `vfs_mount(1, minor, 0, VFS_TYPE_UBIXFS \| VFS_TYPE_FAT, "/", …)` in `kmain_aarch64` ([boot.c](../../sys/arch/aarch64/kern/boot.c)) and `kmain_x86_64` ([main.c](../../sys/arch/x86_64/boot/main.c)) |
| `/dev` | devfs | [sys/fs/devfs/](../../sys/fs/devfs/) |
| `/proc` | procfs | [sys/fs/procfs/](../../sys/fs/procfs/) — mounted in the x86_64 boot path; **TODO: verify** the aarch64 mount |
| `/boot` | FAT32 boot partition | aarch64, only when the pool is the root |
| `/pool` | UbixFS loopback pool | aarch64, only when FAT is the root and `/pool.img` exists |

(On i386, `releng/2`, the root was always the FAT32 disk image mounted from `sys/init/main.c`.)

`vfs_findMount(path)` ([mount.c](../../sys/fs/vfs/mount.c)) resolves a path to its
filesystem by **longest-matching mount prefix**, matched on whole path
components (so `/dev` does not match `/developer`). The mount point string passed
to `vfs_mount` is stored verbatim in `mp->mountPoint`.

A FAT volume's label (e.g. `SYS`, `UBIX`) is read from the BPB and stored in
`fat_fs->vol_label` for **informational use only** (a future automount daemon);
it does **not** affect the mount point — the FAT driver explicitly preserves the
POSIX `mp->mountPoint`.

---

## Current Working Directory

`_current->oInfo.cwd` holds a plain **POSIX absolute path** (no mountpoint
prefix). A process with an empty `cwd` is treated as being at `/` (the native/POSIX getcwd
handlers fall back to `"/"`). The i386 exec path that set `cwd` explicitly
(`sys/arch/i386/i386_exec.c`) exists only on `releng/2`. **TODO: verify** where the
64-bit exec path initialises `oInfo.cwd`.

Two syscalls return the cwd, both now returning the POSIX path verbatim from
`oInfo.cwd`:

| Syscall | Slot / table | Notes |
|---------|--------------|-------|
| `sys_getcwd` | POSIX table, #326 | libc `getcwd()` |
| `sys_getvfscwd` | native table, slot 41 (`NATIVE_GETCWD`, handled inline on both 64-bit arches) | `ubix_getcwd()` in `lib/ubix_api/` |

> **Legacy note:** the two syscalls existed to distinguish a POSIX-stripped path
> (`/bin/`) from a full mountpoint-prefixed path (`sys:/bin/`). Since cwd no
> longer carries a mountpoint prefix, both return the same string and the
> distinction is vestigial. `sys_getvfscwd` is retained for the native ABI.

---

## Shell Path Handling

The shell uses ordinary POSIX path resolution: absolute paths (`/bin/ls`) go
straight to `execve`; relative paths are resolved against cwd. There is no longer
a `:`-prefixed VFS-path special case. The prompt shows the POSIX cwd.

---

## Filesystem Drivers

Each driver registers a `fileSystem` vtable with VFS at boot and is attached to a
POSIX mount point:

| Name | Source | Mounted at |
|------|--------|-----------|
| FAT32 | [sys/fs/fat/](../../sys/fs/fat/) | `/` (root, disk image); additional FAT volumes (e.g. USB) mount at their own POSIX path |
| devfs | [sys/fs/devfs/](../../sys/fs/devfs/) | `/dev` |
| procfs | [sys/fs/procfs/](../../sys/fs/procfs/) | `/proc` |
| ubixfs / ubixfsv2 | `sys/fs/ubixfs*/` | secondary / experimental |

The FAT driver reads via `hdRead` from the IDE driver. `hdRead` adds the
partition offset (`parOffset`, LBA 2048 for the disk image) transparently — do
not add the offset again in the FAT layer when computing sector addresses.
