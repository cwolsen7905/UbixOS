# On-device build (self-hosting the world build) — scoping + plan

**Status:** scoping (2026-06-27, [selfhost-perf]). No code changes yet — this
documents what it takes to `bmake` uBixOS components *on the device* with the
on-device clang/lld toolchain instead of the host cross-GNU toolchain.

## TL;DR

The hard part is already done. `share/mk/ubix.toolchain.mk` has a **clang
profile** that is explicitly designed to use the bare in-OS `clang`/`ld.lld`/
`llvm-*` (no `--target`, no `--ld-path`) when the Homebrew LLVM isn't present —
i.e. on-device. Compiling + linking a *single program* on-device already works
(verified: `bmake`-driven recipe → clang + headers → ld.lld → running binary).

What blocks `bmake` of the **world Makefiles** on-device is a short list of
**host-isms** that assume a cross-build host, plus a few **host-only build
steps** that simply shouldn't run on-device. None require rearchitecting the
build; they need a *native* (`uname -s == UBIX`) branch in two `.mk` files and a
profile that skips the host-only steps.

## Current state (what works / what breaks)

- ✅ Individual program: `clang -c foo.c -o foo.o` + the `-nostdlib -pie … ld.lld`
  link recipe → a running PIE. Headers (`/usr/include`, clang resource dir),
  CRT (`/lib/{Scrt1,crti,crtn}.o`), `libgcc.a`, `/bin/sh`, `bmake`, `sys.mk` all
  ship on the image (mkimage, commit 07baa24b7).
- ✅ Relative paths + cwd (commit 48dcf0e43) and real file modes (351a53edb) — so
  on-device tools behave normally.
- ❌ `bmake` of `/usr/src` (or any world subdir) fails: the build invokes the
  **host cross toolchain** (`aarch64-elf-gcc`, host `gcc -print-libgcc-file-name`,
  host `objcopy`) which don't exist on-device → `exit 127`; and the musl/crt/lib
  paths point into the host build tree, which isn't on the device.

## Host-isms inventory (precise)

1. **`share/mk/ubix.platform.mk:50`** — `LIBGCC != ${CROSS_PREFIX}gcc -print-libgcc-file-name`.
   Runs unconditionally at parse time. On-device `uname -s == "UBIX"` falls into
   the `.else` branch (`CROSS_PREFIX=""`), so this execs bare `gcc` → not found →
   empty `LIBGCC` + the `exit 127` warnings seen in serial.log.
   **Fix:** add a `UNAME_S == "UBIX"` native branch: `CROSS_PREFIX=""`,
   `LIBGCC=/lib/libgcc.a` (the staged one), and guard the `!=` so it never execs
   a compiler on-device.

2. **`share/mk/ubix.musl.vars.mk:16-17,43`** — `MUSL_OBJ=${OBJ_DIR}/obj/musl`,
   `MUSL_LIB=${MUSL_OBJ}/lib`, `MUSL_CRT1=${MUSL_LIB}/Scrt1.o`. These resolve into
   the host build tree (absent on-device). On-device the crt objects + libc live
   in `/lib`. **Fix:** native branch sets `MUSL_LIB=/lib` (so `MUSL_CRT1=/lib/Scrt1.o`,
   crti/crtn from `/lib`), and `-L/lib -lc` already finds `/lib/libc.so`.

