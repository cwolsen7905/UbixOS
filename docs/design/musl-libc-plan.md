# UbixOS musl libc + libc++ Migration Plan

## Goal

Replace the piecemeal FreeBSD-derived `lib/libc/` with musl libc (MIT
licensed), then layer LLVM libc++ on top for a full C++ standard library.
The result is a tested, maintained, multi-architecture libc that supports
future ports to ARM, AArch64, x86_64, and RISC-V with only a syscall-layer
change per new architecture.

---

## Why musl

| Property | Current libc | musl |
|----------|-------------|------|
| License | BSD (FreeBSD-derived) | MIT |
| Completeness | Partial — missing functions discovered per-app | Full POSIX |
| Architecture support | i386 only | i386, x86_64, ARM, AArch64, RISC-V, MIPS, PPC … |
| Platform-specific code | Scattered across all source files | Isolated to `arch/<arch>/` only |
| Update path | Manual per-function | `git subtree pull` from tagged release |
| Used by hobby OSes | No | Sortix, Managarm, SerenityOS (early), many others |

---

## Repository Integration — Git Subtree

musl lives in `contrib/musl/` as a git subtree, not a submodule. This means:
- The full source is in the repo, no `git submodule update` needed
- Normal files to every developer
- Upstream updates are a single command
- UbixOS-specific files sit inside the tree alongside musl source

### Add (one-time)

```sh
git subtree add \
  --prefix=contrib/musl \
  https://git.musl-libc.org/git/musl \
  v1.2.6 --squash
```

### Update to a new release

```sh
git subtree pull \
  --prefix=contrib/musl \
  https://git.musl-libc.org/git/musl \
  v1.2.7 --squash
```

Git merges the upstream diff. UbixOS-specific files in `arch/i386/` that
don't overlap with musl's own changes survive automatically. Conflicts are
rare because all UbixOS changes are isolated to the syscall layer.

---

## What UbixOS Changes in musl

musl's architecture is explicitly designed so that **all platform-specific
code lives in `arch/<arch>/`**. The rest — stdio, stdlib, math, string,
locale, all ~30K lines — is pure portable C that you never touch.

| File | What UbixOS changes |
|------|---------------------|
| `arch/i386/syscall_arch.h` | Remap syscall numbers FreeBSD→Linux mapping (see table below); keep `int $0x80` |
| `arch/i386/bits/syscall.h.in` | UbixOS syscall number table |
| `src/env/__init_tls.c` | Stub TLS init until UbixOS supports `set_thread_area` |
| `src/thread/i386/__set_thread_area.s` | Stub or implement |

Everything else: untouched.

---

## Syscall Number Mapping

UbixOS POSIX syscalls use `int $0x80` with **FreeBSD i386 ABI** numbers.
musl's i386 target expects **Linux i386** numbers. The mapping lives in
`arch/i386/bits/syscall.h.in` — one `#define` per syscall.

### Core syscalls — implemented and mappable

| Syscall | UbixOS # (FreeBSD) | Linux # | Notes |
|---------|--------------------|---------|-------|
| `exit` | 0 | 1 | |
| `fork` | 1 | 2 | |
| `read` | 2 | 3 | |
| `write` | 3 | 4 | |
| `open` | 4 | 5 | |
| `close` | 5 | 6 | |
| `wait4` | 6 | 114 | |
| `unlink` | 9 | 10 | |
| `chdir` | 11 | 12 | |
| `fchdir` | 12 | 133 | |
| `getpid` | 20 | 20 | Same number — lucky |
| `setuid` | 23 | 213 | Linux uses setuid32 |
| `getuid` | 24 | 199 | Linux uses getuid32 |
| `geteuid` | 25 | 201 | Linux uses geteuid32 |
| `access` | 33 | 33 | Same number |
| `getppid` | 39 | 64 | |
| `getegid` | 43 | 202 | Linux uses getegid32 |
| `getgid` | 47 | 200 | Linux uses getgid32 |
| `ioctl` | 54 | 54 | Same number |
| `readlink` | 58 | 85 | |
| `execve` | 59 | 11 | |
| `munmap` | 72 | 91 | |
| `getpgrp` | 81 | 65 | |
| `setpgid` | 82 | 57 | |
| `dup2` | 90 | 63 | |
| `fcntl` | 92 | 221 | Linux uses fcntl64 |
| `select` | 93 | 142 | Linux uses _newselect |
| `gettimeofday` | 116 | 78 | |
| `rename` | 128 | 38 | |
| `setgid` | 181 | 214 | Linux uses setgid32 |
| `stat` | 188 | 195 | Linux uses stat64 |
| `fstat` | 189 | 197 | Linux uses fstat64 |
| `lstat` | 190 | 196 | Linux uses lstat64 |
| `getrlimit` | 194 | 191 | Linux uses ugetrlimit |
| `setrlimit` | 195 | 75 | |
| `__sysctl` | 202 | 149 | Linux has _sysctl (deprecated) |
| `__getcwd` | 326 | 183 | |
| `sched_yield` | 331 | 158 | |
| `sigprocmask` | 340 | 175 | Linux uses rt_sigprocmask |
| `statfs` | 396 | 99 | |
| `fstatfs` | 397 | 100 | |
| `sigaction` | 416 | 174 | Linux uses rt_sigaction |
| `pread` | 475 | 180 | Linux uses pread64 |
| `mmap` | 477 | 192 | Linux uses mmap2 |
| `fstatat` | 493 | 300 | Linux uses fstatat64 |
| `openat` | 499 | 295 | |
| `pipe2` | 542 | 331 | |

