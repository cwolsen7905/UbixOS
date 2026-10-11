# uBixOS Syscall Architecture

uBixOS has **two syscall tables**. Both are plain arrays of `struct syscall_entry`
(`{sc_args, name, handler, status}`) and both are shared by every architecture;
the per-arch entry code only decodes registers and picks a table.

| Table | Array | Defined in | Numbers |
|-------|-------|-----------|---------|
| POSIX | `systemCalls_posix[]` | [sys/posix/syscalls_posix.c](../../sys/posix/syscalls_posix.c) | **FreeBSD syscall numbers** (`exit=1`, `fork=2`, `read=3`, `write=4`, `execve=59`, …). musl is patched to emit these. |
| Native | `systemCalls[]` | [sys/kern/syscalls.c](../../sys/kern/syscalls.c) | uBixOS-private, independent of FreeBSD; new native calls take the next free slot. |

The arch-neutral dispatch engine is `ksyscall_dispatch()` in
[sys/kern/syscall_dispatch.c](../../sys/kern/syscall_dispatch.c). It range-checks the
number, rejects slots whose status is not `SYSCALL_VALID` (logging
`syscall: N (name) not implemented` and returning `-ENOSYS`), copies `sc_args`
register-width words from the arch-gathered `args[]` into a `uap` buffer (at most 8),
calls `handler(td, uap)`, and returns `td->td_retval[0]`. Errors are negative errno.

The rules for adding or changing a syscall are in
[docs/standards/abi-and-syscalls.md](../standards/abi-and-syscalls.md); this document
describes how the system actually works.

