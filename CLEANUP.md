# UbixOS lib/ Cleanup Audit

The active `bmake world` build target builds: init, login, shell, clock, cp, fdisk,
format, disklabel, ubistry, ttyd, stat, ls.  Everything below is measured against
that set.

## lib/ directory verdict

| Library        | Verdict       | Reason |
|----------------|---------------|--------|
| `libc`         | Keep          | Linked by every active binary |
| `ubix`         | Keep          | Startup/crt1 linked by every active binary |
| `libcpp`       | Keep          | Kernel C++ support (in lib/Makefile all: target) |
| `ubix_api`     | Remove        | Only in a commented-out shell LIBRARIES line and an old tools/Makefile copy rule; not in any active build |
| `csu`          | Remove        | Commented out in lib/Makefile (#csu-code); only referenced by bin/sh which is not in the active build |
| `libedit`      | Remove        | Only in bin/sh/Makefile.depend; bin/sh is not in the active build |
| `libmd`        | Remove        | Zero references in the active build |
| `libstdc++`    | Remove        | Only bin/launcher links it; launcher is not in the active build |
| `msun`         | Remove        | Only in libexec/rtld-elf/tests dependency file; tests are not built |
| `objgfx`       | Remove        | Only referenced by bin/muffin and bin/launcher; both commented out of all: |
| `objgfx40`     | Remove        | Same as objgfx |
| `views`        | Remove        | Only referenced by bin/launcher |
| `libc_nonshared` | ~~Remove~~ Done | Zero references in the active build |
| `libc_old`     | Remove        | Zero references after the libc_old → libc Makefile refactor |

## Summary

10 directories are safe to remove: `ubix_api`, `csu`, `libedit`, `libmd`,
`libstdc++`, `msun`, `objgfx`, `objgfx40`, `views`, `libc_nonshared`, `libc_old`

Removal steps (when ready):
1. `git rm -r lib/ubix_api lib/csu lib/libedit lib/libmd "lib/libstdc++" lib/msun lib/objgfx lib/objgfx40 lib/views lib/libc_nonshared lib/libc_old`
2. Remove the dead targets from `lib/Makefile` (ubix_api-code, csu-code, views, libstdc++, objgfx-code)
3. Remove any matching entries from `lib/Makefile.incl` if present
4. `bmake world` to confirm nothing breaks