### Syscalls musl needs that UbixOS must add

| Syscall | Linux # | Why musl needs it | Priority |
|---------|---------|-------------------|----------|
| `brk` | 45 | Heap growth (musl uses mmap2 preferentially, but brk is fallback) | Medium — UbixOS has `obreak` at FreeBSD #17, needs mapping |
| `mprotect` | 125 | musl uses it to protect allocator metadata | High |
| `exit_group` | 252 | musl calls this on exit; can alias to UbixOS `exit` (0) | Easy |
| `set_thread_area` | 243 | TLS setup; stub to return 0 for single-threaded | Easy stub |
| `clock_gettime` | 265 | POSIX clock; map to `gettimeofday` or implement | Medium |
| `getdents64` | 220 | Directory iteration; UbixOS has `getdirentries` (196) | Medium |
| `lseek` | 19 | File seeking; not in current table, needs adding | High |
| `pipe` | 42 | Basic IPC; not in current table | Medium |
| `futex` | 240 | Threading primitive; stub for single-threaded use | Easy stub |
| `kill` | 37 | Signal sending | Medium |
| `mkdir` | 39 | Create directory | Medium |
| `rmdir` | 40 | Remove directory | Medium |
| `dup` | 41 | Duplicate fd | Medium |

### UbixOS-specific calls — no musl equivalent

These are UbixOS native calls (`int $0x81`) or VFS extensions that musl
never calls. They remain unchanged.

| Call | Vector | Purpose |
|------|--------|---------|
| `mpiCreateMbox` / `mpiPostMessage` etc. | `int $0x81` | MPI IPC system |
| `sys_mapfb` (43) | `int $0x81` | Map framebuffer |
| `sys_getmouse` (44) | `int $0x81` | Mouse events |
| `sys_shareregion` (45) | `int $0x81` | VMM region sharing |
| `sys_getkbd` (46) | `int $0x81` | Keyboard events |
| `fopen`/`fread`/`fclose` etc. (294–301) | `int $0x80` | UbixOS VFS (to be retired once musl stdio takes over) |

---

## Include Path Layout

### During migration (both libcs coexist)

```
-I../../contrib/musl/include        ← musl public headers
-I../../contrib/musl/arch/i386/bits ← arch-specific types
-I../../include                     ← UbixOS-specific headers
```

UbixOS-specific headers that musl never provides — `sys/mpi.h`,
`fb/fb.h`, `views/display_proto.h`, `api/ubix.h` — stay in `include/`
permanently.

### After full migration

musl owns all of `stdio.h`, `stdlib.h`, `string.h`, `math.h`, `unistd.h`,
`fcntl.h`, `sys/stat.h` etc. The corresponding files in `include/` are
deleted as each is verified working through musl.

---

## Phases

### Phase 0 — Syscall groundwork
**Done when:** The missing high-priority syscalls (mprotect, exit_group, lseek,
set_thread_area stub, brk/obreak mapping) are implemented in the kernel.

- Add `lseek` to `int $0x80` table
- Add `mprotect` to `int $0x80` table
- Alias `exit_group` to `sys_exit`
- Implement `set_thread_area` (slot 351): writes LDT[1] with the TLS base address from the `user_desc`, loads `%gs = 0xF` (LDT entry 1, ring 3). Persists to userland through `iret`. Returns `entry_number = 1`.
- Stub `futex` (slot 350): return 0 — single-threaded musl never blocks on a futex
- Map `brk` → `obreak` (slot 17): returns current break on brk(0); Linux ABI compatible

