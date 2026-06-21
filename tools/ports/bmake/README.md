# Port: bmake (NetBSD portable make)

The build system uBixOS itself uses. Self-hosting needs it running on-device
(the in-tree `bin/make` is a 558-line toy that can't parse the tree's BSD
Makefiles). Pulled in as a **port** (pinned fetch + SHA + patches) per
`docs/design/third-party-ports-plan.md` — the **first** port, and the proving
ground for `mk/ports.mk`.

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

## Status

| Step | State |
|---|---|
| Fetch pinned tarball + verify SHA-256 | ✅ via `mk/ports.mk` |
| Extract → `build/ports/bmake-20240212/` (gitignored) | ✅ |
| Apply `patches/*.patch` | ✅ (none needed yet) |
| `sys/cdefs.h` shim (musl ships none; make.h needs it) | ✅ `shim/sys/cdefs.h` |
| **`config.h` for musl** | ⬜ **the remaining crux** |
| Per-file cross-compile + link against musl | ⬜ |
| In-OS run (`bmake` parses a real tree Makefile) | ⬜ (also gated on a stable kernel) |

## The remaining work (the cross-build)

bmake is autoconf-based. Normally `configure` detects the host's features and
writes `config.h`, which then selects which of bmake's bundled compat shims
(`getopt.c`, `realpath.c`, `setenv.c`, `strlcpy.c`, `sigaction.c`, `dirname.c`,
`stresep.c`, …) to compile — musl provides most of them, so only a subset is
built.

We **cross-compile** against musl-freestanding (`-nostdlib -nostdinc`), so
`configure` can't link/run its target test-programs. The standard fix for porting
an autoconf program to an embedded/freestanding target is to **author `config.h`
by hand** from `config.h.in` + the known musl feature set, then build with the
uBixOS world toolchain. That `config.h` (+ the per-file compile iteration it
drives via `build.mk`) is the next focused step. Feasibility is validated: with
the shim + the world musl include flags, the core sources compile past the
header-setup stage.

When `config.h` lands here, `do-build` picks it up automatically and links
`build/${TARGET}/bin/bmake`.
