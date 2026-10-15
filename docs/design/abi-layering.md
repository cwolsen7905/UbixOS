# ABI Layering Audit

> Audit A3, 2026-10-14, against master `e0d7182fa`. Docs only: nothing here
> changes code. It describes how uBixOS's three ABI layers fit together
> (§1), lists every musl `__NR_*` against the kernel's slots on both arches
> (§2, full table in the appendix), traces the syscall-242 log flood to its
> caller (§3), and plans **W-009** (errno translation) and **W-029**
> (Linux-numbered slots) as a sequence of small PRs that each leave both
> arches green (§4). Lane A5 implements §4.
>
> **Design rule.** The native core comes first. The POSIX layer follows
> **FreeBSD** semantics (slot numbers, argument shapes, struct layouts, errno
> values), never Linux 1:1. Where FreeBSD has no equivalent, a call that musl
> needs goes into the native table (A-1) or is failed in libc. It does not
> squat a FreeBSD slot. Rules: `docs/standards/abi-and-syscalls.md` (A-1..A-12).

## 1. The layer model

```
  applications, ports (tcsh, bmake, clang, NetSurf, ...)      uBix apps (views, ubistry, Settings, ...)
          | POSIX API                                                 | uBix API
          v                                                           v
  +-----------------------------------------------+    +------------------------------------------+
  | L2  musl libc  (contrib/musl)                 |    | lib/ubix_api                             |
  |     __NR_* = FreeBSD slot numbers             |    |   UBIX_NATIVE_THUNK(n) -> native slot n  |
  |     | 0x8000  -> native table                 |    |   MPI mailboxes, ubistry client,         |
  |     | 0x40000 -> -ENOSYS in libc, no trap     |    |   display, pty, disk/pool queries        |
  +-----------------------------------------------+    +------------------------------------------+
          | syscall / svc #0                                          | int $0x81 / nr|0x8000
          v                                                           v
  +--------------------------------------------------------------------------------------------+
  | arch entry: decode registers only (A-4)                                                    |
  |   aarch64: aarch64_syscall()  sys/arch/aarch64/kern/syscall.c                              |
  |   x86_64:  x86_64_syscall()   sys/arch/x86_64/kern/usermode.c                              |
  |   + MD pre-dispatch cases (bring-up; being retired, no new ones)                           |
  +--------------------------------------------------------------------------------------------+
          |                                                           |
          v                                                           v
  +-----------------------------------------------+    +------------------------------------------+
  | L1  POSIX face: systemCalls_posix[]           |    | L0  native core: systemCalls[]           |
  |     sys/posix/syscalls_posix.c, 585 slots     |    |     sys/kern/syscalls.c, 72 slots        |
  |     FreeBSD numbers, FreeBSD semantics        |    |     append-only (A-3)                    |
  +-----------------------------------------------+    +------------------------------------------+
          \________________ ksyscall_dispatch() (sys/kern/syscall_dispatch.c) _______/
                                         |
                    MI subsystems: VFS, sched, vmm, MPI (sys/mpi), net (lwIP)
                                         |
                    native services over MPI: ubistry, views, authd, logd
```

### L0: the native uBix core

- **Native syscall table**: `systemCalls[]` in `sys/kern/syscalls.c`,
  currently slots 0..71 (`totalCalls`). It holds the uBix-isms: MPI mailbox
  create/destroy/post/fetch/wait (50..53, 69), klog (47, 49), framebuffer and
  display (43, 45, 54), the pty pool (55..58, 61), `net_configure`/`net_status`
  (59, 71), `sysinfo` (62), the UbixFS pool and disk queries (67, 68),
  `set_hostname` (70) and `getvfscwd` (41). It also holds the Linux-isms that
  musl needs and FreeBSD lacks: `futex` 64 and `exit_group` 65, both emitted
  by the 64-bit libc. `set_thread_area` 63 and `thread_exit_unmap` 66 come
  from the i386 era. The 64-bit musl does not call them: TLS is
  `msr tpidr_el0` on aarch64 and `arch_prctl` on x86_64, and `__unmapself`
  uses 73 + 1.
- **Reached by** `int $0x81`, or by OR-ing `NATIVE_FLAG` (`0x8000`) into the
  number on the `syscall`/`svc` path. Both arch entries strip the flag. Some
  native calls (MPI, `getcwd`, `klog_read`, `exit_group`) still go through MD
  `case`s before the table (`x86_64_native_syscall`, and the `NATIVE_FLAG`
  branch of `aarch64_syscall`).
- **MPI** (`sys/mpi/`) is the native IPC. **ubistry** is a native *service*
  (the `bin/ubistry` daemon on MPI, with client `lib/ubix_api/ubistry.c`) and
  not a syscall. Its design is `docs/design/ubistry-plan.md`.
- **`lib/ubix_api`** is the main userland client of L0. musl's only native
  calls are `futex` and `exit_group`. Its thunks are `UBIX_NATIVE_THUNK(name, n)`
  (`sys/ubix_syscall.h`) plus a few hand-written `.S` files for MPI.

### L1: the POSIX face (FreeBSD-numbered)

- `systemCalls_posix[]` in `sys/posix/syscalls_posix.c`, 585 slots (0..584).
  Slot numbers are FreeBSD's (`sys/kern/syscalls.master`). The table is the
  contract (A-2).
- **Dispatch.** `ksyscall_dispatch()` copies `sc_args` register-width words
  into a uap and calls the handler. It then returns **`td->td_retval[0]`**,
  and the handler's own `int` return value is ignored. An out-of-range number,
  or a slot that is not `SYSCALL_VALID`, logs
  `syscall: N (name) not implemented` and returns `-ENOSYS`. That log line is
  the one behind §3.
- **Pre-dispatch (MD) intercepts.** These run before the table and are still
  arch-specific:

  | nr | call | aarch64 | x86_64 | Note |
  |----|------|:-------:|:------:|------|
  | 1 | exit | ✓ | ✓ | terminates the task |
  | 2 | fork | ✓ | ✓ | needs the trapframe |
  | 4 | write | ✓ | console fd only | bring-up serial fast path |
  | 7 | wait4 | ✓ | | |
  | 17 | break (brk) | ✓ | ✓ | returns a 64-bit VA, with **Linux `brk` semantics** |
  | 23, 181 | setuid, setgid | ✓ | | |
  | 59 | execve | ✓ | | |
  | 73, 74 | munmap, mprotect | ✓ | | |
  | 121 | writev | ✓ | | |
  | 158 | **arch_prctl** (Linux x86_64 number) | | ✓ | FreeBSD 158 is compat4 `fstatfs`; see §2.4 |
  | 240 | nanosleep | ✓ | | |
  | 251 | rfork (musl `__clone`) | ✓ | ✓ | |
  | 477 | mmap | ✓ | ✓ | returns a 64-bit VA |
  | 499 | openat | ✓ | | |
  | 1024 | **prlimit64** (uBix-private) | ✓ | ✓ | beyond both tables; see §2.4 |

- **Errors** are negative errno in `td_retval[0]` (A-10). The kernel
  currently has **three errno namespaces**. This is W-009 (§4.2):

  | Header | Numbering | Used by |
  |--------|-----------|---------|
  | `sys/include/sys/errno.h` | **FreeBSD** (EAGAIN 35, ENOSYS 78, ELOOP 62, ...) | `vfs_calls.c`, `signal.c`, `tty.c`, `descrip.c`, `kern_exec.c`, `access.c`, `file.c`, `ubfs_vfs.c`, `vmm_*`, both arch syscall files, ... |
  | `sys/include/ubixos/errno.h` | **Linux** (EAGAIN 11, ENOSYS 38, ELOOP 40, ...) | `syscall_dispatch.c`, `syscall.c`, `namei.c`, `stat.c`, `gen_calls.c`, `sem.c`, `ubthread.c`, `kern_sysctl.c`, aarch64 `syscall_md.c`/`execfile.c`, ... |
  | `sys/include/net/errno.h` | **Linux** (lwIP `LWIP_PROVIDE_ERRNO 1`) | every socket call |

  musl (`contrib/musl/arch/generic/bits/errno.h`) uses **Linux** numbering.
  Values 1..34 are the same in both numberings. Every value from 35 up
  differs, so a FreeBSD-numbered handler's EAGAIN (35) reads in userland as
  EDEADLK, ENOSYS (78) as EREMCHG, ELOOP (62) as ETIME and ENAMETOOLONG (63)
  as ENOSR. Sockets and namei happen to agree with musl. Pipes, ttys and VFS
  calls do not.

### L2: musl, the client

`contrib/musl/arch/{aarch64,x86_64}/bits/syscall.h.in` started as **Linux
i386's** table (not Linux aarch64's or x86_64's) with uBix overrides
appended. The overrides use three mechanisms:

1. **Remap** a name to its FreeBSD slot (`pwrite64` 476, `openat` 499, the
   socket calls, ...).
2. **`__UBIX_ENOSYS`** (`0x40000 | linux_nr`): `syscall_arch.h` and
   `__syscall_cp_c` return `-38` without trapping, and musl takes its fallback
   path (6147b5175, 2026-10-11).
3. **`NATIVE_FLAG`** (`0x8000 | native_nr`) for the four threading primitives.

The two headers evaluate to **identical** numbers for all 455 names. Their
text differs only in where the rlimit override block sits and how it is
worded. The arches differ only in hand-written assembly, which bypasses the
header (§2.4).

### Direction of dependencies

- L2 depends on L1 and L0. L1 handlers may use L0 services (MPI, the native
  scheduler waits). L0 never takes on L1 semantics: a native call has its own
  argument structs and does not borrow a FreeBSD slot.
- A call that FreeBSD defines goes into L1 at FreeBSD's number, with
  FreeBSD's argument shape. If musl's Linux-shaped call differs, the *musl
  wrapper* adapts (A-2), never the kernel.
- A call that FreeBSD lacks and musl needs (thread identity glue, futex) goes
  into L0 and is reached with `0x8000`. A call that nobody needs is failed in
  libc with `0x40000`.

## 2. musl `__NR_*` against the kernel slots

### 2.1 Method (reproducible)

1. Evaluate every `#define`/`#undef __NR_*` in both `syscall.h.in` files, in
   order (last definition wins, expressions resolved).
2. Compare each name's number with upstream musl's Linux i386 number
   (`arch/i386/bits/syscall.h.in`, the template these files came from) and
   with FreeBSD's `syscalls.master` (freebsd-src `main`).
3. Parse `systemCalls_posix[]` for the kernel slot name and status at that
   number.
4. Disassemble the **built** `libc.a` for each arch (`objdump -dr`). Collect
   every immediate loaded into `rax`/`x8` before `syscall`/`svc`, and into the
   first argument before a call to `__syscall_cp`/`syscall`. This yields the
   numbers musl *actually emits*, after `src/internal/syscall.h` has aliased
   names away (e.g. `geteuid` → `geteuid32` = 25). The "emitted" column comes
   from this.

### 2.2 Classes