3. **`share/mk/ubix.toolchain.mk`** — clang profile already correct on-device:
   `.if exists(${_LLVM18}/clang) … .else _CLANG ?= clang` → bare `clang`/`ld.lld`/
   `llvm-*`. `_TGT`/`_UNDEF` are inert in-OS (clang's default *is* linux-musl).
   **No change needed** beyond confirming `TOOLCHAIN=clang` (already the default).
   Caveat: `OBJCOPY=llvm-objcopy`, `AR=llvm-ar`, `NM=llvm-nm`, `RANLIB=llvm-ranlib`
   must be staged on the image (currently only `clang` + `ld.lld` are) — needed by
   any rule that archives/strips (libs, kernel embeds).

4. **`Makefile.incl` SRCTOP/OBJ_DIR** — SRCTOP detection already handles a
   `.git`-less `/usr/src` (falls back to `.CURDIR`/`.PARSEDIR`). For on-device,
   build with `OBJ_DIR=/usr/obj/${ARCH}` (per usr-src-build-layout-plan.md) so
   output doesn't try to write the host `build/` path. **Fix:** none in the file;
   just pass `OBJ_DIR=/usr/obj/${_ARCH}` (or make the native branch default it).

5. **Recursive sub-make path** — the world descends subdirs via `${MAKE}`; on-device
   `bmake` must be on `PATH` at a stable location. mkimage stages it to `/bin/bmake`
   (via the `/bin` loop); ensure `${MAKE}`/`MAKEFLAGS` resolve to that (or set
   `MAKE=/bin/bmake`). (The "/bin/bmake not loadable" cascade was this.)

## Host-only steps (must be SKIPPED on-device, not ported)

- **NetSurf** (world Step 4): needs host `bison`, `libpng`, `nsgenbind` → never
  build on-device. Gate off when native.
- **`tools/makereg.c`, `tools/mkimage.sh`** (host `cc`, python, mtools): image +
  registry authoring is a host operation. N/A on-device.
- **musl rebuild** (`contrib/musl`): musl's own `configure`/Makefile (GNU make,
  shell feature tests). Treat musl + the CRT/libc as *prebuilt* on-device (they're
  already staged in `/lib`); do not rebuild musl in-OS for now.
- **Kernel embeds** (`Makefile` kernel-aarch64): the embedded user demos use
  `${CROSS_PREFIX}gcc` + objcopy. Kernel self-build is Tier 4 (below).

## Phased plan

- **Phase 1 — single program (DONE).** clang + link recipe on-device → running
  binary. Ships in the image.

- **Phase 2 — one world subdir on-device (smallest real step).** Goal:
  `cd /usr/src/bin/cat && bmake OBJ_DIR=/usr/obj/aarch64` builds `cat`. Needs:
  the platform.mk native branch (#1), the musl.vars native branch (#2), staged
  `llvm-ar`/`llvm-objcopy`/`llvm-ranlib`/`llvm-nm` (#3). This proves the
  `ubix.musl.prog.mk` rule end-to-end on-device. Low risk — additive native
  branches; host builds unchanged (guarded by `UNAME_S == "UBIX"`).

- **Phase 3 — `bmake world` on-device.** Descend the program trees with the
  host-only steps gated off (NetSurf, makereg). Slow (demand-paging) but
  mechanical once Phase 2 holds. Needs a `world` target that skips host-only
  subdirs when native, and `MAKE=/bin/bmake`.

- **Phase 4 — kernel on-device.** clang compiles the kernel today; link via
  `ld.lld` (have it) + `llvm-objcopy` for the embeds; replace the `${CROSS_PREFIX}gcc`
  embedded-demo builds with clang. Hardest + least urgent (you rarely rebuild the
  kernel on the device); defer.

## Concrete first change (Phase 2 enabler)

Add to `ubix.platform.mk` (before the `LIBGCC !=`):
```
.elif ${UNAME_S} == "UBIX"          # native on-device build
CROSS_PREFIX ?=
CROSS_M32    ?=
LIBGCC       ?= /lib/libgcc.a        # staged; do NOT exec a compiler
```
and guard the `LIBGCC !=` with `.if !defined(LIBGCC)` (already present — just
ensure the native branch sets it first). Add the matching `MUSL_LIB ?= /lib`
native override in `ubix.musl.vars.mk`. Stage `llvm-{ar,objcopy,ranlib,nm}` in
mkimage. Then verify `bmake -C /usr/src/bin/cat OBJ_DIR=/usr/obj/aarch64` on-device.

## Verification

Per phase, on the HVF headless harness: build the target on-device, then run the
produced binary (and keep the host `bmake world TARGET=aarch64` + `TARGET=x86_64`
green — the native branches are `UNAME_S`-gated so the host path is untouched).

## Out of scope (explicitly)

musl/NetSurf rebuild on-device; image creation on-device; multi-job parallel
make (ld.lld already needs `--threads=1` pending the kernel futex work). Those are
host operations or separate kernel gaps.
