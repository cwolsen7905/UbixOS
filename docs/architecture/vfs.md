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
| `/` | FAT32 (root, disk image) | `vfs_mount(major, minor, 0, 0xFA, "/", "rw")` in [main.c](../../sys/init/main.c) |
| `/dev` | devfs | [sys/fs/devfs/](../../sys/fs/devfs/) |
| `/proc` | procfs | [sys/fs/procfs/](../../sys/fs/procfs/) |

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
prefix). A new process starts at `/` (`sprintf(oInfo.cwd, "/")` in
[i386_exec.c](../../sys/arch/i386/i386_exec.c)).

Two syscalls return the cwd, both now returning the POSIX path verbatim from
`oInfo.cwd`:

| Syscall | Slot / table | Notes |
|---------|--------------|-------|
| `sys_getcwd` | POSIX (`int $0x80`, #326) | libc `getcwd()` |
| `sys_getvfscwd` | native (`int $0x81`, slot 41) | `ubix_getcwd()` in `lib/ubix_api/` |

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