> **History:** the frozen i386 port (`releng/2` only) entered the kernel through
> `int $0x80` (POSIX) and `int $0x81` (native) with arguments on the user stack, via
> `sys_call_posix()` / `sys_call()` (`sys/posix/syscall_posix.c`, `sys/kern/syscall.c`).
> Those two files read `tf_eax`/`tf_esp` and are **not** in the aarch64 or x86_64 kernel
> source lists in the top-level `Makefile`. See [Legacy i386 entry](#legacy-i386-entry).

---

## Entry paths

### aarch64 — `svc #0`

| Item | Value |
|------|-------|
| Instruction | `svc #0` from EL0 |
| Number | `x8` |
| Arguments | `x0`–`x5` (the dispatcher reads from the saved register frame) |
| Return | `x0` (negative errno on failure) |
| Vector | `el0_sync` (lower-EL AArch64 sync, `VBAR_EL1 + 0x400`) in [vectors.S](../../sys/arch/aarch64/kern/vectors.S) |

Flow:

1. `el0_sync` saves x0–x30 and calls the C exception handler in
   [exceptions.c](../../sys/arch/aarch64/kern/exceptions.c) with kind `EXC_EL0_SYNC`.
2. The handler reads `ESR_EL1`; if `EC == 0x15` (SVC from AArch64) it calls
   `aarch64_syscall(gpr[8], gpr)` and stores the result into the saved `x0`, then runs
   `signal_check()` before `ERET` to EL0.
3. `aarch64_syscall()` in [syscall.c](../../sys/arch/aarch64/kern/syscall.c) selects the
   table:
   - **Bit `0x8000` set in the number** → native table. The bit is stripped and a few
     numbers are handled inline (`exit_group`, MPI create/destroy/post/fetch, `getcwd`,
     `klog_read`); everything else goes to `systemCalls[]` through `ksyscall_dispatch()`.
   - **Otherwise** → POSIX. A short `switch` intercepts calls that need arch glue
     (`write`, `writev`, `mmap`, `mprotect`, `brk`, `prlimit64` (1024), `fork`, `rfork`
     (251), `execve`, `wait4`, `openat`, `munmap`, `nanosleep`, `setuid`, `setgid`,
     `exit`); every other number falls through to `systemCalls_posix[]`.
4. Native userland thunks are emitted by `UBIX_NATIVE_THUNK` in
   [include/sys/ubix_syscall.h](../../include/sys/ubix_syscall.h), which on aarch64
   loads `x8 = num | 0x8000` and executes `svc #0`.

`mmap`/`brk`/exec helpers that need machine-dependent page tables live in
[syscall_md.c](../../sys/arch/aarch64/kern/syscall_md.c); the MI uregion policy is in
`sys/vmm/`.

### x86_64 — `syscall` (and two legacy gates)

| Item | `syscall` instruction | `int $0x80` / `int $0x81` |
|------|----------------------|---------------------------|
| Used by | musl and everything built by `bmake world` for POSIX calls | `int $0x81`: native thunks from `UBIX_NATIVE_THUNK` (x86_64 takes the non-aarch64 branch of that macro) |
| Number | `rax` | `rax` |
| Arguments | `rdi, rsi, rdx, r10, r8, r9` (`syscall` clobbers `rcx`) | `rdi, rsi, rdx, rcx, r8, r9` for `int $0x81` (an interrupt gate does not clobber `rcx`); `int $0x80` is routed to the same handler as `syscall`, which reads `r10` for the 4th argument |
| Return | `rax` | `rax` |
| Entry | `x86_64_syscall_entry` in [syscall_entry.S](../../sys/arch/x86_64/kern/syscall_entry.S) | IDT vector 128 / 129 in [idt.c](../../sys/arch/x86_64/kern/idt.c) (`x86_64_exception`) |

Flow for `syscall`:

1. `x86_64_syscall_init()` ([usermode.c](../../sys/arch/x86_64/kern/usermode.c)), run
   on every CPU, sets `EFER.SCE`, `STAR` (kernel CS `0x08`, user base `0x10` → user CS
   `0x23` / SS `0x1b`), `LSTAR = x86_64_syscall_entry` and `SFMASK = 0x200` (IF masked
   on entry).
2. `x86_64_syscall_entry` stashes the user `rsp` in the per-CPU block (`%gs:48`),
   loads the task's kernel stack from `%gs:40`, and builds a `struct x86_64_trapframe`
   (the same shape the IDT path uses), then calls `x86_64_syscall(tf)`. There is no
   `swapgs`: the kernel owns `%gs` and musl keeps TLS in `%fs`.
3. `x86_64_syscall()` (usermode.c):
   - `rax & 0x8000` → `x86_64_native_syscall()` (native table, same flag convention as
     aarch64).
   - Inline cases: `exit` (1), `mmap` (477) and `brk` (17) (the 64-bit result goes
     straight into `rax` because `td_retval[]` is 32-bit), `fork` (2), musl `__clone`
     (251 → `rfork` for `CLONE_THREAD`, otherwise a COW `clone`), `arch_prctl` (158,
     `ARCH_SET_FS` only), `prlimit64` (1024), and a serial fast path for `write` to a
     bare console placeholder on fd 1/2.
   - Everything else → `ksyscall_dispatch(&_current->td, systemCalls_posix, …)`.
   - `signal_check()` runs before returning.
4. The return tail pops the trapframe and leaves with `SYSRETQ` (`rcx = rip`,
   `r11 = rflags`, user `rsp` reloaded). A forked child enters at `ret_from_fork` in
   the same file and shares this tail.

`x86_64_native_syscall()` handles `exit_group` (65), the MPI calls (50–53, 69),
`getcwd` (41) and `klog_read` (47) directly and sends everything else to
`systemCalls[]`.

Machine-dependent syscall bodies (mmap, brk, TLS) are in
[syscall_md.c](../../sys/arch/x86_64/kern/syscall_md.c). Entries that still need
x86_64 glue are stubbed in
[syscall_stubs.c](../../sys/arch/x86_64/kern/syscall_stubs.c) and return `-1`
(for example `sys_net_configure`, `sys_vesa_modes`, `sys_sysctl`); `fork`/`mmap`/`brk`
are intercepted before they reach their stub. **TODO: verify** the stub list is
current before relying on it — it changes as subsystems are brought up.

### Differences worth knowing

| | aarch64 | x86_64 |
|-|---------|--------|
| Native selector | `0x8000` flag in the number | `0x8000` flag **or** `int $0x81` |
| 4th argument | `x3` | `r10` (`syscall`/`int $0x80`), `rcx` (`int $0x81`) |
| `mmap`/`brk` return | via the POSIX intercept in `aarch64_syscall()` | intercepted in `x86_64_syscall()` |
| Per-arch hand-mapped cases | the `switch` in `aarch64_syscall()` (bring-up code still being folded into the table) | the `if`/`else` chain in `x86_64_syscall()` |

---

## Native vs POSIX: which calls go where

- **POSIX table** — anything that exists in FreeBSD keeps its FreeBSD number: file
  I/O, processes, sockets, signals, `mmap`, `getrandom` (563), etc. musl's
  `arch/<arch>/bits/syscall.h.in` is patched to FreeBSD numbers, never the reverse.
- **Native table** — uBixOS-specific services with no FreeBSD slot: MPI mailboxes,
  framebuffer/input, pty management, UbixFS/disk queries, and the Linux-compat
  threading primitives (`futex`, `set_thread_area`, `exit_group`) that musl reaches
  with the `0x8000` flag so they do not squat on FreeBSD slots.

`td->abi` is only read by the legacy i386 dispatchers (and there both
branches call the same handler); on aarch64 and x86_64 the entry path and the `0x8000` flag
select the table.

---

## Native syscall table — `systemCalls[]`

Defined in [sys/kern/syscalls.c](../../sys/kern/syscalls.c); `totalCalls` = 72 slots
(0–71). Unlisted slots are `sys_invalid`.

| # | Name | Handler | Description |
|---|------|---------|-------------|
| 7 | `pidStatus` | `sys_pidStatus` | Returns 1 if the pid is a live task, else 0 |
| 41 | `getvfscwd` | `sys_getvfscwd` | Current working directory (`ubix_getcwd`); see [vfs.md](vfs.md) |
| 42 | `ttyctrl` | `sys_ttyctrl` | TTY control (`cmd`, `val`) |
| 43 | `mapfb` | `sys_mapfb` | Map the framebuffer into the caller (the `views` compositor) |
| 44 | `getmouse` | `sys_getmouse` | Poll the mouse event queue |
| 45 | `shareregion` | `sys_shareregion` | Share a memory region into another process |
| 46 | `getkbd` | `sys_getkbd` | Read keyboard input |
| 47 | `klog_read` | `sys_klog_read` | Read the kernel log ring |
| 48 | `settty` | `sys_settty` | Claim the serial TTY as the caller's controlling terminal |
| 49 | `klog_write` | `sys_klog_write` | Write to the kernel log ring |
| 50 | `mpiCreateMbox` | `sys_mpiCreateMbox` | Create an MPI mailbox |
| 51 | `mpiDestroyMbox` | `sys_mpiDestroyMbox` | Destroy an MPI mailbox |
| 52 | `mpiPostMessage` | `sys_mpiPostMessage` | Post to an MPI mailbox |
| 53 | `mpiFetchMessage` | `sys_mpiFetchMessage` | Fetch from an MPI mailbox |
| 54 | `fbpresent` | `sys_fbpresent` | Present the mapped framebuffer (arch-agnostic per-frame call; flushes the virtio-gpu scanout on aarch64) |
| 55–58 | `ptyalloc/ptyfree/ptyinject/ptysnap` | `sys_pty*` | Pseudo-terminal management |
| 59 | `net_configure` | `sys_net_configure` | DHCP / static network configuration |
| 60 | `vesa_modes` | `sys_vesa_modes` | List display modes |
| 61 | `ptyresize` | `sys_ptyresize` | Resize a pty (SIGWINCH) |
| 62 | `sysinfo` | `sys_sysinfo` | Uptime / free-page statistics |
| 63 | `set_thread_area` | `sys_set_thread_area` | musl TLS |
| 64 | `futex` | `sys_futex` | musl threading |
| 65 | `exit_group` | `sys_exit_group` | musl `exit()` |
| 66 | `thread_exit_unmap` | `sys_thread_exit_unmap` | Unmap own stack and exit (`__unmapself`) |
| 67 | `ubfs_query` | `sys_ubfs_query` | UbixFS pool/dataset query |
| 68 | `disk_query` | `sys_disk_query` | Block-device / partition enumeration |
| 69 | `mpiWaitMessage` | `sys_mpiWaitMessage` | Blocking MPI receive with timeout |
| 70 | `set_hostname` | `sys_set_hostname` | Set the machine hostname |
| 71 | `net_status` | `sys_net_status` | Default interface up/link/IPv4 |

Slot 40 (`sysSDE`) is retired (see the comments in `syscalls.c`). Descriptions are
summaries; handler semantics are in the handler sources.

---

## POSIX syscall table — `systemCalls_posix[]`

Defined in [sys/posix/syscalls_posix.c](../../sys/posix/syscalls_posix.c); `totalCalls_posix`
covers FreeBSD numbers up to 584. The implemented (`SYSCALL_VALID`, non-`sys_invalid`)
slots are listed below by slot number and handler. All other slots are
`SYSCALL_INVALID` or `SYSCALL_NOTIMP` and fail with `-ENOSYS`.

| # | Handler | # | Handler | # | Handler |
|---|---------|---|---------|---|---------|
| 1 | `sys_exit` | 80 | `chown_noop` | 207 | `sys_getpgid` |
| 2 | `sys_fork` | 81 | `sys_getpgrp` | 224 | `sys_gettid` |
| 3 | `sys_read` | 82 | `sys_setpgid` | 232 | `sys_clock_gettime` |
| 4 | `sys_write` | 83 | `sys_setitimer` | 238 | `sys_tkill` |
| 5 | `sys_open` | 86 | `sys_getitimer` | 240 | `sys_nanosleep` |
| 6 | `sys_close` | 88 | `sysGetFreePage` | 251 | `sys_rfork` |
| 7 | `sys_wait4` | 90 | `sys_dup2` | 253 | `sys_issetugid` |
| 10 | `sys_unlink` | 92 | `sys_fcntl` | 254 | `chown_noop` |
| 12 | `sys_chdir` | 93 | `sys_select` | 258 | `sys_set_tid_address` |
| 13 | `sys_fchdir` | 95 | `fsync` | 272 | `sys_getdirentries` |
| 15 | `chmod` | 97 | `sys_socket` | 276 | `lutimes` |
| 16 | `chown_noop` | 98 | `sys_connect` | 294 | `sys_fseek` |
| 17 | `obreak` | 102 | `sys_orecv` | 295 | `sys_fgetc` |
| 20 | `sys_getpid` | 104 | `sys_bind` | 296 | `sys_fclose` |
| 21 | `sys_mount` | 105 | `sys_setsockopt` | 297 | `sys_fread` |
| 22 | `sys_umount` | 106 | `sys_listen` | 298 | `sys_fopen` |
| 23 | `sys_setUID` | 116 | `sys_gettimeofday` | 299 | `sys_opendir` |
| 24 | `sys_getUID` | 117 | `sys_getrusage` | 300 | `sys_readdir` |
| 25 | `sys_geteuid` | 118 | `sys_getsockopt` | 301 | `sys_closedir` |
| 27 | `sys_recvmsg` | 120 | `sys_readv` | 326 | `sys_getcwd` |
| 28 | `sys_sendmsg` | 121 | `sys_writev` | 331 | `sys_sched_yield` |
| 29 | `sys_recvfrom` | 123 | `chown_noop` | 340 | `sys_sigprocmask` |
| 30 | `sys_accept` | 124 | `fchmod` | 383 | `sys_statx` |
| 31 | `sys_getpeername` | 128 | `sys_rename` | 396 | `sys_statfs` |
| 32 | `sys_getsockname` | 133 | `sys_sendto` | 397 | `sys_fstatfs` |
| 33 | `sys_access` | 136 | `sys_mkdir` | 416 | `sys_sigaction` |
| 37 | `sys_kill` | 137 | `sys_rmdir` | 417 | `sys_sigreturn` |
| 39 | `sys_getpid` | 138 | `utimes` | 475 | `sys_pread` |
| 41 | `sys_dup` | 147 | `sys_setsid` | 477 | `sys_mmap2` |
| 42 | `pipe` | 164 | `sys_uname` | 478 | `sys_lseek` |
| 43 | `sys_getegid` | 168 | `sys_poll` | 480 | `sys_ftruncate` |
| 47 | `sys_getGID` | 179 | `sys_sigsuspend` | 489 | `sys_faccessat` |
| 49 | `sys_getlogin` | 181 | `sys_setGID` | 491 | `chown_noop` |
| 54 | `sys_ioctl` | 182 | `chown_noop` | 493 | `sys_fstatat` |
| 55 | `sys_reboot` | 183 | `chown_noop` | 499 | `sys_openat` |
| 57 | `symlink` | 188 | `sys_stat` | 504 | `sys_posix_openpt` |
| 58 | `sys_readlink` | 189 | `sys_fstat` | 541 | `sys_accept4` |
| 59 | `sys_execve` | 190 | `sys_lstat` | 542 | `sys_pipe2` |
| 60 | `sys_umask` | 194 | `sys_getrlimit` | 547 | `sys_utimensat` |
| 65 | `sys_msync` | 195 | `sys_setrlimit` | 563 | `sys_getrandom` |
| 73 | `sys_munmap` | 196 | `sys_getdirentries` | 584 | `sys_membarrier` |
| 74 | `mprotect` | 202 | `sys_sysctl` | | |
| 75 | `sys_madvise` | 206 | `futimes` | | |

Notes:

- `chown_noop` slots (`chown`, `setgroups`, `fchown`, `setegid`, `seteuid`, `lchown`,
  `fchownat`) succeed without enforcing ownership.
- Slot 39 (`getppid`) is currently wired to `sys_getpid` in the table;
  slot 47 (`getgid`) to `sys_getGID`. **TODO: verify** slot 39 is intentional.
- Slot 477 is registered as `sys_mmap2` (offset in page units); both 64-bit entry
  paths intercept 477 before the table, so that table entry is not reached on
  aarch64 or x86_64.
- Slots 294–301 (`fseek`, `fgetc`, `fclose`, `fread`, `fopen`, `opendir`, `readdir`,
  `closedir`) are uBixOS VFS extensions placed outside FreeBSD order, marked
  `XXX - Wrong Spot` in the source. New uBixOS-private calls belong in the native
  table instead.
- POSIX slot 41 is `dup` (the FreeBSD number). The old POSIX-table `getvfscwd`
  extension is gone; `getvfscwd` is native slot 41 (see above).
- Name strings in the table contain a few typos (`wiat4`, `cgdur`, `feek`); the
  handler column is authoritative.

---

## Adding a syscall

The authoritative procedure and rules (ABI, user-pointer validation, errno) are in
[docs/standards/abi-and-syscalls.md](../standards/abi-and-syscalls.md). Mechanically:

1. Add the `args` struct (`sys/include/sys/sysproto_posix.h` for POSIX,
   `sys/include/sys/sysproto.h` for native) and the handler prototype.
2. Implement the handler in the owning subsystem (`sys/posix/`, `sys/fs/`, `sys/kern/`, …).
3. Register it in the table: replace the `SYSCALL_NOTIMP`/`sys_invalid` slot at the FreeBSD
   number (POSIX) or append the next free slot (native).
4. Add a userland wrapper: POSIX calls come from musl; native calls use
   `UBIX_NATIVE_THUNK` in `lib/ubix_api/`.
5. Add the aarch64 and x86_64 entry handling only if the call needs a 64-bit return
   value or the trapframe (like `mmap`, `fork`); otherwise the shared table is enough.
6. Run `tools/check/run.sh` (the `syscall-table` gate) and boot both arches.

---

## Legacy i386 entry

Retained for readers of `releng/2` only. On i386 there was no register-based
convention:

- `int $0x80` → `sys/arch/i386/sys_call_posix.S` → `sys_call_posix()` → `systemCalls_posix[]`
- `int $0x81` → `sys/arch/i386/sys_call.S` → `sys_call()` → `systemCalls[]`
- Both vectors were registered in the i386 `idt.c`; the number was in `%eax` and
  arguments were read from the user stack at `[esp+4]`, `[esp+8]`, …; the handler's
  `td_retval[0]` was copied back to `%eax`.
- POSIX numbers followed the FreeBSD i386 ABI; the old `lib/libc_old/` wrappers used
  inline-asm `int $0x80`.
