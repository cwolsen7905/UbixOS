# UbixOS Syscall Architecture

UbixOS has **two independent syscall tables**, dispatched by different interrupt vectors.

---

## Two Interrupt Vectors, Two Tables

| Vector | ABI | Dispatch | Source |
|--------|-----|----------|--------|
| `int $0x80` | POSIX / FreeBSD | `sys_call_posix.S` → `syscalls_posix.c` | `sys/arch/i386/` |
| `int $0x81` | UbixOS native | `sys_call.S` → `syscalls.c` | `sys/arch/i386/` |

POSIX syscall numbers follow the **FreeBSD i386 ABI** layout (e.g., `read=3`, `write=4`,
`fork=2`, `execve=59`).  Native syscall numbers are independent and assigned as new
kernel APIs are added.

---

## POSIX Syscall Path (`int $0x80`)

1. Userland places the syscall number in `%eax` and arguments on the stack.
2. `sys_call_posix.S` saves registers, indexes into the `syscalls_posix` table, and calls the handler.
3. Handlers are declared in `sys/include/sys/sysproto.h` and implemented in `sys/kernel/` and `sys/fs/vfs/`.
4. Return value is placed in `td->td_retval[0]`; negative values become `errno`.

The POSIX table is used by libc (`lib/libc/sys/`).  Most POSIX functions have assembly stubs
that set `%eax` and execute `int $0x80`.

### Key POSIX syscalls

| Slot | Name | Notes |
|------|------|-------|
| 1 | `exit` | |
| 2 | `fork` | `sys_fork` in `sys/arch/i386/fork.c` |
| 3 | `read` | |
| 4 | `write` | |
| 5 | `open` | routes through VFS |
| 11 | `execve` | ELF loader in `sys/arch/i386/i386_exec.c` |
| 20 | `getpid` | |
| 41 | `getvfscwd` | **UbixOS extension** — returns the cwd (now a plain POSIX path) |
| 49 | `getcwd` | POSIX `getcwd` — returns the cwd |
| 59 | `execve` (alt) | |

> Syscall 41 (`sys_getvfscwd`) is a UbixOS-specific slot retained from when paths
> used a `sys:/` mountpoint prefix and the two calls differed (full vs stripped).
> Now that cwd is a plain POSIX path, both return the same string. See
> [vfs.md](vfs.md).

---

## Native Syscall Path (`int $0x81`)

Used by the UbixOS-specific API in `lib/ubix_api/`.  Userland places the slot number in
`%eax` and executes `int $0x81`.  The table is in `sys/arch/i386/syscalls.c`.

### Key native syscalls

| Slot | Name | Handler | Notes |
|------|------|---------|-------|
| 50 | `mpiCreateMbox` | `sys_mpiCreateMbox` | MPI mailbox create |
| 51 | `mpiDestroyMbox` | `sys_mpiDestroyMbox` | |
| 52 | `mpiPostMessage` | `sys_mpiPostMessage` | |
| 53 | `mpiFetchMessage` | `sys_mpiFetchMessage` | |
| 43 | `mapfb` | `sys_mapfb` | Map VESA framebuffer into process |
| 44 | `getmouse` | `sys_getmouse` | Poll mouse event queue |
| 45 | `shareregion` | `sys_shareregion` | Share memory region into another process |

---

## Adding a New Syscall

**POSIX table** (`sys/arch/i386/syscalls_posix.c`):
1. Declare the handler signature in `sys/include/sys/sysproto.h`.
2. Add `{ ARG_COUNT(...), "name", (sys_call_t *)sys_name, SYSCALL_VALID }` at the correct FreeBSD slot index.
3. Implement in the appropriate subsystem directory.
4. Add a libc assembly stub in `lib/libc/sys/` if userland needs it.

**Native table** (`sys/arch/i386/syscalls.c`):
Same steps; use the next available slot number and add an assembly stub in `lib/ubix_api/` or `lib/libfb/` as appropriate.

---

## Argument Passing Convention

Arguments are passed on the **user stack** (not in registers), at `[esp+4]`, `[esp+8]`, etc.,
matching the standard i386 C calling convention.  The dispatch stubs collect them into an
`args` struct (defined in `sysproto.h`) that is passed by pointer to the handler.

The handler writes its return value into `td->td_retval[0]`.  The dispatch stub copies
this into `%eax` before returning to userland.
