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

- **Phase 2 — one world subdir on-device — DONE (commit 5fff626ce).**
  `bmake -C /usr/src/bin/cat SRCTOP=/usr/src OBJ_DIR=/usr/obj/aarch64` compiles +
  links a valid 42 KB aarch64 PIE that runs on-device (`cat` prints). The real
  blocker chain that had to be cleared (all `UBIX_NATIVE`-gated, host byte-identical):
  (a) `platform.mk` `LIBGCC != gcc` → native LIBGCC=/lib/libgcc.a + UBIX_NATIVE;
  (b) `target.aarch64.mk` re-set CROSS_PREFIX=aarch64-elf- + a 2nd `LIBGCC != gcc`
      → skipped when native; (c) generated musl headers (`bits/alltypes.h`) not on
  device → stage-src pre-seeds `/usr/obj/${ARCH}/obj/musl/obj/include`;
  (d) link `-L/lib` for libc/crt (prog.mk `-L${MUSL_LIB}`); (e) ld.lld ThreadPool
  crash → `-Wl,--threads=1` (MUSL_NATIVE_LDFLAGS). **Still open:** `SRCTOP`
  auto-detect for *direct* subdir builds (top-level `bmake` exports it; direct
  needs `SRCTOP=/usr/src`) — fix = walk `.CURDIR` up to the dir containing
  `share/mk` when `.git`/`.PARSEDIR:tA` don't yield the root on-device.

- **Phase 3 — `bmake world` on-device — mechanisms PROVEN; orchestration target pending.**
  Verified on-device (commits aa2dc5742 SRCTOP walk-up + the Phase-2 retarget):
  - `bmake -C /usr/src/bin/cat OBJ_DIR=/usr/obj/aarch64` (no SRCTOP=) → builds + runs.
  - `bmake -C /usr/src/bin echo _ARCH=aarch64 OBJ_DIR=/usr/obj/aarch64` → the SUBDIRS
    dispatcher's `(cd echo; ${MAKE})` descent works; `${MAKE}` resolves to /bin/bmake
    (mkimage stages it there) and builds echo. So the descent + sub-make path are sound.
  - **DISPATCHER _ARCH default — DONE (commit d8d5806bb).** bin/Makefile,
    usr.bin/Makefile, tests/Makefile read `${_ARCH}` at parse time without including
    Makefile.incl, so a DIRECT `bmake -C <tree>` on-device died "Malformed
    conditional".  Added `_ARCH ?= aarch64` (overridden by the command-line `_ARCH=`
    the top-down/cross world passes, so host builds are byte-identical).  Now
    `bmake -C /usr/src/bin OBJ_DIR=/usr/obj/aarch64` (no _ARCH=) descends + builds a
    runnable program on-device.  **So building a whole TREE on-device works.**
  Remaining for a full top-level `bmake world` on-device (the orchestration):
  1. Gate the host-only `world` steps when native (`UBIX_NATIVE`): Step 0 musl
     rebuild (musl is prebuilt + staged in /lib), Step 4 NetSurf (host bison/libpng/
     nsgenbind).  Set `MAKE=/bin/bmake`.  Additive + low-risk.
  2. **The real wall: libcxx + the C++ apps.**  Step 1a/1b builds libcxxabi/libcxx,
     and usr.bin has C++ apps (vdoom, tessera, cubitaire, taskbar, term, settings,
     files, activity, diskutil) that need it.  A native `world` must either build
     libcxx on-device (a large C++ build) or skip the C++ apps (a native usr.bin
     SUBDIRS subset).  This is the substantive remaining work.
  3. A full world build is slow on-device (per-invocation clang/ld.lld demand-paging),
     so verify a tree at a time, not one long run.
  STATUS: per-TREE on-device builds work (dispatcher fix).  Full `bmake world` is the
  libcxx/host-step orchestration above — a larger follow-on.

- **Phase 4 — kernel on-device — MECHANISMS DONE (commit 434f6b3b4).** `bmake kernel`
  now runs on-device with the in-OS toolchain. Three de-host-ifications, all
  `UBIX_NATIVE`/host-byte-identical:
  1. **Source sweep portability** — `find sys/arch/aarch64 -name '*.S' -not -path
     '*/board/*'` → `find ... | grep -v '/board/'`. busybox find gates BOTH `!` and
     `-not` on `FEATURE_FIND_NOT` (and `-not` also needs `ENABLE_DESKTOP`), so neither
     negation predicate is portable; the grep form needs only `-name`. (This was the
     on-device `find: unrecognized: -not`.)
  2. **`KERN_USER_CC`** (`ubix.toolchain.mk`) — the embedded user demos (hello.elf, the
     boot triad, the linker test) used `${CROSS_PREFIX}gcc`; on-device that is bare
     `gcc` (absent). New var: `aarch64-elf-gcc` on the host, in-OS `clang` when
     `UBIX_NATIVE`. The `kernel-aarch64` embed rules use it.
  3. **Stage `llvm-objcopy` + `llvm-nm`** (`tools/ports/llvm/build.sh`) — the embed
     step objcopy's blobs into the kernel; only clang/ld.lld/llvm-ar/llvm-ranlib were
     shipping. mkimage copies `build/${ARCH}/usr` verbatim, so they reach the device.

  Validated piecewise on the HVF harness (a full ~120-file build is slow — each clang
  demand-pages the 100 MB binary fresh): grep sweep + clang compile loop (exit 0),
  clang embed (CCDONE0), `llvm-objcopy` embed (OCDONE0), `ld.lld` link.

  **Remaining for a self-built kernel that BOOTS:** on-device the musl-linked embeds
  (init/login/sh, worldcat, the dynamic linker test) stub out — their gate
  `[ -f ${OBJ_DIR}/lib/libc.a ]` is false because there is no `libc.a`/crt at
  `/usr/obj/${ARCH}/lib` on-device (the shipped libc lives in `/lib`). So a self-built
  kernel links with 16-byte stub embeds; it can still boot from disk, but the embedded
  bring-up triad is inert. Making the embeds real on-device = stage `libc.a` + crt at
  the OBJ path (or point the embed link at `/lib`). Producing `boot/kernel` on-device
  is the milestone; booting the self-built kernel is the follow-on.

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
