# oksh — uBixOS `/bin/sh`

Portable OpenBSD ksh (pdksh lineage), **public domain**, pulled in as a port per
`docs/design/third-party-ports-plan.md`.  uBixOS needs a POSIX `/bin/sh` so
`bmake` can run recipes and the world can self-host — the tree's Makefiles are
POSIX sh (`!=`, `for`/`do`, `[ ]` tests), which tcsh (the interactive login
shell) can't run.

## Build

```sh
bmake -C tools/ports/sh                 # aarch64 -> build/aarch64/bin/sh
bmake -C tools/ports/sh TARGET=x86_64   # x86_64  -> build/x86_64/bin/sh
```

Cross-built against the musl-freestanding world toolchain (like the bmake port).
oksh's own `configure` runs target test-programs that fail under `-nostdlib`, so
we author `pconfig.h` from a host `./configure` + musl adaptation and compile the
object set directly (`build.sh`).  Built `-DNO_CURSES` (no terminfo lib on
uBixOS; `/bin/sh` doesn't need line-editing terminal control).

### musl adaptations (see `pconfig.h` + `build.sh`)
- BSD-isms macOS has but musl lacks (`issetugid`, `stravis`/`strunvis`,
  `strtonum`, `sys_siglist`/`signame`) → left undefined so oksh's bundled compat
  `.c` files provide them.
- `setresuid`/`setresgid` defined (musl has them; macOS doesn't).
- `st_mtim` (native POSIX) instead of BSD `st_mtimespec`.
- Force-include `sys/cdefs.h` (the bmake shim — `__dead`), `sys/types.h`
  (`u_char`, gated behind `_GNU_SOURCE`), `stdint.h` (`int64_t`), `sys/file.h`
  (`flock`); `-DMAXLOGNAME=32`.

## Status — WORKS (standalone)

Verified in uBixOS over the serial console (aarch64): `/bin/sh -c 'echo X'`
runs and prints `X`, exit 0.  oksh's builtins (echo, test, cd, …) work.

## Known gaps (self-hosting Phase 2 — kernel/fs, not oksh bugs)

1. **Running external programs** (`/bin/sh -c /some/binary`) fails `EACCES`:
   oksh pre-checks `access(path, X_OK)` + `stat` execute bits before exec
   (exec.c:883,966).  uBixOS's FAT/stat doesn't report `0111` execute bits, so
   oksh refuses.  Builtins are unaffected (no exec).  Fix = report execute bits
   from the filesystem stat path.
2. **bmake recipe execution** is additionally blocked by a kernel
   signal-delivery-after-(COW)-fork SIGSEGV: bmake's `vfork`+`execv` of `/bin/sh`
   now works (see the `vfork.s` fix below), the child runs and exits, but the
   kernel faults delivering the resulting `SIGCHLD` to bmake's handler.

## Companion fix shipped with this port: `vfork`

musl's `vfork.s` (both arches) hardcoded the **Linux** clone syscall number
(aarch64 `220`, x86_64 `58`), which collide with FreeBSD's `__semctl`/`readlink`
under uBixOS's FreeBSD ABI — so `vfork()` (used by oksh job control and bmake's
command runner) was calling the wrong syscall.  Fixed by aliasing `vfork` to
`fork` (`b fork` / `jmp fork`): uBixOS has COW fork, so vfork's shared-address
semantics aren't needed, and this reuses the proven fork path.
