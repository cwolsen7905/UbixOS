# VFS Unix Path Overhaul + Automount Daemon

## Overview

Replace UbixOS's `mountpoint:/path` VFS namespace with a standard POSIX
single-rooted namespace.  A userspace automount daemon (`automountd`) handles
all mount policy via MPI, leaving the kernel responsible only for mounting the
root filesystem at boot.

**Goal:** `open("/bin/ls")`, `open("/dev/tty")`, `open("/proc/self/fd/0")` work
natively without any kernel-side path translation hacks.

---

## Status

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | VFS mount table: name-keyed → path-keyed | [x] Done |
| 1b | Remove `sys:/`, `devfs:/`, `proc:/` string constants | [x] Done |
| 1c | CWD format: `sys:/bin/` → `/bin/` | [x] Done |
| 2 | `sys_mount` / `sys_umount` POSIX syscalls | [ ] Not started |
| 3 | `/etc/fstab` config file + `automountd` skeleton | [ ] Not started |
| 4 | `automountd` static mounts (devfs, procfs at boot) | [ ] Not started |
| 5 | Kernel MPI device notifications (USB, IDE) | [ ] Not started |
| 6 | `automountd` dynamic auto-mount at `/mnt/<volname>` | [ ] Not started |

---

## Phase 1 — VFS Mount Table: Name-Keyed → Path-Keyed

**Files:** `sys/fs/vfs/vfs.c`, `sys/fs/vfs/file.c`, `sys/include/fs/vfs/vfs.h`

### What changes

`vfs_mountPoint` currently stores a `name` string (e.g. `"sys"`, `"devfs"`).
Change it to a `mountpath` string (e.g. `"/"`, `"/dev"`, `"/proc"`).

```c
/* Before */
struct vfs_mountPoint {
    char name[32];
    struct vfsOps *fs;
    ...
};

/* After */
struct vfs_mountPoint {
    char mountpath[256];   /* POSIX path, e.g. "/" or "/dev" */
    struct vfsOps *fs;
    ...
};
```

`vfs_findMount(const char *name)` → `vfs_find_mount_for(const char *path)`:
walks the mount table and returns the entry whose `mountpath` is the longest
prefix of `path`.  Standard Unix semantics — `/proc/self/fd` matches `/proc`
before `/`.

```c
struct vfs_mountPoint *vfs_find_mount_for(const char *path);
```

`fopen()` and `vfs_opendir()`: after finding the mount, strip the `mountpath`
prefix from the path to obtain the FS-relative path passed to the driver.

```
path="/proc/self/fd/1"  →  mount="/proc"  →  fs_path="/self/fd/1"
path="/bin/ls"          →  mount="/"      →  fs_path="/bin/ls"
path="/dev/tty"         →  mount="/dev"   →  fs_path="/tty"
```

Remove all colon-split logic (`strstr(path, ":")`, `strtok_r(path, ":")`).

### Boot registration

Change the FAT boot drive registration from `vfs_mount("sys", ...)` to
`vfs_mount("/", ...)`.  devfs and procfs kernel-side registrations are removed
entirely — they will be mounted by `automountd` from userspace (Phase 4).

### Path normalizer hacks to delete

Once Phase 1 is complete, remove all translation shims added for compat:
- `kern_openat`: `/dev/X` → `devfs:/X`, `/proc/X` → `proc:/X`, `/X` → `sys:/X`
- `fopen()`: same normalizers
- `vfs_opendir()`: same normalizers

---

## Phase 1b — Remove `sys:/`, `devfs:/`, `proc:/` String Constants

**Files:** scattered across `sys/`, `bin/`, `lib/`

Audit and replace every hardcoded `mountpoint:/path` string in the codebase.

Key locations:
- `sys/kernel/main.c` — early kernel exec paths (`sys:/bin/init`)
- `sys/arch/i386/i386_exec.c` — `execFile("sys:/bin/...")`
- `bin/init/main.c` — child process paths
- `bin/login/main.c` — shell path, PATH env var (already partially fixed)
- `bin/shell/exec.c` — `path_dirs[]`, PATH env
- `sys/fs/vfs/` — any remaining default-mount fallbacks

