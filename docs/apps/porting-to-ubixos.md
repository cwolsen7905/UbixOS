# Porting Third-Party Software to uBixOS

This is the practical guide to bringing an existing C/C++ program (a compiler, a
shell, an SCM, a daemon) to uBixOS. It covers the **ports framework**, the
**cross-toolchain reality**, and the **gotchas** that bite every port. The
policy behind all of this — *why* we don't just copy source into `contrib/` — is
[`docs/design/third-party-ports-plan.md`](../design/third-party-ports-plan.md).

The reference ports to read alongside this doc, simplest first:

| Port | Shape | Read it for |
|------|-------|-------------|
| [`tools/ports/sh`](../../tools/ports/sh) | oksh, small, own build | the minimal recipe |
| [`tools/ports/bmake`](../../tools/ports/bmake) | bmake, `sys/cdefs.h` shim | header shims |
| [`tools/ports/git`](../../tools/ports/git) | git, drives upstream's Makefile | the **CC-wrapper** trick + new-platform strategy |
| [`tools/ports/dropbear`](../../tools/ports/dropbear) | sshd, hand-driven build | bypassing a hostile `./configure` |

---

## Mental model

A port is **not** vendored source. It is a small **recipe** that:

1. **Fetches** a pinned upstream tarball and verifies its SHA-256.
2. **Extracts** it into a gitignored cache under `build/ports/`.
3. **Patches** it with a committed, reviewable patch series (often *zero*
   patches).
4. **Cross-compiles** it against the uBixOS **musl world** toolchain.
5. **Stages** the resulting binaries into `build/${TARGET}/…` where
   `tools/mkimage.sh` picks them up verbatim.

The upstream source never enters the repo. What you commit is the recipe
(`Makefile` + `build.sh` + `config`/`patches`), typically a few hundred lines.
The framework that runs steps 1–3 is [`share/mk/ports.mk`](../../share/mk/ports.mk).

---

## Project layout

```
tools/ports/<name>/
├── Makefile      # the recipe: pin (URL/version/SHA), cross vars, do-build, do-install
├── build.sh      # the actual cross-compile (invoked by do-build)
├── config.mak    # optional: upstream build config (git uses one)
└── patches/
    └── *.patch   # optional: applied with `patch -p1`, in filename order
```

A port is built **stand-alone**, not by `bmake world`:

```sh
bmake -C tools/ports/<name>            TARGET=aarch64   # fetch→extract→patch→build
bmake -C tools/ports/<name> install    TARGET=aarch64   # …and stage into the image tree
bmake -C tools/ports/<name> install    TARGET=x86_64    # the other arch (build BOTH)
```

> `bmake` (default goal `all`) builds but does **not** stage. Staging is the
> `install` target. This trips everyone once.

---

## The recipe (`Makefile`)

Set the pin + cross settings, then `.include` the framework. The framework
provides `fetch`/`extract`/`patch`/`build`/`install`/`clean`; you provide
`do-build` and (usually) `do-install`.

```make
.MAIN: all                     # keep ports.mk's `all` the default, not do-build

PORT_NAME     = foo
PORT_VERSION  = 1.2.3
PORT_URL      = https://example.org/foo-${PORT_VERSION}.tar.gz
PORT_DISTFILE = foo-${PORT_VERSION}.tar.gz
PORT_SHA256   = <sha256 of the tarball>

TARGET      ?= aarch64
.if ${TARGET} == "x86_64"
CROSS_PREFIX = x86_64-elf-
MUSL_ARCH    = x86_64
LDEMUL       = elf_x86_64
.else
CROSS_PREFIX = aarch64-elf-
MUSL_ARCH    = aarch64
LDEMUL       = aarch64elf
.endif
CC    = ${CROSS_PREFIX}gcc
AR    = ${CROSS_PREFIX}ar
STRIP = ${CROSS_PREFIX}strip
BUILD = ${SRCTOP}/build/${TARGET}

do-build:
	@WRKSRC="${WRKSRC}" CC="${CC}" AR="${AR}" LDEMUL="${LDEMUL}" \
	    BUILD="${BUILD}" MUSL_ARCH="${MUSL_ARCH}" PORTDIR="${.CURDIR}" \
	    SRCTOP="${SRCTOP}" sh ${.CURDIR}/build.sh

do-install:
	@mkdir -p ${BUILD}/usr/bin
	@cp ${WRKSRC}/foo ${BUILD}/usr/bin/foo
	@${STRIP} ${BUILD}/usr/bin/foo

.include "${.CURDIR}/../../../share/mk/ports.mk"
```

Get `PORT_SHA256` by fetching once: `shasum -a 256 <tarball>`.

Choose the **install path** by role, following the filesystem hierarchy:
`/usr/bin` for user commands, `/usr/sbin` for daemons/services, `/sbin` for
system admin. `mkimage.sh` copies `sbin usr/bin usr/sbin usr/lib usr/tests`
verbatim, so anything you drop there ships.

---

## The cross-toolchain reality

This is where ports differ from host builds, and where time goes.

**We use `*-elf-gcc` (or clang) against musl.** The Homebrew `aarch64-elf-gcc` /
`x86_64-elf-gcc` default to a **bare-metal libc**, not musl. So you must supply
musl yourself, on **both** the compile and the link:

- **Compile:** `-nostdinc` + the musl include set + the compiler's own
  freestanding headers:
  ```
  -nostdinc -fno-builtin \
    -I${SRCTOP}/contrib/musl/include \
    -I${BUILD}/obj/musl/obj/include \
    -I${SRCTOP}/contrib/musl/arch/${MUSL_ARCH} \
    -I${SRCTOP}/contrib/musl/arch/generic
  ```
