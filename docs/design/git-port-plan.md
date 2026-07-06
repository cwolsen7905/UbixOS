# Porting Git to uBixOS

Status: **M0 complete** (2026-07-05) — git 2.39.5 cross-compiles for both
aarch64 and x86_64 and stages into the image. On-device verification and the
network milestones (M1–M3) are open.

## Why Git, and how it fits

Git is the de-facto version-control system (GPLv2). It fits uBixOS's
console-first, self-hosting direction: once `bmake world` can run on-device, a
working `git` closes the loop for developing uBixOS *on* uBixOS. Git is pulled
in as a **port** (`tools/ports/git/`), pinned + patched, per
`docs/design/third-party-ports-plan.md` — **not** vendored into `contrib/`.

The one hard dependency, **zlib**, is already in the tree (`lib/zlib`, builds
`libz.so` for every arch). Git's other classic dependencies (curl, OpenSSL,
expat, gettext, iconv, pcre, tcl/tk, perl, python) are all *optional* and
dropped for M0.

## The core strategy: register uBixOS as a new git platform

Git's build has two forms of host-vs-target confusion when cross-compiling from
macOS against our musl world:

1. It autodetects the **build** host (`uname -s` → `Darwin`) and applies
   Darwin-specific quirks that are wrong for the musl target.
2. Its `config.mak.uname` `Linux` block assumes **glibc** (`NEEDS_LIBRT`,
   `HAVE_SYNC_FILE_RANGE`, `/proc/self/exe` procinfo, …). Many of those are
   `ifdef`-guarded, so you cannot un-set them from the command line once the
   block defines them.

Rather than fight either, we tell git uBixOS is a **brand-new platform**:
`make uname_S=UbixOS`. `config.mak.uname` has no `ifeq ($(uname_S),UbixOS)`
block, so git applies **only** our `config.mak` — an explicit, honest
description of what the musl world provides. This is git's documented path for
adding a platform, and it doubles as a clean teaching example (see
`docs/apps/porting-to-ubixos.md`).

## Files

```
tools/ports/git/
  Makefile      port recipe (pin, cross vars, do-build, do-install+strip)
  config.mak    the uBixOS platform definition (feature flags)
  build.sh      generates the CC wrapper, drives git's own Makefile
  patches/      (empty for M0 — no source patches needed)
```

`build/${TARGET}/usr/bin/git` is the product; `tools/mkimage.sh` copies
`usr/bin/*` verbatim, so `bmake image` includes it with no extra wiring.

## The CC wrapper (the only real toolchain trick)

Our `*-elf-gcc` cross compilers default to their bare-metal libc, so every
compile needs `-nostdinc` + the musl include set and every **link** needs
`-nostdlib` + musl crt objects + libc + libgcc + the dynamic linker, threaded in
glibc-crt order (`Scrt1 crti … -lc libgcc crtn`). Git's link recipe
(`$(CC) $(LDFLAGS) <objs> $(LIBS)`) has **no post-`$(LIBS)` hook** to append
`crtn`, so `LDFLAGS` alone can't do it.

`build.sh` therefore generates a wrapper used as `CC`. It classifies each
invocation (`-c`/`-E`/`-S` ⇒ compile, else ⇒ link) and injects the right flags
around git's own arguments — the same link line the world build uses
(`share/mk/ubix.musl.vars.mk`). Compile invocations put git's `-I` args **first**
so a bundled header (e.g. `compat/regex/regex.h`) shadows musl's; the musl
includes come last, as the toolchain fallback.

## M0 feature set (`config.mak`)

Dropped for M0: `NO_CURL NO_OPENSSL NO_EXPAT NO_GETTEXT NO_ICONV NO_TCLTK
NO_PERL NO_PYTHON NO_GITWEB NO_PTHREADS NO_INSTALL_HARDLINKS`. Declared musl
capabilities: `HAVE_ALLOCA_H HAVE_PATHS_H HAVE_DEV_TTY HAVE_GETDELIM
NO_STRLCPY NO_REGEX=NeedsStartEnd FREAD_READS_DIRECTORIES`. SHA is git's bundled
block-SHA1 + built-in SHA-256 (no libcrypto). Editor/pager default to `ed`/`cat`
for a console-first box with no `$EDITOR`/`$PAGER`.

This yields the full **local** workflow — `init add commit status log diff show
branch checkout merge reset rebase(non-interactive)` — with no network.

## Gotchas hit during M0 (all resolved)

| Symptom | Cause | Fix |
|---|---|---|
| `expected '{' before 'thread_local'` in `index-pack.c` | gcc 16 defaults to gnu23, where `thread_local` is a keyword; git 2.39 uses `struct thread_local` | `CFLAGS += -std=gnu11` |
| `#error "Git requires REG_STARTEND support"` | musl's `<regex.h>` (lacking `REG_STARTEND`) was found before git's bundled `compat/regex` | wrapper puts git's `-I` **before** musl includes; `NO_REGEX=NeedsStartEnd` |
| `R_AARCH64_ADR_PREL_PG_HI21 against 'stderr' … recompile with -fPIC` | linking `-pie` but objects weren't position-independent | `CFLAGS += -fPIC` |
| Binary at 3.9 MB near the ~4 MB `EXEC_MAX` loader cap | unstripped symbol tables | `do-install` runs `${CROSS_PREFIX}strip` (→ 2.98 MB aarch64 / 3.59 MB x86_64) |
| Nothing staged after `bmake` | `all: build` does **not** run `do-install` | stage with `bmake install` (`install: build do-install`) |

## Build & stage

```sh
bmake -C tools/ports/git install TARGET=aarch64   # -> build/aarch64/usr/bin/git
bmake -C tools/ports/git install TARGET=x86_64    # -> build/x86_64/usr/bin/git
bmake image            # aarch64 image now contains /usr/bin/git
```

The port worktree (`build/ports/git-2.39.5/`) is shared across arches, so the two
arches must build **serially** (`build.sh` runs `make clean` first). This also
means every invocation is a full rebuild (~2 min).

## Roadmap

- **M1 — make it run on-device.** Iterate under QEMU: author identity
  (`getpwuid`/`$HOME`/`user.name` config), commit timestamps (uBixOS wall clock),
  `core.pager=cat`, hardlink/symlink fallbacks (`core.symlinks=false`), pack
  `mmap` on the FAT/UbixFS root. This is where real syscall gaps surface.
- **M2 — remote over ssh.** Git shells out to an ssh client and pipes to
  `git-upload-pack`/`git-receive-pack`. Enable Dropbear's `dbclient` (an
  `--enable-multi` build option in the dropbear port), then
  `GIT_SSH=dbclient git clone ssh://…` — no TLS inside git.
- **M3 — remote over http(s).** Either port curl against BearSSL, or implement
  git's smart-HTTP over our `libhttp`. Deferred.

Milestones M2/M3 will need the dashed helper programs (`git-upload-pack`,
`git-receive-pack`) staged as well; M0 builds only the multi-call `git` binary.
