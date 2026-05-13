# UbixOS VFS Architecture

**Source:** `sys/fs/vfs/`

---

## Overview

All filesystem calls route through the VFS layer.  Concrete drivers (FAT, ubixfs, devfs, etc.)
register a vtable of function pointers; callers always go through VFS dispatch — never call
driver functions directly from outside `sys/fs/vfs/`.

---

## Path Format and Mountpoints

UbixOS paths use a `<mountpoint>:/path` prefix convention:

```
sys:/bin/shell        — file "shell" under /bin on the "sys" mount
sys:/etc/userdb       — user database
```

The kernel tracks the **full VFS path** (including mountpoint) in `_current->oInfo.cwd`.
Example: after `cd /bin`, cwd is `sys:/bin/`.

`sys_chdir` preserves the mountpoint prefix for bare `/` paths: if the user does
`cd /`, cwd becomes `sys:/`, not just `/`.

---

## Dual `getcwd` Design

Two syscalls return the current directory, for different audiences:

| Syscall | Slot | Vector | Returns | Purpose |
|---------|------|--------|---------|---------|
| `sys_getcwd` | 49 | `int $0x80` | `/bin/` (no mountpoint) | POSIX compatibility |
| `sys_getvfscwd` | 41 | `int $0x80` | `sys:/bin/` (full path) | UbixOS native apps |

`sys_getcwd` strips the mountpoint prefix before returning, so POSIX programs that parse
the result see a standard absolute path.  `sys_getvfscwd` returns the full internal path.

Userland access:
- `getcwd(buf, size)` in libc calls `sys_getcwd` — for portable code.
- `ubix_getcwd(buf, size)` in `lib/ubix_api/ubixcwd.c` calls `sys_getvfscwd` via slot 41.

The shell uses `ubix_getcwd()` for its prompt so it can display the full `sys:/bin/#` path.
POSIX apps that call `getcwd()` are unaffected.

---

## Shell Path Handling

The shell treats paths containing `:` as absolute VFS paths, passing them directly to
`execve` without prepending cwd.  This allows `sys:/bin/hello` to work from any directory.

`execve` sets the initial cwd of the new process to `sys:/` (set in
`sys/arch/i386/i386_exec.c`).

---

## Filesystem Drivers

Each driver registers with VFS at boot.  Currently mounted drivers:

| Name | Source | Mounted as |
|------|--------|-----------|
| FAT32 | `sys/fs/fat/` | `sys:` — root filesystem on disk image |
| devfs | `sys/fs/devfs/` | device namespace |
| ubixfs | `sys/fs/ubixfs/` | (secondary; in-memory) |

The FAT driver uses `hdRead` from the IDE driver.  `hdRead` adds the partition offset
(`parOffset`, LBA 2048 for the disk image) transparently — do not add the offset again in
the FAT layer when computing sector addresses.