- **Link:** `-nostdlib` + musl crt objects + `-lc` + libgcc + `crtn` + the
  dynamic linker, in this order:
  ```
  -nostdlib -Wl,-m,${LDEMUL} \
    ${BUILD}/obj/musl/lib/Scrt1.o ${BUILD}/obj/musl/lib/crti.o \
    <your objects> \
    -L${BUILD}/lib -lc $(${CC} -print-libgcc-file-name) ${BUILD}/obj/musl/lib/crtn.o \
    -Wl,-dynamic-linker,/lib/ld-musl-${MUSL_ARCH}.so.1 -Wl,-rpath,/lib -pie
  ```

**All userland is PIE.** Compile with `-fPIC` and link `-pie`, or the linker
rejects position-dependent relocations (`R_AARCH64_ADR_PREL_PG_HI21 against
'stderr' … recompile with -fPIC`).

### Two build shapes

1. **Upstream has a sane Makefile you can drive** (git, bmake, oksh). Feed it
   `CC`, `AR`, `CFLAGS`, `LDFLAGS` and let it build. The wrinkle: upstream's
   link recipe rarely has a hook to append `crtn` *after* its libraries. The
   clean fix is a **CC wrapper** — a generated shell script used as `CC` that
   classifies each call (`-c`/`-E`/`-S` ⇒ compile, else ⇒ link) and injects the
   flags above around the upstream arguments. See `tools/ports/git/build.sh`.

2. **Upstream's `./configure` runs target compile+link probes that fail under a
   freestanding toolchain** (dropbear). Don't fight configure — hand-author a
   `config.h` and drive the compile/link directly in `build.sh`. See
   `tools/ports/dropbear/build.sh`.

If you can, prefer shape 1: it tracks upstream with far less code.

### Registering a new platform

Well-travelled projects (git, perl, …) key their build off `uname`. When the
build host's `uname` (macOS ⇒ `Darwin`) pulls in the wrong quirks, **declare
uBixOS as a new platform** instead of overriding a wrong one: pick a platform
name with no built-in block (`make uname_S=UbixOS`) and state the target libc's
capabilities explicitly in the port's own config. This is cleaner than
subtracting glibc-isms from a `Linux` block (many are `ifdef`-guarded and can't
be un-set from the command line). Git's port is the worked example.

---

## Libraries already in the tree

Before porting a dependency, check `contrib/` and `lib/` — a lot is already
here and builds per-arch into `build/${TARGET}/lib`:

- **zlib** (`lib/zlib` → `libz.so`) — compression.
- **BearSSL** (`lib/bearssl` → `libbearssl.so`) — TLS 1.2, no-SSE-safe.
- **libhttp** (`http_get` over BearSSL), **libpw** (PBKDF2), **libz**, **objGFX**.
- **musl** is the libc; **libc++/libc++abi** are available for C++.

Link a tree library by adding `-L${BUILD}/lib -l<name>` to your link and its
header dir to your compile includes.

---

## Common gotchas

| Symptom | Cause | Fix |
|---|---|---|
| `expected '{' before 'thread_local'` / other identifiers-are-now-keywords | gcc 16 defaults to **gnu23**; older code uses C23 keywords as identifiers | `CFLAGS += -std=gnu11` |
| A **system** header shadows a bundled one (e.g. `<regex.h>` without `REG_STARTEND`) | musl include dir searched before the project's `-I` | put upstream's `-I` **before** the musl includes on the compile line |
| `… relocation … recompile with -fPIC` | linking `-pie` with non-PIC objects | `CFLAGS += -fPIC` |
| `cannot find -lrt` / `-lpthread` | musl folds `librt`/`libpthread` into `libc` | drop those `-l` flags (or the feature that adds them) |
| Missing `strlcpy`, `strlcat` | musl doesn't provide them | use the project's compat, or provide your own |
| Binary rejected by the loader near ~4 MB | unstripped symbols exceed `EXEC_MAX` | `${CROSS_PREFIX}strip` in `do-install` |
| `<sys/cdefs.h>` not found | musl doesn't ship it; BSD-ish code expects it | add the shim: `-I tools/ports/bmake/shim` |
| Nothing in the image | you ran `bmake`, not `bmake install` | staging is the `install` target |

---

## Build both arches, always

A port isn't done until it builds **and stages for both aarch64 and x86_64**
(the project keeps both 64-bit arches green — see the root `CLAUDE.md`). The
port worktree under `build/ports/<name>-<version>/` is **shared** across arches,
not arch-homed, so:

- The two arches must build **serially** — a concurrent aarch64 + x86_64 build
  writes objects into the same dirs and corrupts them.
- Your `build.sh` should `make clean` (or `find . -name '*.o' -delete`) first,
  so an arch switch starts from clean objects.

---

## Checklist

- [ ] `tools/ports/<name>/Makefile` with pinned URL/version/**SHA-256**.
- [ ] `build.sh` cross-compiles against musl (compile + link flags above).
- [ ] `-fPIC` / `-pie`; links `libc.so` + interpreter `/lib/ld-musl-${ARCH}.so.1`.
- [ ] `do-install` stages to the right hierarchy path and **strips**.
- [ ] Builds **and** installs for `TARGET=aarch64` *and* `TARGET=x86_64`.
- [ ] Verify the ELF: `readelf -dl <bin>` shows the expected `NEEDED`/interpreter.
- [ ] `bmake image` includes it; boot under QEMU and run it.
- [ ] A short design note under `docs/design/` for anything non-trivial.