**Unblocks:** Phase 1. Without these, musl fails to initialise at all.

---

### Phase 1 — musl in the tree, builds for i386 ✅ Done
**Done when:** `contrib/musl/` builds a static `libc.a` for i386 UbixOS.
No apps use it yet.

- ✅ `git subtree add` musl v1.2.6 (`contrib/musl/`)
- ✅ Write `arch/i386/bits/syscall.h.in` — Linux→UbixOS (FreeBSD slot) number remapping via `#undef`/`#define` overrides at end of file; sed step in musl Makefile generates matching `SYS_*` aliases
- ✅ Replace `arch/i386/syscall_arch.h` — declares `__syscall0`..`__syscall6` extern; `SYSCALL_NO_TLS=1` forces `int $0x80` path; no register ABI
- ✅ Write `src/thread/i386/ubixos_syscall.S` — `__syscall0`..`__syscall6` for FreeBSD stack ABI (`pushl args; pushl $0; int $0x80; addl $N; jnc/negl`)
- ✅ Replace `src/thread/i386/__set_thread_area.s` — FreeBSD ABI call to slot 351; kernel sets `%gs=0xF`, no selector reload needed
- ✅ Replace `src/thread/i386/syscall_cp.s` — cancellable syscall with FreeBSD frame; `__cp_begin`/`__cp_end` mark cancellation window
- ✅ Add `contrib/musl/Makefile.ubixos` wrapper (`bmake -f contrib/musl/Makefile.ubixos`)
- ✅ `build/lib/musl.a` — 1.9 MB static archive, all UbixOS shim symbols present
- Note: `entry_number*8+7` fix in `__init_tls.c` not needed — kernel already sets `%gs=0xF` before returning from `set_thread_area`; musl's selector formula is never executed for the TLS load path we use

**Unblocks:** Phase 2.

---

### Phase 2 — First app on musl
**Done when:** One simple app (`clock` or `stat`) runs under musl, verified
in QEMU.

- Point that app's Makefile at `musl.so` instead of `libc.so`
- `-I contrib/musl/include` replaces `-I include` for that app
- Fix any symbol gaps discovered
- Run `bmake image run`, verify the app works

**Unblocks:** Confidence to migrate more apps.

---

### Phase 3 — App-by-app migration
**Done when:** All dynamically-linked apps use musl. `libc.so` (old) is only
used by `login` and `shell` (which remain statically linked for now).