| Class | Meaning | Count |
|-------|---------|------:|
| **FreeBSD** | The number is FreeBSD's slot for this call, either by coincidence with Linux (`read` 3) or by a uBix remap (`openat` 499). | 148 |
| **native** | `0x8000` → native table (`futex`, `exit_group`, `set_thread_area`). | 3 |
| **stub** | `0x40000` → `-ENOSYS` in libc, no trap. | 71 |
| **Linux:squat** | Still at its Linux number, and the **kernel serves the Linux call in that FreeBSD slot**. These are W-029. | 6 |
| **Linux:live** | Still at its Linux number, which lands on an *implemented, unrelated* FreeBSD call. | 31 |
| **Linux:latent** | Still at its Linux number, which lands on an unrelated FreeBSD slot that is NOTIMP/INVALID **today**, so the call gets `-ENOSYS` plus a log line. | 193 |
| **remap-wrong** | Remapped to a FreeBSD slot, but to a different call (`chown` → 254 `lchown`). | 2 |
| **private** | uBix-private number outside both tables (`prlimit64` 1024). | 1 |
| **Total** | | **455** |

So **230 names (51%) are still Linux-numbered**. libc emits 84 of them:
the 6 squats plus **78 latent collisions**. None of the 31 live collisions is
emitted by libc itself. They are names that `src/internal/syscall.h` aliases
away, or calls musl never makes, but `<sys/syscall.h>` still exports them as
`SYS_*`. A port that calls `syscall(SYS_stat, ...)` runs `listen`, and
`syscall(SYS_mmap, ...)` runs `dup2`.

### 2.3 Findings

**F1: the six squats (W-029).** These are live, emitted by libc on both
arches, and served by the kernel at a slot that FreeBSD either reserves or
uses for something else:

| musl call | nr | FreeBSD meaning of nr | Emitted by | FreeBSD equivalent | Target |
|-----------|---:|-----------------------|------------|--------------------|--------|
| `poll` | 168 | reserved | `poll`, `__init_libc` | `poll` 209 | L1 209 |
| `rt_sigsuspend` | 179 | reserved | `sigsuspend`, `exit` | `sigsuspend` 341 | L1 341 (wrapper passes a FreeBSD 16-byte `sigset_t`) |
| `gettid` | 224 | compat7 `msgctl` | `__synccall` (and ports: LLVM `syscall(SYS_gettid)`) | `thr_self` 432 (`long *id`) | L1 432 (musl wrapper returns the id) |
| `tkill` | 238 | **`ktimer_gettime`** | `raise`, `abort`, `pthread_kill`, `__synccall`, `__membarrier`, ... | `thr_kill` 433 | L1 433 |
| `set_tid_address` | 258 | reserved | `__init_tp`, `_Fork`, thread start | none | **L0** (next native slot, 72), like `futex` |
| `statx` | 383 | obsolete `kse_release` | `__fstatat`, `statx` | `fstatat` 552 (ino64 `struct stat`) | L1 552; `statx` becomes a libc stub |

**F2: 78 emitted latent collisions.** Today each returns `-ENOSYS` and
logs. The danger is the future: **implementing the FreeBSD call in that slot
silently activates the wrong musl call.** If 341 `sigsuspend` is wired up
(F1), `name_to_handle_at` starts calling `sigsuspend`. If 311/312
`setresuid`/`setresgid` are wired up, every `pthread_exit` (`set_robust_list`
311) calls `setresuid`. If 338 `jail` is wired up, `fanotify_init` calls it.
So musl hygiene must come **before** kernel slots are filled in (§4.1, step
L1). The emitted set, grouped by what to do:

| Group | musl calls (Linux nr) | FreeBSD equivalent |
|-------|-----------------------|--------------------|
| *at() family, which FreeBSD has | `renameat` 302, `linkat` 303, `symlinkat` 304, `readlinkat` 305, `fchmodat` 306, `renameat2` 353, `fchmodat2` 452 | 501, 495, 502, 500, 490 (`renameat2`/`fchmodat2`: stub) |
| scheduling | `sched_setparam` 154, `sched_getparam` 155, `sched_setscheduler` 156 (from `pthread_create` with sched attrs), `sched_getscheduler` 157, `sched_get_priority_max`/`min` 159/160, `sched_rr_get_interval` 161, `getcpu` 318 | 327, 328, 329, 330, 332, 333, 334, `sched_getcpu` 581 |
| signals | `rt_sigpending` 176, `rt_sigtimedwait` 177, `rt_sigqueueinfo` 178 | `sigpending` 343, `sigtimedwait` 345, `sigqueue` 456 (shapes differ: wrapper or stub) |
| timers / mqueue | `timer_create` 259, `timer_getoverrun` 262, `timer_delete` 263, `mq_*` 277..282 | `ktimer_*` 235..239, `kmq_*` 457..462 |
| I/O | `pselect6` 308, `ppoll` 309, `dup3` 330, `sendfile64` 239, `copy_file_range` 377, `fallocate` 324, `utimes` 271 (`utimensat` fallback), `waitid` 284, `execveat` 358 (`fexecve`) | `pselect` 522, `ppoll` 545, `fcntl(F_DUP3FD)`, `sendfile` 393, 569, `posix_fallocate` 530, `utimes` 138, `wait6` 532, `fexecve` 492 |
| robust futex list | `set_robust_list` 311, `get_robust_list` 312 | none: stub (or L0 if robust mutexes are ever needed) |
| Linux-only | `umount2` 52, `ioperm` 101, `syslog`/`klogctl` 103, `iopl` 110, `vhangup` 111, `swapoff` 115, `quotactl` 131, `prctl` 172, `capget`/`capset` 184/185, `setfsuid32`/`setfsgid32` 215/216, `pivot_root` 217, `readahead` 225, `remap_file_pages` 257, `inotify_*` 291..293/332, `unshare` 310, `splice`/`sync_file_range`/`tee`/`vmsplice` 313..316, `signalfd`/`signalfd4` 321/327, `eventfd`/`eventfd2` 323/328, `recvmmsg` 337, `fanotify_*` 338/339, `*_handle_at` 341/342, `syncfs` 344, `setns` 346, `process_vm_*` 347/348, `memfd_create` 356, `mlock2` 376, `arch_prctl()` 384 | stub (`0x40000`); `unmount` 22 for `umount2`; `shm_open2` 571 for `memfd_create` if wanted |

**F3: `chown` → 254.** `chown`/`chown32` are remapped to 254, which is
FreeBSD `lchown`. FreeBSD `chown` is **16**. Both kernel slots run
`chown_noop` today, so nothing breaks yet. Once ownership is real, `chown()`
would stop following symlinks.

**F4: `brk` → 17.** FreeBSD 17 is `break` (`obreak`), which returns 0/-1
and does not exist on FreeBSD amd64/arm64 userland. uBix intercepts 17 before
the table on both arches and gives it Linux `brk` semantics (it returns the
new break). This is FreeBSD-numbered but Linux-shaped. It is acceptable while
mallocng uses `brk` for its metadata area. The FreeBSD way is `mmap` only,
and mallocng already falls back to `mmap` when `brk` fails. Recorded here, but no step is planned for it.

**F5: compat-numbered stat family.** `stat64`/`fstat64`/`lstat64` → 188/189/190,
`getdents64` → 272 and `statfs64`/`fstatfs64` → 396/397 are FreeBSD numbers,
but FreeBSD's *compat11* ones (32-bit `ino_t`, old `struct stat`/`dirent`). The
current FreeBSD ABI is `fstat` 551, `fstatat` 552, `getdirentries` 554,
`statfs` 555 and `fstatfs` 556. Moving `statx` to 552 (F1) is the moment to
pin the `struct stat` layout. Moving the rest is outside W-029.

**F6: `fdatasync` 550** is FreeBSD-numbered and emitted, but the kernel slot
is NOTIMP. It is not a numbering problem. It is listed here because it shows
up in the same `not implemented` log.

**F7: stub-encoding slip.** `timer_settime32`/`timer_gettime32` are stubbed
as `0x40000|1`/`0x40000|2`, copied from the `__NR_timer_create+1/+2`
expressions instead of being written as `|260`/`|261`. This is harmless (both
still return ENOSYS without trapping) but misleading.

### 2.4 Per-arch differences (hand-written assembly)