Script to audit:
```sh
grep -r "sys:/" sys/ bin/ lib/ --include="*.c" --include="*.h" -l
grep -r "devfs:/" sys/ bin/ lib/ --include="*.c" --include="*.h" -l
grep -r "proc:/" sys/ bin/ lib/ --include="*.c" --include="*.h" -l
```

---

## Phase 1c — CWD Format: `sys:/bin/` → `/bin/`

**Files:** `sys/fs/vfs/file.c` (`sys_chdir`), `sys/arch/i386/i386_exec.c`,
`sys/kernel/vfs_calls.c` (`sys_getcwd`), `lib/ubix_api/ubix_getcwd.c`

`_current->oInfo.cwd` is currently stored as `sys:/bin/`.  Change all
read/write sites to use plain POSIX paths.

- `sys_chdir`: stores new CWD as `/newpath/`
- `sys_getcwd`: returns `_current->oInfo.cwd` directly (no mountpoint strip needed)
- `execFile` / `sys_exec`: initialize CWD to `/` for new processes
- Shell prompt: already reads CWD from `ubix_getcwd()` — will now show `/bin/`
  instead of `sys:/bin/` automatically

The native `sys_getvfscwd` (slot 41) can be removed or aliased to `sys_getcwd`
since the distinction between "VFS path" and "POSIX path" disappears.

---

## Phase 2 — `sys_mount` / `sys_umount` Syscalls

**Files:** `sys/kernel/syscalls_posix.c`, `sys/kernel/vfs_calls.c` (or new
`sys/fs/vfs/mount.c`)

FreeBSD ABI syscall numbers: `mount` = 21, `umount` = 22.

```c
int sys_mount(struct thread *td, struct sys_mount_args *args);
/* args: type (fstype string), path (mountpoint), flags, data */

int sys_umount(struct thread *td, struct sys_umount_args *args);
/* args: path (mountpoint), flags */
```

`sys_mount`:
1. Validate `args->path` is an existing directory
2. Look up filesystem driver by `args->type` (`"fat"`, `"devfs"`, `"procfs"`)
3. Allocate and register a `vfs_mountPoint` at `args->path`
4. For block-device FSes, open the device and pass to the driver's `vfsMount` op

`sys_umount`:
1. Find mount by path
2. Call driver's `vfsUmount` op
3. Remove from mount table

The kernel's early boot mount of `/` bypasses these syscalls and goes directly
to the internal registration — same as today.

---

## Phase 3 — `/etc/fstab` + `automountd` Skeleton

**Files:** `etc/fstab` (new), `bin/automountd/` (new)

### `/etc/fstab` format

```
# device       mountpoint   fstype    options
none            /dev         devfs     defaults
none            /proc        procfs    defaults
none            /mnt         auto      automount
```

- `device` = block device path or `none` for pseudo-FSes
- `mountpoint` = POSIX path
- `fstype` = `devfs`, `procfs`, `fat`, `auto`
- `auto` fstype on a directory marks it as the landing zone for removable media

### `automountd` structure

```c
/* bin/automountd/main.c */
int main(void) {
    fstab_entry_t entries[FSTAB_MAX];
    int n = fstab_parse("/etc/fstab", entries, FSTAB_MAX);

    /* Phase 4: mount static entries */
    for (int i = 0; i < n; i++) {
        if (strcmp(entries[i].fstype, "auto") != 0)
            mount_entry(&entries[i]);
    }

    /* Phase 6: event loop for removable media */
    mpi_mailbox_t mb = mpi_open(AUTOMOUNTD_MAILBOX);
    for (;;) {
        mpi_msg_t msg;
        mpi_recv(mb, &msg);
        handle_storage_event(&msg);
    }
}
```

`init` starts `automountd` as one of its first children, before `login`, so
`/dev` and `/proc` are live by the time any interactive process runs.

---

## Phase 4 — `automountd` Static Mounts (devfs, procfs)

**Files:** `bin/automountd/main.c`, `bin/automountd/fstab.c`

`automountd` reads `/etc/fstab` at startup and calls `mount(2)` for each
non-`auto` entry in order:

```c
static void mount_entry(fstab_entry_t *e) {
    mkdir(e->mountpoint, 0755);   /* ensure dir exists */
    if (mount(e->fstype, e->mountpoint, 0, NULL) < 0)
        fprintf(stderr, "automountd: mount %s failed\n", e->mountpoint);
}
```

After this phase:
- `/dev/tty`, `/dev/ttyv0`–`/dev/ttyv3` are accessible via devfs at `/dev/`
- `/proc/self/fd/N` is accessible at `/proc/`
- No path translation needed anywhere in the kernel

---

## Phase 5 — Kernel MPI Device Notifications (USB, IDE)

**Files:** `sys/pci/usb/usb_storage.c`, `sys/pci/ide/ide.c`,
`include/mpi/storage.h` (new)

### MPI message definition

```c
/* include/mpi/storage.h */
#define MPI_STORAGE_APPEARED  0x5100
#define MPI_STORAGE_DEPARTED  0x5101

typedef struct {
    uint32_t type;           /* MPI_STORAGE_APPEARED / DEPARTED */
    char     dev_path[64];   /* e.g. "/dev/sda1", "/dev/uba1" */
    char     volume_name[32];/* FAT volume label, e.g. "UBIX" */
    char     fstype[16];     /* "fat", "ext2", ... */
    char     mountpath[256]; /* filled by automountd on DEPARTED */
} mpi_storage_msg_t;
```

USB storage driver sends `MPI_STORAGE_APPEARED` when a device enumerates and
a valid FAT BPB is found.  IDE driver does the same for hot-inserted media
(if supported).

`DEVICE_DEPARTED` is sent when USB disconnect is detected.

The kernel does not decide where to mount — it only reports what appeared.

---

## Phase 6 — `automountd` Dynamic Auto-Mount at `/mnt/<volname>`

**Files:** `bin/automountd/main.c`

```c
static void handle_storage_event(mpi_msg_t *msg) {
    mpi_storage_msg_t *s = (mpi_storage_msg_t *)msg->data;
    char mntpath[256];

    if (s->type == MPI_STORAGE_APPEARED) {
        /* lowercase the volume name for the mount path */
        char volname[33];
        for (int i = 0; s->volume_name[i]; i++)
            volname[i] = tolower((unsigned char)s->volume_name[i]);
        volname[strlen(s->volume_name)] = '\0';

        snprintf(mntpath, sizeof(mntpath), "/mnt/%s", volname);
        mkdir(mntpath, 0755);
        if (mount(s->fstype, mntpath, 0, NULL) == 0) {
            /* remember mountpath for DEPARTED */
            strncpy(s->mountpath, mntpath, sizeof(s->mountpath) - 1);
            printf("automountd: mounted %s at %s\n", s->dev_path, mntpath);
        }

    } else if (s->type == MPI_STORAGE_DEPARTED) {
        umount(s->mountpath);
        rmdir(s->mountpath);
        printf("automountd: unmounted %s\n", s->mountpath);
    }
}
```

USB volume "UBIX" → `/mnt/ubix`.  Volume "MY DATA" → `/mnt/my data` (spaces
preserved; can sanitize to `my_data` if preferred).

---

## Execution Order and Dependencies

```
Phase 1  →  Phase 1b  →  Phase 1c   (must be sequential — 1 unlocks 1b/1c)
Phase 2                              (independent, can overlap with 1b/1c)
Phase 3  →  Phase 4                  (3 is the skeleton, 4 fills it in)
Phase 5  →  Phase 6                  (5 is kernel side, 6 is userland side)

Phase 4 depends on Phase 2 (needs mount syscall).
Phase 6 depends on Phases 4 and 5.
```

## Effort Estimate

| Phase | Effort |
|-------|--------|
| 1 — VFS core refactor | ~1 day |
| 1b — string constant cleanup | ~2–3 hrs |
| 1c — CWD format | ~1–2 hrs |
| 2 — mount/umount syscalls | ~2 hrs |
| 3 — fstab + automountd skeleton | ~2 hrs |
| 4 — static mounts at boot | ~1 hr |
| 5 — kernel MPI notifications | ~3 hrs |
| 6 — dynamic /mnt auto-mount | ~2 hrs |
| **Total** | **~2.5–3 days** |