Migration order (simplest first):
1. ✅ `format`, `clock`, `cp`, `ls`, `cat`, `uname`, `syscheck`, `make`, `filetest`, `ed`, `ttytest` — stdio + basic syscalls
2. ✅ `disklabel`, `fdisk` — full stdio; fixed `gets()→fgets()` and missing `stdint.h`
3. ✅ `ttyd`, `ubistry` — MPI + terminal; extended `ubix_api.a` with `mpi_destroyMbox` + `gettime`
4. ✅ `init`, `login`, `shell` — boot chain; fixed fread item-count bug in login, added fflush calls throughout
5. ✅ `tcc` — custom Makefile; dropped old-libc shims; expanded `moddi3.c` with full 64-bit division helpers to avoid host libgcc (elf64, can't link into i386 binary)
6. ✅ `term` — C++/objgfx; already updated to clean `ubix.prog.mk` form
7. 🔄 `views`, `taskbar` — C++/objgfx; remaining old-libc binaries in active build
8. ⬜ Orphaned (`kill`, `printf`, `test`, `edit`, `mount`) — triage: port, rewrite, or remove before Phase 4
9. Removed: `sh` (superseded by `shell`), `muffin` (orphaned, kept as reference), `stat` (to be rewritten)

As each app migrates, audit which headers it needed from `include/`. Once
nothing uses a header from `include/` that musl provides, delete it.

---

### Phase 4 — Retire lib/libc/
**Done when:** `lib/libc/` is removed from the build. musl is the sole libc.

- Remove `lib/libc/` from `lib/Makefile`
- Delete `lib/libc/` directory
- Remove remaining libc headers from `include/` that musl now owns
- `include/` contains only UbixOS-specific headers at this point

---

### Phase 5 — libc++ (C++ standard library)
**Done when:** `<string>`, `<vector>`, `<algorithm>` etc. work in userland C++.

Uses LLVM libc++ (Apache 2.0) + libc++abi (replaces `lib/libcpp/`).

- `git subtree add` libc++abi into `contrib/libcxxabi/`
  - Provides all `__cxa_*` ABI symbols, RTTI, exception infrastructure
  - Direct replacement for `lib/libcpp/libcpp.o`
- `git subtree add` libc++ into `contrib/libcxx/`
  - Provides `<string>`, `<vector>`, `<algorithm>`, `<memory>` etc.
  - Depends on libc++abi + musl
- Include path gains a third layer:
  ```
  -I../../contrib/libcxx/include      ← <string>, <vector> etc.
  -I../../contrib/musl/include        ← <stdio.h>, <stdlib.h> etc.
  -I../../include                     ← UbixOS-specific
  ```
- Retire `lib/libcpp/` once libc++abi is wired in

SerenityOS, Managarm, and others have done this exact stack. It is the
standard path for hobby OSes that want real C++ without GPL entanglement.

---

### Phase 6 — New architecture port (ARM / x86_64)
**Done when:** UbixOS boots on a second architecture.

With musl the new work is:
1. Write `arch/<newarch>/syscall_arch.h` — the new syscall convention
2. Write `arch/<newarch>/bits/syscall.h.in` — syscall number table
3. Kernel: implement the new arch's exception/syscall entry path

All of libc, libcpp, libc++, and userland apps recompile untouched.
The payoff of the entire migration effort.

---

### Post-Phase 6 — Kernel cleanup
Once a second architecture exists, arch-specific syscall code in `sys/kernel/gen_calls.c`
should be split out:

- Move `sys_set_thread_area` (and any future TLS/GDT helpers) from `gen_calls.c`
  into `sys/arch/i386/tls.c` with a header at `sys/include/machine/tls.h`.
  The i386 implementation uses LDT[1] and the `0xF` selector — none of that
  belongs in a generic file once a second arch has its own equivalent.
- Repeat for any other i386-specific calls that accumulate in `gen_calls.c`
  during Phases 1–5.

---

## What Stays UbixOS-Specific Forever

These are never replaced by musl or libc++:

| Header / Library | Why |
|-----------------|-----|
| `include/sys/mpi.h` | UbixOS IPC — no POSIX equivalent |
| `include/fb/fb.h` | Framebuffer API — UbixOS-specific |
| `include/views/display_proto.h` | Views compositor protocol |
| `include/api/ubix.h` | Native UbixOS API (`ubix_getcwd` etc.) |
| `lib/ubix/` | Startup stub (`_start` → `main`) |
| `lib/ubix_api/` | Native API implementation |
| `lib/libfb/` | Framebuffer drawing library |

---

## Notes

- The UbixOS VFS extension syscalls (294–301: `fopen`, `fread`, `fclose` etc.)
  are retired once musl's stdio layer works correctly via POSIX fds. musl
  stdio never calls these — it uses `open`/`read`/`write`/`close` only.
- `lib/objgfx` currently includes kernel headers (`<lib/kprintf.h>`,
  `<vfs/file.h>`). These must be cleaned up before objgfx can be built
  as a pure userland library against musl. Not on the critical path.
- Dynamic linking infrastructure (`libexec/ld.so`) is already working.
  musl ships its own dynamic linker (`musl-libc/lib/ld-musl-i386.so.1`)
  which should eventually replace the existing `libexec/ld.so`.

---

## Status

Last updated: 2026-05-14

| Phase | Description | Status | Notes |
|-------|-------------|--------|-------|
| Phase 0 | Syscall groundwork | ✅ Done | obreak@17, kill@37, mprotect@74, clock_gettime@232, futex@350, set_thread_area@351, exit_group@352, getdents@272 wired |
| Phase 1 | musl in tree, builds for i386 | ✅ Done | `contrib/musl/` builds `build/lib/musl.a`; FreeBSD ABI syscall layer in place; TIOCGWINSZ fixed via `arch/i386/bits/ioctl.h` |
| Phase 2 | First app on musl | ✅ Done | `clock` was first; full boot chain (init→login→shell) verified in QEMU |
| Phase 3 | App-by-app migration | 🔄 In progress | All binaries migrated except `views` and `taskbar`; `muffin` orphaned from build; `sh` removed; `tcc` migrated with self-contained 64-bit division shim; `stat` removed from build (to be rewritten later) |
| Phase 4 | Retire lib/libc/ | ⬜ Not started | Blocked on views + taskbar migration; orphaned bins (kill, printf, test, edit, mount) need triage |
| Phase 5 | libc++ (C++ standard library) | ⬜ Not started | Blocked on Phase 4 |
| Phase 6 | New architecture port | ⬜ Not started | |

### Legend
- ✅ Done
- 🔄 In progress
- ⬜ Not started
- ❌ Blocked