| Site | aarch64 | x86_64 | Status |
|------|---------|--------|--------|
| TLS setup | `msr tpidr_el0` (no syscall) | `__set_thread_area.s`: `syscall` **158** (`arch_prctl(ARCH_SET_FS)`, Linux x86_64 number), served by an MD intercept | **Linux-numbered slot not in W-029.** FreeBSD equivalent: `sysarch` 165 with `AMD64_SET_FSBASE` (129). Slot 165 is `sys_sysarch`, `SYSCALL_DEBUG`. |
| `arch_prctl()` C API | 384 | 384 | latent: FreeBSD `__mac_get_proc`, NOTIMP |
| signal restorer | `restore.s`: `mov x8,#139` (Linux aarch64 `rt_sigreturn`) | `restore.s`: `mov $15` (Linux x86_64 `rt_sigreturn`; **FreeBSD 15 = `chmod`**) | dormant: the kernel ignores `sa_restorer` and returns through the magic-LR fault (`sys/posix/signal.c`). Must stay unreachable, or become `sigreturn` 417 |
| thread clone | `clone.s`: 251 (`rfork`) | `clone.s`: 251 | FreeBSD slot, uBix semantics (CLONE_* flags in arg0) |
| thread exit | `clone.s`/`__unmapself.s`: 1, 73 | same | FreeBSD |
| `vfork` | tail-calls `fork` | tail-calls `fork` | FreeBSD |
| `prlimit64` | 1024, MD intercept | 1024, MD intercept | **uBix-private.** The header comment says 1024 is out of range so the kernel returns ENOSYS and musl falls back to 194/195. In fact both arches *serve* it in pre-dispatch. Choose one: drop the intercepts (musl's fallback to FreeBSD `getrlimit`/`setrlimit` already works), or stub it in libc. |

Apart from these rows, the kernel route is the same on both arches: a name
hits the POSIX table, an MD intercept, the native table or the libc stub, as
the appendix's `aarch64`/`x86_64` columns show.

## 3. Syscall 242 (`ffclock_setestimate`) in the log

**Symptom.** The kernel log fills with
`syscall: 242 (ffclock_setestimate) not implemented`, printed by
`ksyscall_dispatch()` (`sys/kern/syscall_dispatch.c:50`) once per trap, with
no rate limit.

**What 242 is.** 242 is Linux **i386** `sched_getaffinity`, the template
the musl headers came from. FreeBSD 242 is `ffclock_setestimate`, a NOTIMP
slot in the uBix table. It is the *name* in the log that misleads: nothing
calls ffclock.

**Current libc does not trap 242.** Since 6147b5175 (2026-10-11),
`__NR_sched_getaffinity` is `0x40000|242`, so the inline `__syscallN` returns
-ENOSYS without a `syscall`/`svc`. In the libc.a disassembly (§2.1), 242 is
not among the numbers emitted on either arch. The trap therefore comes from a
binary **statically linked against an older libc.a**.

**The caller: the in-OS LLVM toolchain.** `tools/ports/llvm` builds
`clang`, `ld.lld`, `llvm-ar`, `llvm-objcopy` and `llvm-nm` with `-static`
(`ubixos.cmake`: `CMAKE_EXE_LINKER_FLAGS_INIT "-static ..."`), and stages
them into `build/${ARCH}/usr/bin`, which `bmake image` copies to the device.
The staged binaries are dated **2026-10-10 18:25** on both arches, the day
before the musl fix. A byte scan finds the trap sites:

| Binary | `nr=242` → `syscall`/`svc` sites |
|--------|---------------------------------:|
| x86_64 `clang`, `ld.lld` | 2 each (`mov $0xf2,%eax … syscall`) |
| aarch64 `clang`, `ld.lld` | 3 each (`mov x8,#242 … svc #0`) |

These are musl's `sched_getaffinity()` (`src/sched/affinity.c`) and
`sysconf(_SC_NPROCESSORS_ONLN)` (`src/conf/sysconf.c:206`, an inline
`__syscall(SYS_sched_getaffinity, ...)`).

**The call chain.** LLVM is cross-built for `*-unknown-linux-musl`
(`ubixos-clang.cmake` sets the compiler target; `build.sh` sets
`LLVM_HOST_TRIPLE`), so `__linux__` is defined and LLVM takes its Linux
paths:

```
ld.lld main
  lld/ELF/Driver.cpp:1590/1596   parallel::strategy.compute_thread_count()
  llvm/lib/Support/Parallel.cpp:53  ThreadPoolExecutor(...) -> compute_thread_count()
    llvm/lib/Support/Threading.cpp:57  computeHostNumHardwareThreads()
      llvm/lib/Support/Unix/Threading.inc:307  sched_getaffinity(0, ...)   -> trap 242 -> -ENOSYS
      fallback: std::thread::hardware_concurrency()
        libc++ src/thread.cpp:66  sysconf(_SC_NPROCESSORS_ONLN)
          musl sysconf.c:206  __syscall(SYS_sched_getaffinity, ...)       -> trap 242 again
```

On Linux/x86, `get_physical_cores()` (`Threading.inc`, the
`__linux__ && __x86_64__` branch) also starts with `sched_getaffinity` when a
heavyweight thread pool is asked for. Each `compute_thread_count()` costs two
traps, and `ld.lld` reaches it at least three times per link, so each link
logs about six lines. An in-OS world build (W-007) links hundreds of
programs, which produces the flood. A plain `clang -c` does not build a
thread pool on this path. The per-invocation count is from reading the
source, not from a measurement.

**Side effect.** After the failed call, `sysconf` counts its initial
`set[0] = 1` and returns **1**, so LLVM believes the machine has one CPU and
`ld.lld` runs single-threaded, even though both arches have SMP.

**Why the flood shows on x86_64.** The code path is **not** x86_64-specific.
aarch64 uses the same dispatcher and logs the same line, and its binaries
have the same vintage and even more trap sites. The flood is most likely
seen on x86_64 because that is where in-OS builds were being watched
(x86_64's serial log under TCG). This audit could not confirm that by
running anything: no saved log contains the line, and A3 starts no QEMU.
Step L4 measures it on both arches.

**Fix** (§4.1, steps L4 and L7):

1. Rebuild the LLVM port against current musl. `sched_getaffinity` is then
   a libc stub and nothing traps. This is the immediate fix.
2. **Do not** make slot 242 answer. It is FreeBSD's `ffclock_setestimate`
   (A-1).
3. Implement FreeBSD `cpuset_getaffinity` (487) and make musl's
   `sched_getaffinity` a wrapper over it, as FreeBSD's own libc does. That
   makes `sysconf(_SC_NPROCESSORS_ONLN)` and LLVM report the real CPU count.
4. Rate-limit the `not implemented` log (once per pid and number), so that a
   stale binary cannot flood the log again.

## 4. Plan: W-009 and W-029

Each step is **one PR** that builds and boots **aarch64 and x86_64**
(`bmake kernel world image` on both, plus `run-debug-*` to a login prompt)
and passes `tools/check/run.sh --tier ci`. A step that changes a number or
an errno value visible to a shipped binary gets a `CHANGELOG.md` *Changed*
entry naming the break (see "Stability" in `abi-and-syscalls.md`).

Order across both items:
**L0 → L1 → E0 → E1 → L2 → L3 → E2 → L4 (+E3) → L5 → L6**, then L7 and E4 at
any point after L3. The ordering exists for two reasons. musl hygiene (L1)
must land before any FreeBSD slot is filled in (F2). Both userland flips (L3,
E2) should land before the single port rebuild (L4), so that the static
toolchain is rebuilt once.

### 4.1 W-029: Linux-numbered slots

| Step | PR content | Check | ABI |
|------|-----------|-------|-----|
| **L0** gate | `tools/check` gate `abi-numbers`. It evaluates both `syscall.h.in` files as §2.1 does and fails when a name is newly left at its Linux i386 number on a FreeBSD slot whose FreeBSD name differs. The baseline is today's 230 Linux-numbered names (§2.2), which then only ratchet down. The two headers must also evaluate identically. | `selftest.sh` proves that the gate catches a reintroduced `pwrite64 181` | none |
| **L1** musl hygiene | Clear every **emitted latent collision** (F2) and the 31 dead live collisions. Where FreeBSD has the call *with the same argument shape*, remap it (the *at() family to 501/495/502/500/490, `sched_*` to 327..334, `umount2` to 22, `chown`/`chown32` to 16, F3). Otherwise stub it with `0x40000`. Fix the F7 encodings. Stub `arch_prctl` 384. Split the PR into L1a (remaps) and L1b (stubs) if review needs it. | Rerun the §2.1 scan: no emitted `Linux:latent`/`Linux:live` numbers. vfstest still passes. Boot to the desktop on both arches | none in practice: every affected call already returned -ENOSYS |
| **L2** kernel, new slots | Serve the six at their targets **while keeping the old slots**: 209 `poll` (same args as 168); 341 `sigsuspend` (reads FreeBSD's 16-byte `sigset_t`); 433 `thr_kill(long id, int sig)`; 432 `thr_self(long *id)`; **native 72 `set_tid_address`** (A-1, A-3); 552 `fstatat` with the FreeBSD ino64 `struct stat`, `_Static_assert`ed (A-5). Split 552 into its own PR (L2b) if the layout work is large. | One `tests/` case per new slot, including the error path and a bad pointer (K-40), on both arches | additive |
| **L3** musl flip | Point musl at the L2 slots: `__NR_poll` 209; `__NR_rt_sigsuspend` 341, with the wrapper widening the 8-byte musl mask to FreeBSD's 16; `__NR_tkill` 433; `gettid()`/`__synccall` through a `thr_self` wrapper; `__NR_set_tid_address` `0x8000\|72`; `__NR_statx` stub and `__NR_fstatat64` 552 (musl's kstat path, with an `arch/*/kstat.h` matching FreeBSD). Remove the stale `SYS_SET_TID_ADDRESS`/`SYS_STATX` defines in `sys/arch/aarch64/kern/syscall.c`. | `bmake world image` on both arches; tcsh job control, `pthread` tests, `stat`/`ls -l`, `raise(SIGINT)`; the §2.1 scan shows none of 168/179/224/238/258/383 emitted | dynamic binaries move with libc.so; static binaries still use the old slots, which L2 kept |
| **L4** rebuild static ports | Rebuild `tools/ports/llvm` (and any other static port) against the L3 + E2 musl. Add the trap-site scan as `tools/ports/scan-syscalls` (or as an `abi-numbers` mode) and run it on the staged binaries. | The scan finds no 168/179/224/238/242/258/383 sites. An in-OS world build on **both** arches with a count of `not implemented` lines: expect 0 for 242. This also closes §3 | none |
| **L5** deprecation | Log once per pid when one of the six old slots is used (`deprecated Linux slot N, rebuild against current libc`). This is one observation cycle and can merge into L6 if the person prefers. | The log stays quiet after L4 | none |
| **L6** retire | Restore FreeBSD meanings: 168, 179, 258 → reserved (`SYSCALL_INVALID`), 224 → INVALID, 238 → `ktimer_gettime` NOTIMP, 383 → INVALID. On x86_64, retire the 158 `arch_prctl` intercept (musl now uses `sysarch` 165, `AMD64_SET_FSBASE`; the musl change goes in L3). Settle `prlimit64` 1024 (§2.4): drop the intercepts. | Full boot on both arches; `syscall-table` gate; old static binaries now get -ENOSYS | **break**: binaries linked before L3. CHANGELOG *Changed*. When to land it is the person's call |
| **L7** affinity | FreeBSD `cpuset_getaffinity` 487 (and `cpuset_setaffinity` 488 if wanted), returning the online-CPU mask; musl `sched_getaffinity` becomes a wrapper over it. Add the not-implemented log rate limit (§3, fix 4). | `nproc`/`sysconf` report the SMP CPU count on both arches; ld.lld runs threaded | additive |

### 4.2 W-009: errno at the POSIX boundary

End state: **one errno namespace, with FreeBSD numbers, from kernel to
libc**. That means the kernel uses `sys/errno.h` only and musl's constants
are patched to FreeBSD values (A-2). No translation table remains. A
permanent FreeBSD→Linux translation would make the POSIX face Linux-shaped,
which the design rule rules out. It is used below only as a bridge, so that
every step stays green.

| Step | PR content | Check | ABI |
|------|-----------|-------|-----|
| **E0** test | `tests/errnotest`: provoke each reachable errno ≥ 35 through real calls and compare `errno` with the *symbolic* constant. EAGAIN: `O_NONBLOCK` read of an empty pipe and of a tty. ENOSYS: an unimplemented slot. ENAMETOOLONG: a path over `PATH_MAX`. ENOTEMPTY: `rmdir` of a non-empty dir. ELOOP: if symlinks resolve. EINPROGRESS: non-blocking `connect`. ENOTSOCK: `send` on a file. It prints PASS/FAIL per case and keeps a committed expected-fail list, so it starts green. | Runs on both arches; the expected-fail list matches §1's table | none |
| **E1** one kernel namespace + bridge | `ubixos/errno.h` becomes a thin wrapper around `sys/errno.h` (it keeps `ENOERR` and uBix extras). lwIP: `LWIP_PROVIDE_ERRNO 0` + `LWIP_ERRNO_STDINCLUDE`/`LWIP_ERRNO_INCLUDE` → `sys/errno.h`, and `net/errno.h` is retired. Add a **bridge** `errno_to_user()` (a FreeBSD→Linux table) applied once to any `[-4095,-1]` result in `ksyscall_dispatch()` and in the MD intercepts that return errors (x86_64 `prlimit64`, aarch64 pre-cases). Audit every `== -E…`/`case E…` comparison and every literal negative errno in `sys/`. Fix handlers that set `td_retval[0] = -1` (x86_64 `sys_sysinfo`, `sys_nanosleep`, ...; musl reads -1 as EPERM) to a real `-E…`. | errnotest is all PASS on both arches (the expected-fail list is emptied); the network still works (DHCP, `httpsget`) | none visible: userland sees consistent Linux values, which fixes every ≥ 35 mismatch |
| **E2** musl to FreeBSD values | Give musl FreeBSD errno numbers: an `arch/{aarch64,x86_64}/bits/errno.h` override, so frozen i386 is untouched. In `syscall_arch.h` (both arches), the `return -38` becomes `return -ENOSYS` under its FreeBSD value. Check musl's `strerror` table and any in-tree code with numeric errno. **Remove the E1 bridge in the same PR**: kernel, musl and world move in lockstep. | `bmake kernel world image` on both arches; errnotest all PASS; tcsh, bmake, the in-OS world build | **break** for static binaries linked before E2: errnos ≥ 35 misdecode until they are rebuilt (L4/E3). CHANGELOG *Changed* |
| **E3** rebuild | Fold into **L4**: one rebuild of the static ports after E2. | Spot-check that `clang` on a missing file prints the right `strerror` | none |
| **E4** rule + gate | Standards: add an A-13 to `abi-and-syscalls.md` (the kernel errno is FreeBSD-numbered from `sys/errno.h` only). Add a gate that rejects new includes of `ubixos/errno.h`/`net/errno.h` and literal negative errno numbers in `sys/`. | `selftest.sh` | none |

### 4.3 WORK rows

`docs/WORK.md` is held by another lane. The proposed row changes are in this
PR's description. In short: W-009 and W-029 point their Notes at this
document. W-029 gains the extra Linux-numbered sites found here (x86_64 158
`arch_prctl`, the private 1024 `prlimit64` intercept, and the 242 trace to
the static LLVM port).

## Appendix: every musl `__NR_*`

Columns: **nr** is the effective number, identical on aarch64 and x86_64
(`0x8000|n` = native slot n, `0x40000|n` = libc stub). **Linux i386** is the
template number. **FreeBSD** is FreeBSD's slot name at *nr*. **kernel** is the
uBix POSIX-table entry at *nr* (name and status). **class** is as in §2.2.
**aarch64**/**x86_64** give the kernel route on each arch: `table` (POSIX
table), `MD` (pre-dispatch intercept), `native`, `libc` (stub, no trap), `out`
(out of range, -ENOSYS). **emitted** means this name's number appears in the
built `libc.a` (both arches agree; see §2.4 for the assembly-only sites).
For a stub row, "emitted" means libc passes the number to `__syscall_cp`,
which returns -ENOSYS without trapping.
Rows are sorted by number, with the stubs last.

| `__NR_` name | nr | Linux i386 | FreeBSD | kernel | class | aarch64 | x86_64 | emitted |
|--------------|----|-----------:|---------|--------|-------|---------|--------|:-------:|
| `restart_syscall` | 0 | 0 | `syscall` | `No Call` valid | Linux:live | table | table |  |
| `exit` | 1 | 1 | `_exit` | `exit` valid | FreeBSD | MD | MD | yes |
| `fork` | 2 | 2 | `fork` | `fork` valid | FreeBSD | MD | MD | yes |
| `read` | 3 | 3 | `read` | `read` valid | FreeBSD | table | table | yes |
| `write` | 4 | 4 | `write` | `write` valid | FreeBSD | MD | table | yes |
| `open` | 5 | 5 | `open` | `open` valid | FreeBSD | table | table | yes |
| `close` | 6 | 6 | `close` | `close` valid | FreeBSD | table | table | yes |
| `wait4` | 7 | 114 | `wait4` | `wiat4` valid | FreeBSD | MD | table | yes |
| `waitpid` | 7 | 7 | `wait4` | `wiat4` valid | FreeBSD | MD | table | yes |
| `creat` | 8 | 8 | `compat creat` | `No Call` invalid | FreeBSD | table | table |  |
| `link` | 9 | 9 | `link` | `link` notimp | FreeBSD | table | table | yes |
| `unlink` | 10 | 10 | `unlink` | `unlink` valid | FreeBSD | table | table | yes |
| `chdir` | 12 | 12 | `chdir` | `cgdur` valid | FreeBSD | table | table | yes |
| `fchdir` | 13 | 133 | `fchdir` | `fchdir` valid | FreeBSD | table | table | yes |
| `time` | 13 | 13 | `fchdir` | `fchdir` valid | Linux:live | table | table |  |
| `mknod` | 14 | 14 | `compat11 mknod` | `mknod` notimp | FreeBSD | table | table | yes |
| `chmod` | 15 | 15 | `chmod` | `chmod` valid | FreeBSD | table | table | yes |
| `break` | 17 | 17 | `break` | `break` valid | FreeBSD | MD | MD | yes |
| `brk` | 17 | 45 | `break` | `break` valid | FreeBSD | MD | MD | yes |
| `oldstat` | 18 | 18 | `compat4 getfsstat` | `No Call` invalid | Linux:latent | table | table |  |
| `getpid` | 20 | 20 | `getpid` | `getpid` valid | FreeBSD | table | table | yes |
| `mount` | 21 | 21 | `mount` | `mount` valid | FreeBSD | table | table | yes |
| `umount` | 22 | 22 | `unmount` | `unmount` valid | Linux:live | table | table |  |
| `setuid` | 23 | 23 | `setuid` | `setuid` valid | FreeBSD | MD | table | yes |
| `setuid32` | 23 | 213 | `setuid` | `setuid` valid | FreeBSD | MD | table | yes |
| `getuid` | 24 | 24 | `getuid` | `getuid` valid | FreeBSD | table | table | yes |
| `getuid32` | 24 | 199 | `getuid` | `getuid` valid | FreeBSD | table | table | yes |
| `geteuid32` | 25 | 201 | `geteuid` | `geteuid` valid | FreeBSD | table | table | yes |
| `stime` | 25 | 25 | `geteuid` | `geteuid` valid | Linux:live | table | table |  |
| `ptrace` | 26 | 26 | `ptrace` | `ptrace` notimp | FreeBSD | table | table | yes |
| `alarm` | 27 | 27 | `recvmsg` | `recvmsg` valid | Linux:live | table | table |  |
| `recvmsg` | 27 | 372 | `recvmsg` | `recvmsg` valid | FreeBSD | table | table | yes |
| `oldfstat` | 28 | 28 | `sendmsg` | `sendmsg` valid | Linux:live | table | table |  |
| `sendmsg` | 28 | 370 | `sendmsg` | `sendmsg` valid | FreeBSD | table | table | yes |
| `recvfrom` | 29 | 371 | `recvfrom` | `recvfrom` valid | FreeBSD | table | table | yes |
| `utime` | 30 | 30 | `accept` | `accept` valid | Linux:live | table | table |  |
| `getpeername` | 31 | 368 | `getpeername` | `getpeername` valid | FreeBSD | table | table | yes |
| `stty` | 31 | 31 | `getpeername` | `getpeername` valid | Linux:live | table | table |  |
| `getsockname` | 32 | 367 | `getsockname` | `getsockname` valid | FreeBSD | table | table | yes |
| `gtty` | 32 | 32 | `getsockname` | `getsockname` valid | Linux:live | table | table |  |
| `access` | 33 | 33 | `access` | `access` valid | FreeBSD | table | table | yes |
| `nice` | 34 | 34 | `chflags` | `chflags` notimp | Linux:latent | table | table |  |
| `ftime` | 35 | 35 | `fchflags` | `fchflags` notimp | Linux:latent | table | table |  |
| `sync` | 36 | 36 | `sync` | `sync` notimp | FreeBSD | table | table | yes |
| `kill` | 37 | 37 | `kill` | `kill` valid | FreeBSD | table | table | yes |
| `getppid` | 39 | 64 | `getppid` | `getppid` valid | FreeBSD | table | table | yes |
| `dup` | 41 | 41 | `dup` | `dup` valid | FreeBSD | table | table | yes |
| `pipe` | 42 | 42 | `compat10 pipe` | `pipe` valid | FreeBSD | table | table | yes |
| `getegid32` | 43 | 202 | `getegid` | `getegid` valid | FreeBSD | table | table | yes |
| `prof` | 44 | 44 | `profil` | `profil` notimp | Linux:latent | table | table |  |
| `setgid` | 46 | 46 | `compat sigaction` | `No Call` invalid | Linux:latent | table | table |  |
| `getgid` | 47 | 47 | `getgid` | `getuid` valid | FreeBSD | table | table | yes |
| `getgid32` | 47 | 200 | `getgid` | `getuid` valid | FreeBSD | table | table | yes |
| `signal` | 48 | 48 | `compat sigprocmask` | `No Call` invalid | Linux:latent | table | table |  |
| `geteuid` | 49 | 49 | `getlogin` | `getlogin` valid | Linux:live | table | table |  |
| `getegid` | 50 | 50 | `setlogin` | `setlogin` notimp | Linux:latent | table | table |  |
| `acct` | 51 | 51 | `acct` | `acct` notimp | FreeBSD | table | table | yes |
| `umount2` | 52 | 52 | `compat sigpending` | `No Call` invalid | Linux:latent | table | table | yes |
| `lock` | 53 | 53 | `sigaltstack` | `sigaltstack` valid | Linux:live | table | table |  |
| `sigaltstack` | 53 | 186 | `sigaltstack` | `sigaltstack` valid | FreeBSD | table | table | yes |
| `ioctl` | 54 | 54 | `ioctl` | `ioctl` valid | FreeBSD | table | table | yes |
| `mpx` | 56 | 56 | `revoke` | `revoke` notimp | Linux:latent | table | table |  |
| `symlink` | 57 | 83 | `symlink` | `symlink` valid | FreeBSD | table | table | yes |
| `readlink` | 58 | 85 | `readlink` | `readlink` valid | FreeBSD | table | table | yes |
| `ulimit` | 58 | 58 | `readlink` | `readlink` valid | Linux:live | table | table |  |
| `execve` | 59 | 11 | `execve` | `execve` valid | FreeBSD | MD | table | yes |
| `oldolduname` | 59 | 59 | `execve` | `execve` valid | Linux:live | MD | table |  |
| `umask` | 60 | 60 | `umask` | `umask` valid | FreeBSD | table | table | yes |
| `chroot` | 61 | 61 | `chroot` | `chroot` notimp | FreeBSD | table | table | yes |
| `ustat` | 62 | 62 | `compat fstat` | `No Call` invalid | Linux:latent | table | table |  |
| `msync` | 65 | 144 | `msync` | `msync` valid | FreeBSD | table | table | yes |
| `vfork` | 66 | 190 | `vfork` | `vfork` notimp | FreeBSD | table | table |  |
| `sigaction` | 67 | 67 | `obsol vread` | `No Call` invalid | Linux:latent | table | table |  |
| `sgetmask` | 68 | 68 | `obsol vwrite` | `No Call` invalid | Linux:latent | table | table |  |
| `ssetmask` | 69 | 69 | `obsol sbrk` | `sbrk` notimp | Linux:latent | table | table |  |
| `sigsuspend` | 72 | 72 | `compat11 vadvise` | `vadvise` notimp | Linux:latent | table | table |  |
| `munmap` | 73 | 91 | `munmap` | `munmap` valid | FreeBSD | MD | table | yes |
| `sigpending` | 73 | 73 | `munmap` | `munmap` valid | Linux:live | MD | table |  |
| `mprotect` | 74 | 125 | `mprotect` | `mprotect` valid | FreeBSD | MD | table | yes |
| `madvise` | 75 | 219 | `madvise` | `madvise` valid | FreeBSD | table | table | yes |
| `mincore` | 78 | 218 | `mincore` | `mincore` notimp | FreeBSD | table | table | yes |
| `getgroups` | 79 | 80 | `compat14 getgroups` | `getgroups` notimp | FreeBSD | table | table | yes |
| `getgroups32` | 79 | 205 | `compat14 getgroups` | `getgroups` notimp | FreeBSD | table | table | yes |
| `settimeofday_time32` | 79 | 79 | `compat14 getgroups` | `getgroups` notimp | Linux:latent | table | table |  |
| `setgroups` | 80 | 81 | `compat14 setgroups` | `setgroups` valid | FreeBSD | table | table | yes |
| `setgroups32` | 80 | 206 | `compat14 setgroups` | `setgroups` valid | FreeBSD | table | table | yes |
| `getpgrp` | 81 | 65 | `getpgrp` | `getpgrp` valid | FreeBSD | table | table |  |
| `select` | 82 | 82 | `setpgid` | `setpgid` valid | Linux:live | table | table |  |
| `setpgid` | 82 | 57 | `setpgid` | `setpgid` valid | FreeBSD | table | table | yes |
| `setitimer` | 83 | 104 | `setitimer` | `setitimer` valid | FreeBSD | table | table | yes |
| `oldlstat` | 84 | 84 | `compat wait` | `old wait` invalid | Linux:latent | table | table |  |
| `swapon` | 85 | 87 | `swapon` | `swapon` notimp | FreeBSD | table | table | yes |
| `getitimer` | 86 | 105 | `getitimer` | `getitimer` valid | FreeBSD | table | table | yes |
| `uselib` | 86 | 86 | `getitimer` | `getitimer` valid | Linux:live | table | table |  |
| `readdir` | 89 | 89 | `getdtablesize` | `getdtablesize` notimp | Linux:latent | table | table |  |
| `dup2` | 90 | 63 | `dup2` | `dup2` valid | FreeBSD | table | table | yes |
| `mmap` | 90 | 90 | `dup2` | `dup2` valid | Linux:live | table | table |  |
| `fcntl` | 92 | 55 | `fcntl` | `fcntl` valid | FreeBSD | table | table | yes |
| `fcntl64` | 92 | 221 | `fcntl` | `fcntl` valid | FreeBSD | table | table | yes |
| `_newselect` | 93 | 142 | `select` | `select` valid | FreeBSD | table | table | yes |
| `fsync` | 95 | 118 | `fsync` | `fsync` valid | FreeBSD | table | table | yes |
| `setpriority` | 96 | 97 | `setpriority` | `setpriority` notimp | FreeBSD | table | table | yes |
| `socket` | 97 | 359 | `socket` | `socket` valid | FreeBSD | table | table | yes |
| `connect` | 98 | 362 | `connect` | `connect` valid | FreeBSD | table | table | yes |
| `profil` | 98 | 98 | `connect` | `connect` valid | Linux:live | table | table |  |
| `statfs` | 99 | 99 | `compat accept` | `old accept` invalid | Linux:latent | table | table |  |
| `fstatfs` | 100 | 100 | `getpriority` | `getpriority` notimp | Linux:latent | table | table |  |
| `getpriority` | 100 | 96 | `getpriority` | `getpriority` notimp | FreeBSD | table | table | yes |
| `ioperm` | 101 | 101 | `compat send` | `old send` invalid | Linux:latent | table | table | yes |
| `syslog` | 103 | 103 | `compat sigreturn` | `old sigreturn` invalid | Linux:latent | table | table | yes |
| `bind` | 104 | 361 | `bind` | `bind` valid | FreeBSD | table | table | yes |
| `setsockopt` | 105 | 366 | `setsockopt` | `setsockopt` valid | FreeBSD | table | table | yes |
| `listen` | 106 | 363 | `listen` | `listen` valid | FreeBSD | table | table | yes |
| `stat` | 106 | 106 | `listen` | `listen` valid | Linux:live | table | table |  |
| `lstat` | 107 | 107 | `obsol vtimes` | `obsolete vtimes` invalid | Linux:latent | table | table |  |
| `fstat` | 108 | 108 | `compat sigvec` | `old sigvec` invalid | Linux:latent | table | table |  |
| `olduname` | 109 | 109 | `compat sigblock` | `old sigblock` invalid | Linux:latent | table | table |  |
| `iopl` | 110 | 110 | `compat sigsetmask` | `old sigsetmask` invalid | Linux:latent | table | table | yes |
| `vhangup` | 111 | 111 | `compat sigsuspend` | `old sigsuspend` invalid | Linux:latent | table | table | yes |
| `idle` | 112 | 112 | `compat sigstack` | `old sigstack` invalid | Linux:latent | table | table |  |
| `vm86old` | 113 | 113 | `compat recvmsg` | `old recvmsg` invalid | Linux:latent | table | table |  |
| `swapoff` | 115 | 115 | `obsol vtrace` | `obsolete vtrace` invalid | Linux:latent | table | table | yes |
| `gettimeofday_time32` | 116 | 78 | `gettimeofday` | `gettimeofday` valid | FreeBSD | table | table | yes |
| `getrusage` | 117 | 77 | `getrusage` | `getrusage` valid | FreeBSD | table | table | yes |
| `getsockopt` | 118 | 365 | `getsockopt` | `getsockopt` valid | FreeBSD | table | table | yes |
| `sigreturn` | 119 | 119 | `reserved` | `resuba` notimp | Linux:latent | table | table |  |
| `clone` | 120 | 120 | `readv` | `readv` valid | Linux:live | table | table |  |
| `readv` | 120 | 145 | `readv` | `readv` valid | FreeBSD | table | table | yes |
| `writev` | 121 | 146 | `writev` | `writev` valid | FreeBSD | MD | table | yes |
| `fchown` | 123 | 95 | `fchown` | `fchown` valid | FreeBSD | table | table | yes |
| `fchown32` | 123 | 207 | `fchown` | `fchown` valid | FreeBSD | table | table | yes |
| `modify_ldt` | 123 | 123 | `fchown` | `fchown` valid | Linux:live | table | table |  |
| `fchmod` | 124 | 94 | `fchmod` | `fchmod` valid | FreeBSD | table | table | yes |
| `setreuid` | 126 | 70 | `setreuid` | `setreuid` notimp | FreeBSD | table | table | yes |
| `setreuid32` | 126 | 203 | `setreuid` | `setreuid` notimp | FreeBSD | table | table | yes |
| `sigprocmask` | 126 | 126 | `setreuid` | `setreuid` notimp | Linux:latent | table | table |  |
| `create_module` | 127 | 127 | `setregid` | `setregid` notimp | Linux:latent | table | table |  |
| `setregid` | 127 | 71 | `setregid` | `setregid` notimp | FreeBSD | table | table | yes |
| `setregid32` | 127 | 204 | `setregid` | `setregid` notimp | FreeBSD | table | table | yes |
| `rename` | 128 | 38 | `rename` | `rename` valid | FreeBSD | table | table | yes |
| `get_kernel_syms` | 130 | 130 | `compat ftruncate` | `old fruncate` invalid | Linux:latent | table | table |  |
| `flock` | 131 | 143 | `flock` | `flock` notimp | FreeBSD | table | table | yes |
| `quotactl` | 131 | 131 | `flock` | `flock` notimp | Linux:latent | table | table | yes |
| `sendto` | 133 | 369 | `sendto` | `sendto` valid | FreeBSD | table | table | yes |
| `bdflush` | 134 | 134 | `shutdown` | `shutdown` notimp | Linux:latent | table | table |  |
| `shutdown` | 134 | 373 | `shutdown` | `shutdown` notimp | FreeBSD | table | table | yes |
| `socketpair` | 135 | 360 | `socketpair` | `socketpair` notimp | FreeBSD | table | table | yes |
| `sysfs` | 135 | 135 | `socketpair` | `socketpair` notimp | Linux:latent | table | table |  |
| `mkdir` | 136 | 39 | `mkdir` | `mkdir` valid | FreeBSD | table | table | yes |
| `afs_syscall` | 137 | 137 | `rmdir` | `rmdir` valid | Linux:live | table | table |  |
| `rmdir` | 137 | 40 | `rmdir` | `rmdir` valid | FreeBSD | table | table | yes |
| `setfsuid` | 138 | 138 | `utimes` | `utimes` valid | Linux:live | table | table |  |
| `setfsgid` | 139 | 139 | `obsol sigreturn` | `obsolete sigreturn` invalid | Linux:latent | table | table |  |
| `setsid` | 147 | 66 | `setsid` | `setsid` valid | FreeBSD | table | table | yes |
| `sched_setparam` | 154 | 154 | `nlm_syscall` | `nlm_syscall` notimp | Linux:latent | table | table | yes |
| `sched_getparam` | 155 | 155 | `nfssvc` | `nfssvc` notimp | Linux:latent | table | table | yes |
| `sched_setscheduler` | 156 | 156 | `compat getdirentries` | `old getdirentries` invalid | Linux:latent | table | table | yes |
| `sched_getscheduler` | 157 | 157 | `compat4 statfs` | `old statfs` invalid | Linux:latent | table | table | yes |
| `sched_get_priority_max` | 159 | 159 | `reserved` | `nosys` notimp | Linux:latent | table | table | yes |
| `sched_get_priority_min` | 160 | 160 | `lgetfh` | `lgetfh` notimp | Linux:latent | table | table | yes |
| `sched_rr_get_interval` | 161 | 161 | `getfh` | `getfh` notimp | Linux:latent | table | table | yes |
| `setresuid` | 164 | 164 | `compat4 uname` | `uname` valid | Linux:live | table | table |  |
| `uname` | 164 | 122 | `compat4 uname` | `uname` valid | FreeBSD | table | table | yes |
| `getresuid` | 165 | 165 | `sysarch` | `sysarch` debug | Linux:live | table | table |  |
| `vm86` | 166 | 166 | `rtprio` | `rtprio` notimp | Linux:latent | table | table |  |
| `query_module` | 167 | 167 | `reserved` | `nosys` notimp | Linux:latent | table | table |  |
| `poll` | 168 | 168 | `reserved` | `poll` valid | Linux:squat | table | table | yes |
| `nfsservctl` | 169 | 169 | `semsys` | `semsys` notimp | Linux:latent | table | table |  |
| `setresgid` | 170 | 170 | `msgsys` | `msgsys` notimp | Linux:latent | table | table |  |
| `getresgid` | 171 | 171 | `shmsys` | `shmsys` notimp | Linux:latent | table | table |  |
| `prctl` | 172 | 172 | `reserved` | `nosys` notimp | Linux:latent | table | table | yes |
| `rt_sigreturn` | 173 | 173 | `compat6 pread` | `old pread` invalid | Linux:latent | table | table |  |
| `rt_sigpending` | 176 | 176 | `ntp_adjtime` | `ntp_adjtime` notimp | Linux:latent | table | table | yes |
| `rt_sigtimedwait` | 177 | 177 | `reserved` | `sfork` notimp | Linux:latent | table | table | yes |
| `rt_sigqueueinfo` | 178 | 178 | `reserved` | `getdescriptor` notimp | Linux:latent | table | table | yes |
| `rt_sigsuspend` | 179 | 179 | `reserved` | `sigsuspend` valid | Linux:squat | table | table | yes |
| `setgid32` | 181 | 214 | `setgid` | `setgid` valid | FreeBSD | MD | table | yes |
| `capget` | 184 | 184 | `obsol lfs_bmapv` | `lfs_bmapv` notimp | Linux:latent | table | table | yes |
| `capset` | 185 | 185 | `obsol lfs_markv` | `lfs_markv` notimp | Linux:latent | table | table | yes |
| `sendfile` | 187 | 187 | `obsol lfs_segwait` | `lfs_segwait` notimp | Linux:latent | table | table |  |
| `getpmsg` | 188 | 188 | `compat11 stat` | `stat` valid | Linux:live | table | table |  |
| `stat64` | 188 | 195 | `compat11 stat` | `stat` valid | FreeBSD | table | table |  |
| `fstat64` | 189 | 197 | `compat11 fstat` | `fstat` valid | FreeBSD | table | table |  |
| `putpmsg` | 189 | 189 | `compat11 fstat` | `fstat` valid | Linux:live | table | table |  |
| `lstat64` | 190 | 196 | `compat11 lstat` | `lstat` valid | FreeBSD | table | table |  |
| `getrlimit` | 194 | 76 | `getrlimit` | `getrlimit` valid | FreeBSD | table | table | yes |
| `ugetrlimit` | 194 | 191 | `getrlimit` | `getrlimit` valid | FreeBSD | table | table | yes |
| `setrlimit` | 195 | 75 | `setrlimit` | `setrlimit` valid | FreeBSD | table | table | yes |
| `_sysctl` | 202 | 149 | `__sysctl` | `__sysctl` valid | FreeBSD | table | table |  |
| `mlock` | 203 | 150 | `mlock` | `mlock` notimp | FreeBSD | table | table | yes |
| `munlock` | 204 | 151 | `munlock` | `munlock` notimp | FreeBSD | table | table | yes |
| `getpgid` | 207 | 132 | `getpgid` | `getpgid` valid | FreeBSD | table | table | yes |
| `setresuid32` | 208 | 208 | `reserved` | `reboot` notimp | Linux:latent | table | table |  |
| `setresgid32` | 210 | 210 | `__semctl` | `lkmnosys` notimp | Linux:latent | table | table |  |
| `setfsuid32` | 215 | 215 | `__semctl` | `lkmnosys` notimp | Linux:latent | table | table | yes |
| `setfsgid32` | 216 | 216 | `__semctl` | `lkmnosys` notimp | Linux:latent | table | table | yes |
| `pivot_root` | 217 | 217 | `__semctl` | `lkmnosys` notimp | Linux:latent | table | table | yes |
| `gettid` | 224 | 224 | `compat7 msgctl` | `gettid` valid | Linux:squat | table | table | yes |
| `readahead` | 225 | 225 | `msgget` | `msgget` notimp | Linux:latent | table | table | yes |
| `clock_gettime32` | 232 | 265 | `clock_gettime` | `clock_gettime` valid | FreeBSD | table | table | yes |
| `tkill` | 238 | 238 | `ktimer_gettime` | `tkill` valid | Linux:squat | table | table | yes |
| `sendfile64` | 239 | 239 | `ktimer_getoverrun` | `ktimer_getoverrun` notimp | Linux:latent | table | table | yes |
| `nanosleep` | 240 | 162 | `nanosleep` | `nanosleep` valid | FreeBSD | MD | table | yes |
| `get_thread_area` | 244 | 244 | `clock_nanosleep` | `clock_nanosleep` notimp | Linux:latent | table | table |  |
| `io_setup` | 245 | 245 | `reserved` | `No Call` invalid | Linux:latent | table | table |  |
| `io_destroy` | 246 | 246 | `reserved` | `No Call` invalid | Linux:latent | table | table |  |
| `io_getevents` | 247 | 247 | `clock_getcpuclockid2` | `clock_getcpuclockid2` notimp | Linux:latent | table | table |  |
| `io_submit` | 248 | 248 | `ntp_gettime` | `ntp_gettime` notimp | Linux:latent | table | table |  |
| `io_cancel` | 249 | 249 | `reserved` | `No Call` invalid | Linux:latent | table | table |  |
| `fadvise64` | 250 | 250 | `minherit` | `minherit` notimp | Linux:latent | table | table |  |
| `lookup_dcookie` | 253 | 253 | `issetugid` | `issetugid` valid | Linux:live | table | table |  |
| `chown` | 254 | 182 | `lchown` | `lchown` valid | remap-wrong | table | table | yes |
| `chown32` | 254 | 212 | `lchown` | `lchown` valid | remap-wrong | table | table | yes |
| `lchown` | 254 | 16 | `lchown` | `lchown` valid | FreeBSD | table | table | yes |
| `lchown32` | 254 | 198 | `lchown` | `lchown` valid | FreeBSD | table | table | yes |
| `remap_file_pages` | 257 | 257 | `lio_listio` | `lio_listio` notimp | Linux:latent | table | table | yes |
| `set_tid_address` | 258 | 258 | `reserved` | `set_tid_address` valid | Linux:squat | table | table | yes |
| `timer_create` | 259 | 259 | `reserved` | `No Call` invalid | Linux:latent | table | table | yes |
| `timer_getoverrun` | 262 | 262 | `reserved` | `No Call` invalid | Linux:latent | table | table | yes |
| `timer_delete` | 263 | 263 | `reserved` | `No Call` invalid | Linux:latent | table | table | yes |
| `tgkill` | 270 | 270 | `reserved` | `No Call` invalid | Linux:latent | table | table |  |
| `utimes` | 271 | 271 | `reserved` | `No Call` invalid | Linux:latent | table | table | yes |
| `getdents` | 272 | 141 | `compat11 getdents` | `getdents` valid | FreeBSD | table | table | yes |
| `getdents64` | 272 | 220 | `compat11 getdents` | `getdents` valid | FreeBSD | table | table | yes |
| `vserver` | 273 | 273 | `reserved` | `No Call` invalid | Linux:latent | table | table |  |
| `mbind` | 274 | 274 | `lchmod` | `lchmod` notimp | Linux:latent | table | table |  |
| `get_mempolicy` | 275 | 275 | `obsol netbsd_lchown` | `netbsd_lchown` notimp | Linux:latent | table | table |  |
| `set_mempolicy` | 276 | 276 | `lutimes` | `lutimes` valid | Linux:live | table | table |  |
| `mq_open` | 277 | 277 | `obsol netbsd_msync` | `netbsd_msync` notimp | Linux:latent | table | table | yes |
| `mq_unlink` | 278 | 278 | `compat11 nstat` | `nstat` notimp | Linux:latent | table | table | yes |
| `mq_timedsend` | 279 | 279 | `compat11 nfstat` | `nfstat` notimp | Linux:latent | table | table | yes |
| `mq_timedreceive` | 280 | 280 | `compat11 nlstat` | `nlstat` notimp | Linux:latent | table | table | yes |
| `mq_notify` | 281 | 281 | `reserved` | `No Call` invalid | Linux:latent | table | table | yes |
| `mq_getsetattr` | 282 | 282 | `reserved` | `No Call` invalid | Linux:latent | table | table | yes |
| `kexec_load` | 283 | 283 | `reserved` | `No Call` invalid | Linux:latent | table | table |  |
| `waitid` | 284 | 284 | `reserved` | `No Call` invalid | Linux:latent | table | table | yes |
| `add_key` | 286 | 286 | `reserved` | `No Call` invalid | Linux:latent | table | table |  |
| `request_key` | 287 | 287 | `reserved` | `No Call` invalid | Linux:latent | table | table |  |
| `keyctl` | 288 | 288 | `reserved` | `No Call` invalid | Linux:latent | table | table |  |
| `ioprio_set` | 289 | 289 | `preadv` | `preadv` notimp | Linux:latent | table | table |  |
| `preadv` | 289 | 333 | `preadv` | `preadv` notimp | FreeBSD | table | table | yes |
| `ioprio_get` | 290 | 290 | `pwritev` | `pwritev` notimp | Linux:latent | table | table |  |
| `pwritev` | 290 | 334 | `pwritev` | `pwritev` notimp | FreeBSD | table | table | yes |
| `inotify_init` | 291 | 291 | `reserved` | `No Call` invalid | Linux:latent | table | table | yes |
| `inotify_add_watch` | 292 | 292 | `reserved` | `No Call` invalid | Linux:latent | table | table | yes |
| `inotify_rm_watch` | 293 | 293 | `reserved` | `No Call` invalid | Linux:latent | table | table | yes |
| `migrate_pages` | 294 | 294 | `reserved` | `feek` valid | Linux:live | table | table |  |
| `renameat` | 302 | 302 | `modfnext` | `modfnext` notimp | Linux:latent | table | table | yes |
| `linkat` | 303 | 303 | `modfind` | `modfind` notimp | Linux:latent | table | table | yes |
| `symlinkat` | 304 | 304 | `kldload` | `kldload` notimp | Linux:latent | table | table | yes |
| `readlinkat` | 305 | 305 | `kldunload` | `kldunload` notimp | Linux:latent | table | table | yes |
| `fchmodat` | 306 | 306 | `kldfind` | `kldfind` notimp | Linux:latent | table | table | yes |
| `pselect6` | 308 | 308 | `kldstat` | `kldstat` notimp | Linux:latent | table | table | yes |
| `ppoll` | 309 | 309 | `kldfirstmod` | `kldfirstmod` notimp | Linux:latent | table | table | yes |
| `getsid` | 310 | 147 | `getsid` | `getsid` notimp | FreeBSD | table | table | yes |
| `unshare` | 310 | 310 | `getsid` | `getsid` notimp | Linux:latent | table | table | yes |
| `set_robust_list` | 311 | 311 | `setresuid` | `setresuid` notimp | Linux:latent | table | table | yes |
| `get_robust_list` | 312 | 312 | `setresgid` | `setresgid` notimp | Linux:latent | table | table | yes |
| `splice` | 313 | 313 | `obsol signanosleep` | `obsolete signanosleep` invalid | Linux:latent | table | table | yes |
| `sync_file_range` | 314 | 314 | `aio_return` | `aio_return` notimp | Linux:latent | table | table | yes |
| `tee` | 315 | 315 | `aio_suspend` | `aio_suspend` notimp | Linux:latent | table | table | yes |
| `vmsplice` | 316 | 316 | `aio_cancel` | `aio_cancel` notimp | Linux:latent | table | table | yes |
| `move_pages` | 317 | 317 | `aio_error` | `aio_error` notimp | Linux:latent | table | table |  |
| `getcpu` | 318 | 318 | `compat6 aio_read` | `old aio_read` invalid | Linux:latent | table | table | yes |
| `signalfd` | 321 | 321 | `yield` | `yield` notimp | Linux:latent | table | table | yes |
| `eventfd` | 323 | 323 | `obsol thr_wakeup` | `obsolete thr_wakeup` invalid | Linux:latent | table | table | yes |
| `fallocate` | 324 | 324 | `mlockall` | `mlockall` notimp | Linux:latent | table | table | yes |
| `mlockall` | 324 | 152 | `mlockall` | `mlockall` notimp | FreeBSD | table | table | yes |
| `munlockall` | 325 | 153 | `munlockall` | `munlockall` notimp | FreeBSD | table | table | yes |
| `getcwd` | 326 | 183 | `__getcwd` | `__getcwd` valid | FreeBSD | table | table | yes |
| `signalfd4` | 327 | 327 | `sched_setparam` | `sched_setparam` notimp | Linux:latent | table | table | yes |
| `eventfd2` | 328 | 328 | `sched_getparam` | `sched_getparam` notimp | Linux:latent | table | table | yes |
| `dup3` | 330 | 330 | `sched_getscheduler` | `sched_getscheduler` notimp | Linux:latent | table | table | yes |
| `sched_yield` | 331 | 158 | `sched_yield` | `sched_yield` valid | FreeBSD | table | table | yes |
| `inotify_init1` | 332 | 332 | `sched_get_priority_max` | `sched_get_priority_max` notimp | Linux:latent | table | table | yes |
| `rt_tgsigqueueinfo` | 335 | 335 | `utrace` | `utrace` notimp | Linux:latent | table | table |  |
| `perf_event_open` | 336 | 336 | `compat4 sendfile` | `old sendfile` invalid | Linux:latent | table | table |  |
| `recvmmsg` | 337 | 337 | `kldsym` | `kldsym` notimp | Linux:latent | table | table | yes |
| `fanotify_init` | 338 | 338 | `jail` | `jail` notimp | Linux:latent | table | table | yes |
| `fanotify_mark` | 339 | 339 | `nnpfs_syscall` | `nnpfs_syscall` notimp | Linux:latent | table | table | yes |
| `rt_sigprocmask` | 340 | 175 | `sigprocmask` | `sigprocmask` valid | FreeBSD | table | table | yes |
| `name_to_handle_at` | 341 | 341 | `sigsuspend` | `sigsuspend` notimp | Linux:latent | table | table | yes |
| `open_by_handle_at` | 342 | 342 | `compat4 sigaction` | `old sigaction` invalid | Linux:latent | table | table | yes |
| `syncfs` | 344 | 344 | `compat4 sigreturn` | `old sigreturn` invalid | Linux:latent | table | table | yes |
| `sendmmsg` | 345 | 345 | `sigtimedwait` | `sigtimedwait` notimp | Linux:latent | table | table |  |
| `setns` | 346 | 346 | `sigwaitinfo` | `sigwaitinfo` notimp | Linux:latent | table | table | yes |
| `process_vm_readv` | 347 | 347 | `__acl_get_file` | `__acl_get_file` notimp | Linux:latent | table | table | yes |
| `process_vm_writev` | 348 | 348 | `__acl_set_file` | `__acl_set_file` notimp | Linux:latent | table | table | yes |
| `kcmp` | 349 | 349 | `__acl_get_fd` | `__acl_get_fd` notimp | Linux:latent | table | table |  |
| `sched_setattr` | 351 | 351 | `__acl_delete_file` | `__acl_delete_file` notimp | Linux:latent | table | table |  |
| `sched_getattr` | 352 | 352 | `__acl_delete_fd` | `__acl_delete_fd` notimp | Linux:latent | table | table |  |
| `renameat2` | 353 | 353 | `__acl_aclcheck_file` | `__acl_aclcheck_file` notimp | Linux:latent | table | table | yes |
| `seccomp` | 354 | 354 | `__acl_aclcheck_fd` | `__acl_aclcheck_fd` notimp | Linux:latent | table | table |  |
| `memfd_create` | 356 | 356 | `extattr_set_file` | `extattr_set_file` notimp | Linux:latent | table | table | yes |
| `bpf` | 357 | 357 | `extattr_get_file` | `extattr_get_file` notimp | Linux:latent | table | table |  |
| `execveat` | 358 | 358 | `extattr_delete_file` | `extattr_delete_file` notimp | Linux:latent | table | table | yes |
| `getresuid32` | 360 | 209 | `getresuid` | `getresuid` notimp | FreeBSD | table | table | yes |
| `getresgid32` | 361 | 211 | `getresgid` | `getresgid` notimp | FreeBSD | table | table | yes |
| `userfaultfd` | 374 | 374 | `__setugid` | `__setugid` notimp | Linux:latent | table | table |  |
| `mlock2` | 376 | 376 | `eaccess` | `eaccess` notimp | Linux:latent | table | table | yes |
| `copy_file_range` | 377 | 377 | `afs3_syscall` | `afs2_syscall` notimp | Linux:latent | table | table | yes |
| `pkey_mprotect` | 380 | 380 | `obsol kse_wakeup` | `kse_wakeup` notimp | Linux:latent | table | table |  |
| `pkey_alloc` | 381 | 381 | `obsol kse_create` | `kse_create` notimp | Linux:latent | table | table |  |
| `pkey_free` | 382 | 382 | `obsol kse_thr_interrupt` | `kse_thr_interrupt` notimp | Linux:latent | table | table |  |
| `statx` | 383 | 383 | `obsol kse_release` | `statx` valid | Linux:squat | table | table | yes |
| `arch_prctl` | 384 | 384 | `__mac_get_proc` | `__mac_get_proc` notimp | Linux:latent | table | table | yes |
| `io_pgetevents` | 385 | 385 | `__mac_set_proc` | `__mac_set_proc` notimp | Linux:latent | table | table |  |
| `rseq` | 386 | 386 | `__mac_get_fd` | `__mac_get_fd` notimp | Linux:latent | table | table |  |
| `statfs64` | 396 | 268 | `compat11 statfs` | `statfs` valid | FreeBSD | table | table | yes |
| `fstatfs64` | 397 | 269 | `compat11 fstatfs` | `fstatfs` valid | FreeBSD | table | table | yes |
| `io_pgetevents_time64` | 416 | 416 | `sigaction` | `sigaction` valid | Linux:live | table | table |  |
| `rt_sigaction` | 416 | 174 | `sigaction` | `sigaction` valid | FreeBSD | table | table | yes |
| `pidfd_send_signal` | 424 | 424 | `compat13 swapoff` | `swapoff` notimp | Linux:latent | table | table |  |
| `io_uring_setup` | 425 | 425 | `__acl_get_link` | `__acl_get_link` notimp | Linux:latent | table | table |  |
| `io_uring_enter` | 426 | 426 | `__acl_set_link` | `__acl_set_link` notimp | Linux:latent | table | table |  |
| `io_uring_register` | 427 | 427 | `__acl_delete_link` | `__acl_delete_link` notimp | Linux:latent | table | table |  |
| `open_tree` | 428 | 428 | `__acl_aclcheck_link` | `__acl_aclcheck_link` notimp | Linux:latent | table | table |  |
| `move_mount` | 429 | 429 | `sigwait` | `sigwait` notimp | Linux:latent | table | table |  |
| `fsopen` | 430 | 430 | `thr_create` | `thr_create` notimp | Linux:latent | table | table |  |
| `fsconfig` | 431 | 431 | `thr_exit` | `thr_exit` notimp | Linux:latent | table | table |  |
| `fsmount` | 432 | 432 | `thr_self` | `thr_self` notimp | Linux:latent | table | table |  |
| `fspick` | 433 | 433 | `thr_kill` | `thr_kill` notimp | Linux:latent | table | table |  |
| `pidfd_open` | 434 | 434 | `compat10 _umtx_lock` | `No Call` invalid | Linux:latent | table | table |  |
| `clone3` | 435 | 435 | `compat10 _umtx_unlock` | `No Call` invalid | Linux:latent | table | table |  |
| `close_range` | 436 | 436 | `jail_attach` | `jail_attach` notimp | Linux:latent | table | table |  |
| `openat2` | 437 | 437 | `extattr_list_fd` | `extattr_list_fd` notimp | Linux:latent | table | table |  |
| `pidfd_getfd` | 438 | 438 | `extattr_list_file` | `extattr_list_file` notimp | Linux:latent | table | table |  |
| `process_madvise` | 440 | 440 | `obsol kse_switchin` | `kse_switchin` notimp | Linux:latent | table | table |  |
| `mount_setattr` | 442 | 442 | `thr_suspend` | `thr_suspend` notimp | Linux:latent | table | table |  |
| `quotactl_fd` | 443 | 443 | `thr_wake` | `thr_wake` notimp | Linux:latent | table | table |  |
| `landlock_create_ruleset` | 444 | 444 | `kldunloadf` | `kldunloadf` notimp | Linux:latent | table | table |  |
| `landlock_add_rule` | 445 | 445 | `audit` | `audit` notimp | Linux:latent | table | table |  |
| `landlock_restrict_self` | 446 | 446 | `auditon` | `auditon` notimp | Linux:latent | table | table |  |
| `memfd_secret` | 447 | 447 | `getauid` | `getauid` notimp | Linux:latent | table | table |  |
| `process_mrelease` | 448 | 448 | `setauid` | `setauid` notimp | Linux:latent | table | table |  |
| `futex_waitv` | 449 | 449 | `getaudit` | `getaudit` notimp | Linux:latent | table | table |  |
| `set_mempolicy_home_node` | 450 | 450 | `setaudit` | `setauid` notimp | Linux:latent | table | table |  |
| `cachestat` | 451 | 451 | `getaudit_addr` | `getaudit_addr` notimp | Linux:latent | table | table |  |
| `fchmodat2` | 452 | 452 | `setaudit_addr` | `setaudit_addr` notimp | Linux:latent | table | table | yes |
| `map_shadow_stack` | 453 | 453 | `auditctl` | `auditctl` notimp | Linux:latent | table | table |  |
| `futex_wake` | 454 | 454 | `_umtx_op` | `_umtx_op` notimp | Linux:latent | table | table |  |
| `futex_wait` | 455 | 455 | `thr_new` | `thr_new` notimp | Linux:latent | table | table |  |
| `futex_requeue` | 456 | 456 | `sigqueue` | `sigqueue` notimp | Linux:latent | table | table |  |
| `statmount` | 457 | 457 | `kmq_open` | `kmq_open` notimp | Linux:latent | table | table |  |
| `listmount` | 458 | 458 | `kmq_setattr` | `kmq_setattr` notimp | Linux:latent | table | table |  |
| `lsm_get_self_attr` | 459 | 459 | `kmq_timedreceive` | `kmq_timedreceive` notimp | Linux:latent | table | table |  |
| `lsm_set_self_attr` | 460 | 460 | `kmq_timedsend` | `kmq_timedsend` notimp | Linux:latent | table | table |  |
| `lsm_list_modules` | 461 | 461 | `kmq_notify` | `kmq_notify` notimp | Linux:latent | table | table |  |
| `mseal` | 462 | 462 | `kmq_unlink` | `kmq_unlink` notimp | Linux:latent | table | table |  |
| `setxattrat` | 463 | 463 | `abort2` | `abort2` notimp | Linux:latent | table | table |  |
| `getxattrat` | 464 | 464 | `thr_set_name` | `thr_set_name` notimp | Linux:latent | table | table |  |
| `listxattrat` | 465 | 465 | `aio_fsync` | `aio_fsync` notimp | Linux:latent | table | table |  |
| `removexattrat` | 466 | 466 | `rtprio_thread` | `rtprio_thread` notimp | Linux:latent | table | table |  |
| `open_tree_attr` | 467 | 467 | `reserved` | `No Call` invalid | Linux:latent | table | table |  |
| `file_getattr` | 468 | 468 | `reserved` | `No Call` invalid | Linux:latent | table | table |  |
| `file_setattr` | 469 | 469 | `reserved` | `__getpath_fromfd` notimp | Linux:latent | table | table |  |
| `listns` | 470 | 470 | `reserved` | `__getpath_fromaddr` notimp | Linux:latent | table | table |  |
| `pread64` | 475 | 180 | `pread` | `pread` valid | FreeBSD | table | table | yes |
| `pwrite64` | 476 | 181 | `pwrite` | `pwrite` valid | FreeBSD | table | table | yes |
| `mmap2` | 477 | 192 | `mmap` | `mmap2` valid | FreeBSD | MD | MD | yes |
| `lseek` | 478 | 19 | `lseek` | `lseek` valid | FreeBSD | table | table | yes |
| `truncate` | 479 | 92 | `truncate` | `truncate` notimp | FreeBSD | table | table | yes |
| `truncate64` | 479 | 193 | `truncate` | `truncate` notimp | FreeBSD | table | table | yes |
| `ftruncate` | 480 | 93 | `ftruncate` | `ftruncate` valid | FreeBSD | table | table | yes |
| `ftruncate64` | 480 | 194 | `ftruncate` | `ftruncate` valid | FreeBSD | table | table | yes |
| `faccessat` | 489 | 307 | `faccessat` | `faccessat` valid | FreeBSD | table | table | yes |
| `fchownat` | 491 | 298 | `fchownat` | `fchownat` valid | FreeBSD | table | table | yes |
| `futimesat` | 494 | 299 | `futimesat` | `futimesat` notimp | FreeBSD | table | table | yes |
| `mkdirat` | 496 | 296 | `mkdirat` | `mkdirat` notimp | FreeBSD | table | table | yes |
| `openat` | 499 | 295 | `openat` | `openat` valid | FreeBSD | MD | table | yes |
| `unlinkat` | 503 | 301 | `unlinkat` | `unlinkat` notimp | FreeBSD | table | table | yes |
| `posix_openpt` | 504 | — | `posix_openpt` | `posix_openpt` valid | FreeBSD | table | table | yes |
| `fadvise64_64` | 531 | 272 | `posix_fadvise` | `posix_fadvise` valid | FreeBSD | table | table | yes |
| `accept4` | 541 | 364 | `accept4` | `accept4` valid | FreeBSD | table | table | yes |
| `pipe2` | 542 | 331 | `pipe2` | `pipe2` valid | FreeBSD | table | table | yes |
| `utimensat` | 547 | 320 | `utimensat` | `utimensat` valid | FreeBSD | table | table | yes |
| `fdatasync` | 550 | 148 | `fdatasync` | `fdatasync` notimp | FreeBSD | table | table | yes |
| `mknodat` | 559 | 297 | `mknodat` | `mknodat` notimp | FreeBSD | table | table | yes |
| `getrandom` | 563 | 355 | `getrandom` | `getrandom` valid | FreeBSD | table | table | yes |
| `membarrier` | 584 | 375 | `membarrier` | `membarrier` valid | FreeBSD | table | table | yes |
| `prlimit64` | 1024 | 340 | `beyond FreeBSD` | — | private | MD | MD | yes |
| `set_thread_area` | 0x8000\|63 | 243 |  |  | native | native | native |  |
| `futex` | 0x8000\|64 | 240 |  |  | native | native | native | yes |
| `exit_group` | 0x8000\|65 | 252 |  |  | native | native | native | yes |
| `timer_settime32` | 0x40000\|1 | 260 |  |  | stub | libc | libc |  |
| `timer_gettime32` | 0x40000\|2 | 261 |  |  | stub | libc | libc |  |
| `clock_settime32` | 0x40000\|5 | 264 |  |  | stub | libc | libc |  |
| `clock_getres_time32` | 0x40000\|7 | 266 |  |  | stub | libc | libc |  |
| `clock_nanosleep_time32` | 0x40000\|8 | 267 |  |  | stub | libc | libc |  |
| `times` | 0x40000\|43 | 43 |  |  | stub | libc | libc |  |
| `sethostname` | 0x40000\|74 | 74 |  |  | stub | libc | libc |  |
| `reboot` | 0x40000\|88 | 88 |  |  | stub | libc | libc |  |
| `sysinfo` | 0x40000\|116 | 116 |  |  | stub | libc | libc |  |
| `ipc` | 0x40000\|117 | 117 |  |  | stub | libc | libc | yes |
| `setdomainname` | 0x40000\|121 | 121 |  |  | stub | libc | libc |  |
| `adjtimex` | 0x40000\|124 | 124 |  |  | stub | libc | libc |  |
| `init_module` | 0x40000\|128 | 128 |  |  | stub | libc | libc |  |
| `delete_module` | 0x40000\|129 | 129 |  |  | stub | libc | libc |  |
| `personality` | 0x40000\|136 | 136 |  |  | stub | libc | libc |  |
| `mremap` | 0x40000\|163 | 163 |  |  | stub | libc | libc |  |
| `setxattr` | 0x40000\|226 | 226 |  |  | stub | libc | libc |  |
| `lsetxattr` | 0x40000\|227 | 227 |  |  | stub | libc | libc |  |
| `fsetxattr` | 0x40000\|228 | 228 |  |  | stub | libc | libc |  |
| `getxattr` | 0x40000\|229 | 229 |  |  | stub | libc | libc |  |
| `lgetxattr` | 0x40000\|230 | 230 |  |  | stub | libc | libc |  |
| `fgetxattr` | 0x40000\|231 | 231 |  |  | stub | libc | libc |  |
| `listxattr` | 0x40000\|232 | 232 |  |  | stub | libc | libc |  |
| `llistxattr` | 0x40000\|233 | 233 |  |  | stub | libc | libc |  |
| `flistxattr` | 0x40000\|234 | 234 |  |  | stub | libc | libc |  |
| `removexattr` | 0x40000\|235 | 235 |  |  | stub | libc | libc |  |
| `lremovexattr` | 0x40000\|236 | 236 |  |  | stub | libc | libc |  |
| `fremovexattr` | 0x40000\|237 | 237 |  |  | stub | libc | libc |  |
| `sched_setaffinity` | 0x40000\|241 | 241 |  |  | stub | libc | libc |  |
| `sched_getaffinity` | 0x40000\|242 | 242 |  |  | stub | libc | libc |  |
| `epoll_create` | 0x40000\|254 | 254 |  |  | stub | libc | libc |  |
| `epoll_ctl` | 0x40000\|255 | 255 |  |  | stub | libc | libc |  |
| `epoll_wait` | 0x40000\|256 | 256 |  |  | stub | libc | libc | yes |
| `epoll_pwait` | 0x40000\|319 | 319 |  |  | stub | libc | libc | yes |
| `timerfd_create` | 0x40000\|322 | 322 |  |  | stub | libc | libc |  |
| `timerfd_settime32` | 0x40000\|325 | 325 |  |  | stub | libc | libc |  |
| `timerfd_gettime32` | 0x40000\|326 | 326 |  |  | stub | libc | libc |  |
| `epoll_create1` | 0x40000\|329 | 329 |  |  | stub | libc | libc |  |
| `clock_adjtime` | 0x40000\|343 | 343 |  |  | stub | libc | libc |  |
| `finit_module` | 0x40000\|350 | 350 |  |  | stub | libc | libc |  |
| `preadv2` | 0x40000\|378 | 378 |  |  | stub | libc | libc |  |
| `pwritev2` | 0x40000\|379 | 379 |  |  | stub | libc | libc | yes |
| `semget` | 0x40000\|393 | 393 |  |  | stub | libc | libc |  |
| `semctl` | 0x40000\|394 | 394 |  |  | stub | libc | libc |  |
| `shmget` | 0x40000\|395 | 395 |  |  | stub | libc | libc |  |
| `shmctl` | 0x40000\|396 | 396 |  |  | stub | libc | libc |  |
| `shmat` | 0x40000\|397 | 397 |  |  | stub | libc | libc |  |
| `shmdt` | 0x40000\|398 | 398 |  |  | stub | libc | libc |  |
| `msgget` | 0x40000\|399 | 399 |  |  | stub | libc | libc |  |
| `msgsnd` | 0x40000\|400 | 400 |  |  | stub | libc | libc |  |
| `msgrcv` | 0x40000\|401 | 401 |  |  | stub | libc | libc |  |
| `msgctl` | 0x40000\|402 | 402 |  |  | stub | libc | libc |  |
| `clock_settime64` | 0x40000\|404 | 404 |  |  | stub | libc | libc |  |
| `clock_adjtime64` | 0x40000\|405 | 405 |  |  | stub | libc | libc |  |
| `clock_getres_time64` | 0x40000\|406 | 406 |  |  | stub | libc | libc |  |
| `timer_gettime64` | 0x40000\|408 | 408 |  |  | stub | libc | libc |  |
| `timer_settime64` | 0x40000\|409 | 409 |  |  | stub | libc | libc |  |
| `timerfd_gettime64` | 0x40000\|410 | 410 |  |  | stub | libc | libc |  |
| `timerfd_settime64` | 0x40000\|411 | 411 |  |  | stub | libc | libc |  |
| `utimensat_time64` | 0x40000\|412 | 412 |  |  | stub | libc | libc |  |
| `pselect6_time64` | 0x40000\|413 | 413 |  |  | stub | libc | libc | yes |
| `ppoll_time64` | 0x40000\|414 | 414 |  |  | stub | libc | libc | yes |
| `recvmmsg_time64` | 0x40000\|417 | 417 |  |  | stub | libc | libc | yes |
| `mq_timedsend_time64` | 0x40000\|418 | 418 |  |  | stub | libc | libc | yes |
| `mq_timedreceive_time64` | 0x40000\|419 | 419 |  |  | stub | libc | libc | yes |
| `semtimedop_time64` | 0x40000\|420 | 420 |  |  | stub | libc | libc |  |
| `rt_sigtimedwait_time64` | 0x40000\|421 | 421 |  |  | stub | libc | libc | yes |
| `futex_time64` | 0x40000\|422 | 422 |  |  | stub | libc | libc | yes |
| `sched_rr_get_interval_time64` | 0x40000\|423 | 423 |  |  | stub | libc | libc |  |
| `epoll_pwait2` | 0x40000\|441 | 441 |  |  | stub | libc | libc |  |
| `faccessat2` | 0x40000\|1025 | 439 |  |  | stub | libc | libc |  |
