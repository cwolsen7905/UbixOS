# Port: bmake (NetBSD portable make)

The build system uBixOS itself uses. Self-hosting needs it running on-device
(the in-tree `bin/make` is a 558-line toy that can't parse the tree's BSD
Makefiles). Pulled in as a **port** (pinned fetch + SHA + patches) per
`docs/design/third-party-ports-plan.md` — the **first** port, and the proving
ground for `share/mk/ports.mk`.

- **Upstream:** Simon Gerraty's portable bmake, `bmake-20240212`
  (`https://www.crufty.net/ftp/pub/sjg/`).
- **License:** 2-/3-clause BSD + Berkeley / Adam de Boor / NetBSD Foundation
  copyrights (see `LICENSE` in the tarball) — compatible with uBixOS's BSD
  license; this is *why* bmake over GNU make. The bundled GNU `configure` is GPL
  but is a host build-tool, never part of the shipped binary.

## Build

```
bmake -C tools/ports/bmake            # default TARGET=aarch64
bmake -C tools/ports/bmake TARGET=x86_64
```

## Status — WORKING

| Step | State |
|---|---|
| Fetch pinned tarball + verify SHA-256 | ✅ via `share/mk/ports.mk` |
| Extract → `build/ports/bmake-20240212/` (gitignored) | ✅ |
| `sys/cdefs.h` shim (musl ships none; make.h needs it) | ✅ `shim/sys/cdefs.h` |
| `config.h` for musl | ✅ `config.h` (configure-on-host + musl adaptation) |
| Cross-compile + link both arches → `build/${TARGET}/bin/bmake` | ✅ via `build.sh` |
| **In-OS: runs + evaluates a variable from a real makefile** | ✅ `-V V` → the value, exit 0 |

Verified on aarch64 in UbixOS over the serial console: `bmake -r -V MACHINE`
prints `aarch64` (the compiled-in machine) and `bmake -r -f <makefile> -V V`
parses a real file and prints the variable — clean exit.

### How `config.h` was produced

bmake is autoconf-based; `configure` can't run target test-programs under
`-nostdlib`, so we ran `configure` **on the host** to get a real baseline
`config.h`, then adapted the handful of musl differences (disabled
`HAVE_SYS_SIGLIST`, `HAVE_SYSCTL`, `HAVE_SYS_SYSCTL_H`, and `HAVE_MMAP` — see
below). `config.h` then drives the source set; `build.sh` compiles that set with
the uBixOS world musl flags and links like a world program (musl crt + ld.so).
`USE_META`/filemon are off (no kernel filemon support, and plain bmake is what
self-hosting needs).

## Known follow-ups (not bmake bugs)

1. **`HAVE_MMAP` disabled** — bmake `mmap`s makefiles by default; doing so on
   uBixOS **SIGABRTs** reading a real file. Disabling `HAVE_MMAP` (read() path)
   makes it work. The underlying **file-backed mmap** fault is a kernel issue —
   self-hosting-plan Phase 2.  Re-enable once that's fixed.
2. **Recipe execution needs `/bin/sh`** — bmake shells recipes out to `/bin/sh`
   (self-hosting-plan Phase 2) and the world needs `printf`/coreutils. Variable
   evaluation + parsing work today; running recipes waits on the shell.

## Build

```
bmake -C tools/ports/bmake            # default TARGET=aarch64
bmake -C tools/ports/bmake TARGET=x86_64
```
