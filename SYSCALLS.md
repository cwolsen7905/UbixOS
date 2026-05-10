# UbixOS Syscall Reference

## Dispatch Architecture

Two separate IDT vectors, two separate tables. Set up in [sys/sys/idt.c](sys/sys/idt.c):

```
int 0x80  →  _sys_call_posix (sys_call_posix.S)  →  systemCalls_posix[]
int 0x81  →  _sys_call       (sys_call.S)         →  systemCalls[]
```

Both use the same calling convention:
- `%eax` = syscall number
- Arguments on the stack before the `int` instruction
- The assembly stub saves all registers, switches to the kernel data segment (`0x10`), calls the C dispatcher, then restores and `iret`s

The `td->abi` field (set from the ELF OS/ABI byte when a binary is exec'd) exists for finer-grained routing, but the branch inside each dispatcher that reads it is currently commented out. The interrupt vector alone selects the table.

### Adding a new syscall

1. Add an `args` struct to [sys/include/sys/sysproto_posix.h](sys/include/sys/sysproto_posix.h) (POSIX table) or [sys/include/sys/sysproto.h](sys/include/sys/sysproto.h) (native table)
2. Implement the handler in the appropriate kernel source file
3. Declare it in the header
4. Register it in [sys/kernel/syscalls_posix.c](sys/kernel/syscalls_posix.c) or [sys/kernel/syscalls.c](sys/kernel/syscalls.c) — replace the `SYSCALL_NOTIMP` slot at the desired number with `ARG_COUNT(...)`, the name string, the function pointer cast to `(sys_call_t*)`, and `SYSCALL_VALID`
5. Add a userland wrapper in `lib/libc_old/` using the inline-asm `int $0x80` / `int $0x81` pattern

---

## Table 1 — Native UbixOS (`int 0x81`) — `systemCalls[]`

Defined in [sys/kernel/syscalls.c](sys/kernel/syscalls.c).  
Dispatched by [sys/kernel/syscall.c](sys/kernel/syscall.c).  
Total slots: 55 (0–54). All unassigned slots call `sys_invalid`.

| # | Name | Handler | Description |
|---|------|---------|-------------|
| 40 | `sysSDE` | `sysSDE` | Software Display Environment — kernel graphics calls |
| 50 | `mpiCreateMbox` | `sys_mpiCreateMbox` | Create an MPI mailbox |
| 51 | `mpiDestroyMbox` | `sys_mpiDestroyMbox` | Destroy an MPI mailbox |
| 52 | `mpiPostMessage` | `sys_mpiPostMessage` | Post a message to an MPI mailbox |
| 53 | `mpiFetchMessage` | `sys_mpiFetchMessage` | Fetch a message from an MPI mailbox |

Everything else (0–39, 41–49, 54) calls `sys_invalid` and will print an error.

---

## Table 2 — POSIX/FreeBSD ABI (`int 0x80`) — `systemCalls_posix[]`

Defined in [sys/kernel/syscalls_posix.c](sys/kernel/syscalls_posix.c).  
Dispatched by [sys/kernel/syscall_posix.c](sys/kernel/syscall_posix.c).  
Numbers follow the **FreeBSD i386 ABI** layout except for 294–301 (UbixOS VFS extensions, marked `XXX - Wrong Spot` in the source).

All `libc_old` userland wrappers target this table via `int $0x80`.

| # | Name | Handler | Description |
|---|------|---------|-------------|
| 0 | `exit` | `sys_exit` | Terminate process |
| 1 | `fork` | `sys_fork` | Fork process |
| 2 | `read` | `sys_read` | Read from POSIX fd |
| 3 | `write` | `sys_write` | Write to POSIX fd |
| 4 | `open` | `sys_open` | Open file (POSIX fd) |
| 5 | `close` | `sys_close` | Close POSIX fd |
| 6 | `wait4` | `sys_wait4` | Wait for child process |
| 9 | `unlink` | `sys_unlink` | Delete a file |
| 11 | `chdir` | `sys_chdir` | Change working directory |
| 12 | `fchdir` | `sys_fchdir` | Change working directory via fd |
| 20 | `getpid` | `sys_getpid` | Get process ID |
| 23 | `setuid` | `sys_setUID` | Set user ID |
| 24 | `getuid` | `sys_getuid` | Get user ID |
| 25 | `geteuid` | `sys_geteuid` | Get effective user ID |
| 33 | `access` | `sys_access` | Check file access permissions |
| 39 | `getppid` | `sys_getppid` | Get parent process ID |
| 43 | `getegid` | `sys_getegid` | Get effective group ID |
| 47 | `getgid` | `sys_getgid` | Get group ID |
| 48 | `getlogin` | `sys_getlogin` | Get login name |
| 54 | `ioctl` | `sys_ioctl` | Device control |
| 58 | `readlink` | `sys_readlink` | Read symbolic link |
| 59 | `execve` | `sys_execve` | Execute a program |
| 72 | `munmap` | `sys_munmap` | Unmap memory region |
| 81 | `getpgrp` | `sys_getpgrp` | Get process group |
| 82 | `setpgid` | `sys_setpgid` | Set process group |
| 88 | `getFreePage` | `sysGetFreePage` | Allocate a free VMM page (UbixOS-specific) |
| 90 | `dup2` | `sys_dup2` | Duplicate file descriptor |
| 92 | `fcntl` | `sys_fcntl` | File control |
| 93 | `select` | `sys_select` | I/O multiplexing |
| 97 | `socket` | `sys_socket` | Create socket |
| 105 | `setsockopt` | `sys_setsockopt` | Set socket option |
| 116 | `gettimeofday` | `sys_gettimeofday` | Get current time |
| 128 | `rename` | `sys_rename` | Rename a file |
| 133 | `sendto` | `sys_sendto` | Send data on a socket |
| 181 | `setgid` | `sys_setGID` | Set group ID |
| 188 | `stat` | `sys_stat` | Get file status |
| 189 | `fstat` | `sys_fstat` | Get file status via fd |
| 190 | `lstat` | `sys_lstat` | Get file status (no symlink follow) |
| 194 | `getrlimit` | `sys_getrlimit` | Get resource limit |
| 195 | `setrlimit` | `sys_setrlimit` | Set resource limit |
| 196 | `getdirentries` | `sys_getdirentries` | Read directory entries (legacy) |
| 202 | `__sysctl` | `sys_sysctl` | Kernel parameter query/set |
| 253 | `issetugid` | `sys_issetugid` | Check if process is setuid/setgid |
| 294 | `fseek` ¹ | `sys_fseek` | Seek within VFS file descriptor |
| 295 | `fgetc` ¹ | `sys_fgetc` | Read one byte from VFS fd |
| 296 | `fclose` ¹ | `sys_fclose` | Close VFS file descriptor |
| 297 | `fread` ¹ | `sys_fread` | Read from VFS file descriptor |
| 298 | `fopen` ¹ | `sys_fopen` | Open file via UbixOS VFS |
| 299 | `opendir` ¹ | `sys_opendir` | Open directory for listing |
| 300 | `readdir` ¹ | `sys_readdir` | Read next directory entry |
| 301 | `closedir` ¹ | `sys_closedir` | Close directory handle |
| 326 | `__getcwd` | `sys_getcwd` | Get current working directory |
| 331 | `sched_yield` | `sys_sched_yield` | Yield the CPU |
| 340 | `sigprocmask` | `sys_sigprocmask` | Examine/change signal mask |
| 396 | `statfs` | `sys_statfs` | Get filesystem statistics |
| 397 | `fstatfs` | `sys_fstatfs` | Get filesystem statistics via fd |
| 416 | `sigaction` | `sys_sigaction` | Examine/change signal action |
| 475 | `pread` | `sys_pread` | Read at offset without seeking |
| 477 | `mmap` | `sys_mmap` | Map memory region |
| 493 | `fstatat` | `sys_fstatat` | Get file status relative to directory fd |
| 499 | `openat` | `sys_openat` | Open file relative to directory fd |
| 542 | `pipe2` | `sys_pipe2` | Create pipe with flags |

¹ Numbers 294–301 are UbixOS VFS extensions placed out of FreeBSD ABI order. New VFS-specific calls should continue from 302 upward.

### Unimplemented slots

Any slot with `SYSCALL_NOTIMP` logs `"Not Implemented Call: [N][name]"` and returns. Any slot with `SYSCALL_INVALID` (gap in the table) logs `"Invalid Call"`. Neither causes a crash — the process gets a non-zero return value and continues.
