# uBixOS `/usr/src` → `/usr/obj` On-Device Build & FreeBSD-Style Layout Plan

**Status:** DESIGN (approved 2026-06-24). Complements
[`self-hosting-plan.md`](self-hosting-plan.md) — that doc owns the toolchain
bring-up phases (bmake/oksh/Stage-0 clang); *this* doc owns the **source-tree
layout on the image** and **making `bmake` build from `/usr/src` into
`/usr/obj`** on both the host (cross) and in-OS (native clang).

## Progress matrix

Legend: ☑ done · ◐ in progress · ☐ todo · ⊘ blocked

### Prerequisites (already landed)
| # | Item | Status | Commit / note |
|---|------|--------|---------------|
| P1 | Stage-0 clang/lld/llvm-* built for uBixOS (aarch64) | ☑ | `4af1a1cfd` |
| P2 | `EXEC_MAX` 4 MB → 256 MB (both arches) | ☑ | `ccd24107a` |
| P3 | Stage-0 binaries stripped + staged into `build/${ARCH}/usr` | ☑ | `19c7c31dd` |

### Phase A — Relocatable bmake (host stays green)
| # | Step | Status | Note |
|---|------|--------|------|
| A1 | `SRCTOP` robust without git (derive from `Makefile.incl` location) | ☑ | `.PARSEDIR:tA`; resolves from root+subdir |
| A2 | `OBJ_DIR=/usr/obj/${_ARCH}` override verified; reconcile root-Makefile default | ☑ | override honored; removed dup default |
| A3 | Fix `install-world` relative `build/{bin,lib,libexec}` → `${OBJ_DIR}` | ☑ | |
| A4 | Verify: both arches green no-override; OBJ_DIR relocates whole link | ☑ | both kernels + bin/cat green; link all `${OBJ_DIR}` |

### Phase B — Native (in-OS) toolchain profile
| # | Step | Status | Note |
|---|------|--------|------|
| B1 | `share/mk/ubix.toolchain.mk` (gcc default + clang/ld.lld/llvm-* branch) | ☑ | central seam; gcc byte-identical |
| B2 | Toolchain-agnostic consumers (bin/lib/libexec incl + 5 musl mk + WORLD_FLAGS) | ☑ | `TC_NOSTDINC`/`TC_STDFLAG`; in-OS auto-detect deferred to E |
| B3 | Confirm clang/lld link rule (crt, -pie, dyld) | ☑ | clang cat = aarch64 musl PIE |
| B4 | Host cross-drive `CC=clang --target=…`; gcc full world green | ☑ | deltas: `-nostdlibinc`, `-std=gnu23`, `aarch64linux` |

### Phase C — Stage source tree + obj dir into the image
| # | Step | Status | Note |
|---|------|--------|------|
| C1 | `tools/stage-src.sh`: `ubfs cpr` repo → `…:/usr/src` (excl build/.git, trim libcxx) | ☐ | |
| C2 | Create `/usr/obj/${ARCH}` + `/usr/share/mk` on pool; log staged size | ☐ | |
| C3 | Wire into image target (avoid contested mkimage.sh hunk) | ☐ | |
| C4 | Reconcile CLAUDE.md "Installed layout" table | ☐ | |
| C5 | Verify via `ubfs ls` (`/usr/src/Makefile`, `/usr/obj/aarch64`, `/usr/share/mk`) | ☐ | |

### Phase D — Close in-OS build-tool gaps
| # | Step | Status | Note |
|---|------|--------|------|
| D1 | Port `find` (busybox) or replace kernel-recipe `find` with explicit list | ☐ | |
| D2 | Confirm `sed` + `kbuild-cc.sh` run under oksh in-OS | ☐ | |
| D3 | Map kernel `objcopy -I binary` embeds → `llvm-objcopy` (or gate off in-OS) | ☐ | |
| D4 | musl: ship prebuilt, defer musl self-rebuild (rebuild world against it) | ☐ | |
| D5 | Verify: dry-run kernel `find` enum + one `kbuild-cc.sh` compile in-OS | ☐ | |

### Phase E — Boot Stage-0, verify clang, first in-OS compile
| # | Step | Status | Note |
|---|------|--------|------|
| E1 | Build image (Stage-0 + `/usr/src`); boot `run-debug-aarch64` (serial) | ☐ | |
| E2 | `clang --version` + `clang -S t.c` on-device (99 MB load test) | ☐ | streaming loader fallback |
| E3 | `clang hello.c -o hello && ./hello` (ld.lld + crt + libc) | ☐ | |
| E4 | `bmake -C bin/cat OBJ_DIR=/usr/obj/aarch64` → on-device binary | ☐ | |

### Phase F — Full in-OS world, then kernel
| # | Step | Status | Note |
|---|------|--------|------|
| F1 | `bmake world OBJ_DIR=/usr/obj/aarch64` in-OS reproduces host world | ☐ | |
| F2 | `bmake kernel` in-OS (find + ld.lld + llvm-objcopy) | ☐ | |
| F3 | Reproducibility: in-OS `/usr/obj/aarch64` ≈ host `build/aarch64` | ☐ | |
| F4 | Fold E/F outcomes into `self-hosting-plan.md` Phases 4/6/7 | ☐ | |

### Path to cleaner layout (#1 → #2)
| # | Step | Status | Note |
|---|------|--------|------|
| G1 | Repo tidy to FreeBSD-shaped top-level (fold `mk/`→`share/mk`, etc.) | ☐ | incremental |
| G2 | Real `installworld`/`installkernel` (source → installed homes) | ☐ | |

## Context

uBixOS just gained a **Stage-0 toolchain** — statically-linked aarch64
`clang`/`lld`/`llvm-ar`/`nm`/`objcopy`/`ranlib` that run on uBixOS (commit
`4af1a1cfd`; `EXEC_MAX` raised to 256 MB in `ccd24107a`; staged into
`build/${ARCH}/usr` in `19c7c31dd`). The next milestone is to **build uBixOS
from within uBixOS**: ship the source tree on the image in a FreeBSD-style
layout and make `bmake` work from `/usr/src` building into `/usr/obj`, using the
native clang instead of the host cross-gcc — producing the same result `bmake`
produces from the repo root on the host today.

**Why now:** the toolchain half is done; what remains is (1) get the *sources*
on-device in a recognized layout, (2) make the build relocatable so the same
Makefiles drive a `/usr/src` → `/usr/obj` build, and (3) switch the in-OS build
to the native clang. The reward is a self-hosting OS — the classic milestone
that proves the toolchain, libc, kernel syscalls, and userland are all coherent.

**Recon finding (the good news):** the build is already ~80 % relocatable.
`Makefile.incl` computes `SRCTOP` (via `git rev-parse`, falling back to
`${.CURDIR}`), derives `UBIX_MK=${SRCTOP}/share/mk` and `OBJ_DIR`, and
`.export`s all three; subdir Makefiles already consume `${OBJ_DIR}`/`${UBIX_MK}`
and inherit them through the environment. This is mostly *closing concrete gaps*,
not a rewrite.

## Decisions

- **Layout:** start with **whole repo → `/usr/src`** verbatim (preserves every
  relative include — e.g. `bin/cat/Makefile` does `include ../../Makefile.incl`
  — so bmake works unchanged); install `share/mk` → `/usr/share/mk`. This is the
  "easy win." Long-term we iterate toward a cleaner FreeBSD-faithful structure
  (see *Path to the cleaner layout*).
- **Scope:** include the **native-clang in-OS compile** — verify clang/lld
  actually rebuild world (and ultimately kernel) on-device, not just
  relocatability.
- **Object dir:** **`/usr/obj/<arch>`** (e.g. `/usr/obj/aarch64`), mirroring
  today's `build/<arch>` split — one `OBJ_DIR` override.

**Non-negotiables:** keep **both** 64-bit arches green at every step (the host
cross-build must not regress — it is the gate); design-only until approved;
prefer changes confined to dedicated files (`mkimage.sh` carries another agent's
in-flight hunk — stage source via a separate helper to avoid index contention).

## Current state (what recon established)

- **Variable flow:** `Makefile.incl` → `SRCTOP` (git or `.CURDIR`),
  `UBIX_MK=${SRCTOP}/share/mk`, `OBJ_DIR ?= ${SRCTOP}/build/${_ARCH}`, all
  `.export`ed. Root `Makefile` also sets `OBJ_DIR ?= ${CURDIR}/build/${_ARCH}`
  and drives world via `WMAKE … BUILD_DIR=${OBJ_DIR}`; the kernel via explicit
  `find … | kbuild-cc.sh` shell loops.
- **Toolchain selection:** `share/mk/ubix.platform.mk` +
  `ubix.target.${ARCH}.mk` set `CROSS_PREFIX` (`aarch64-elf-`), `LIBGCC`, etc.,
  and export them. World CFLAGS/link rules live in `share/mk/ubix.musl.*.mk`;
  they already key off `${CC}`/`${LD}`/`${OBJ_DIR}`/`${SRCTOP}`.
- **Relocatability gaps:** (1) `SRCTOP` via git fails in a `/usr/src` with no
  `.git` — the `.CURDIR` fallback only works when bmake runs from the tree root;
  (2) `install-world` uses **relative** `find build/bin …` instead of
  `${OBJ_DIR}`; (3) the default `OBJ_DIR` points inside the source tree; (4) no
  **native** (non-cross) toolchain profile exists.
- **Kernel build host-deps:** `find` (source enumeration),
  `${CROSS_PREFIX}{gcc,ld,objcopy}`, `head -c`, and `kbuild-cc.sh`'s `sed`.
  World additionally builds **musl via GNU make** (`${GNU_MAKE}`).
- **Image staging:** `tools/mkimage.sh` builds a STAGE tree and `ubfs cpr`s it
  into the UbixFS pool. It stages `/bin /lib /sbin /usr/{bin,sbin,lib} /etc
  /var …` — **no `/usr/src`, `/usr/include`, `/usr/obj`, or `/usr/share/mk`**
  today (the CLAUDE.md `/usr/src` claim is the legacy `install-world` FAT path).
  `ubfs` supports `mkpool/mkdir/cp/cpr/ln/ls/rm` and **preserves symlinks**
  since `a5bc599fe`.
- **Sizes:** source ≈ 12 MB (sys/bin/lib/libexec/include/share/mk/tools) + ~84
  MB contrib (musl 12 MB, libcxx 72 MB) ≈ **96 MB**; world ≈ 290 MB; Stage-0
  staged ≈ 186 MB; a full obj tree ≈ 300 MB. All fit the **3072 MB** pool with
  ~2 GB to spare. (libcxx's 72 MB dominates contrib; stage a trimmed subset.)
- **In-OS readiness:** `bmake` + `oksh` run in-OS and the SIGCHLD-after-fork
  blocker is **resolved** (recipes execute end-to-end). `find`/`sed` are the
  main coreutils gaps for the kernel recipe; `clang` integrated-as removes the
  `as` dependency; `ld.lld` honors the GNU linker scripts.

## Plan

### Phase A — Make bmake fully relocatable (host stays green)
`bmake -C <tree> OBJ_DIR=<obj>` works regardless of where the tree lives or
whether it's a git checkout. No behavior change for the repo-root host build.

1. **Robust `SRCTOP` without git** — in `Makefile.incl`, derive `SRCTOP` from
   the location of `Makefile.incl` itself, keeping the git probe only as an
   optimization and `.CURDIR` as the final fallback. Makes a `.git`-less
   `/usr/src` work from any subdir.
2. **`OBJ_DIR` default → arch-homed, overridable** — keep `${SRCTOP}/build/
   ${_ARCH}` as the host default; verify `OBJ_DIR=/usr/obj/${_ARCH}` cleanly
   overrides; reconcile the duplicate default in the root `Makefile` so the
   override isn't shadowed.
3. **Fix relative output paths** — change `install-world`'s `find build/{bin,
   lib,libexec}` to `${OBJ_DIR}/…`; audit for other bare `build/` literals.
4. **Verify** on host: `bmake world TARGET=aarch64 OBJ_DIR=/tmp/obj-test` builds
   into the alternate dir; both arches still build with no override.

### Phase B — Native (in-OS) toolchain profile
A build profile that uses the on-device clang/lld/llvm-* instead of host
cross-gcc, selected automatically when building in-OS.

1. **`share/mk/ubix.target.native.mk`** (new) — `CROSS_PREFIX=` empty, `CC=clang`,
   `CXX=clang++`, `LD=ld.lld`, `AR=llvm-ar`, `RANLIB=llvm-ranlib`,
   `OBJCOPY=llvm-objcopy`, `NM=llvm-nm`, `AS=clang` (integrated-as). `LIBGCC`:
   the cross-gcc archive is host-only; in-OS resolve clang builtins from a
   shipped builtins archive (the same `-lgcc` the Stage-0 link used). Keep ABI
   knobs (`-march=armv8-a`, musl paths, PIE flags) identical to the cross profile.
2. **Profile selection** — add a uBixOS branch to `ubix.platform.mk`: when
   running in-OS, select the native profile. Host cross-builds unaffected.
3. **World link rule** — `ubix.musl.prog.mk` already parameterizes on
   `${CC}`/`${LD}`/`${LIBGCC}`/`${MUSL_*}`; adjust only where gcc-isms leak.
4. **Verify** on host first by *cross-driving* clang
   (`CC=clang --target=aarch64-…-musl`) to shake out flag differences before the
   on-device run.

### Phase C — Stage the source tree + obj dir into the image
`/usr/src` (whole repo), `/usr/obj/<arch>` (empty, writable), `/usr/share/mk` on
the pool — without editing the contested `mkimage.sh` hunk.

1. **New staging step** (`tools/stage-src.sh`, invoked from the image target)
   that `ubfs cpr`s the repo into `…:/usr/src`, **excluding** `build/`, `.git/`,
   and unused contrib (stage `contrib/musl` + needed libc++ headers; skip libcxx
   test/docs to trim the 72 MB). Create `/usr/obj/${ARCH}` and `/usr/share/mk`.
2. **Pool sizing** — 96 MB src + 290 MB world + 300 MB obj ≈ 700 MB < 3072 MB;
   no growth needed. Log staged size as a guard.
3. **Symlinks** — `ubfs cpr` preserves them; the source tree has few, low risk.
4. **Reconcile CLAUDE.md** — make the "Installed layout" table match what the
   pool flow actually stages.
5. **Verify** — rebuild image; `ubfs ls` confirms `/usr/src/Makefile`,
   `/usr/src/share/mk`, `/usr/obj/aarch64`, `/usr/share/mk`.

### Phase D — Close in-OS build-tool gaps
Every host tool the build shells out to either exists in-OS or is removed from
the in-OS path.

1. **`find`** — the kernel build enumerates sources via `find sys/arch/<arch>
   -name '*.S'`. Port a minimal `find` (busybox has one — same pattern as the
   ~35 coreutils already vendored) or replace the recipe's `find` with an
   explicit list. Prefer porting `find` (reusable).
2. **`sed`** — already vendored + verified in-OS; confirm `kbuild-cc.sh`'s usage
   works under oksh.
3. **Kernel embed steps** — `objcopy -I binary` blob embeds map to
   `llvm-objcopy` in the native profile, or gate the demo-embed steps off in-OS
   (they're bring-up artifacts).
4. **musl via GNU make** — musl's Makefile is GNU-make syntax. For the in-OS
   world rebuild, ship prebuilt musl and rebuild it only in a later phase
   (rebuild world *against* existing musl first); porting `gmake` is the
   alternative.
5. **Verify** — dry-run the kernel `find` enumeration + one `kbuild-cc.sh`
   compile in-OS.

### Phase E — Boot Stage-0, verify clang runs, first in-OS compile
Prove the toolchain executes on-device and compiles one TU before a full world.

1. **Build image** with Stage-0 + `/usr/src`; boot `run-debug-aarch64` (serial).
2. **Smoke-test** `clang --version`, then `clang -S <tiny>.c` (exercises the
   99 MB load under the raised `EXEC_MAX`). If the read-all-into-`kmalloc`
   loader OOMs/faults on 99 MB, switch `read_elf_file` (both arches'
   `kern/execfile.c`) to **stream PT_LOAD segments** from the file.
3. **Link-test** `clang hello.c -o hello && ./hello` (exercises `ld.lld` + crt +
   libc on-device).
4. **First bmake compile** — `cd /usr/src && bmake -C bin/cat
   OBJ_DIR=/usr/obj/aarch64` (native profile) → `/usr/obj/aarch64/bin/cat`.

### Phase F — Full in-OS world (then kernel) rebuild
`cd /usr/src && bmake world OBJ_DIR=/usr/obj/aarch64` in-OS reproduces the host
world; then the kernel.

1. **World** — iterate the bin/lib gaps surfaced in E; each is a flag/path fix
   in the native profile, not a redesign.
2. **Kernel** — `bmake kernel` in-OS: needs `find` (D), `ld.lld` +
   `ldscript.aarch64` (works), `llvm-objcopy`. The shell-loop recipe runs under
   oksh.
3. **Reproducibility** — compare in-OS `/usr/obj/aarch64` against host
   `build/aarch64` (Stage-2 idea from `self-hosting-plan.md`).
4. Aligns with `self-hosting-plan.md` Phases 4/6/7 — update that doc, don't
   duplicate.

## Path to the cleaner layout (#1 → #2, iterative)

We start by dumping the repo in `/usr/src` (#1) because the build's *relative*
includes couple on-disk shape to repo shape. The clean long-term target (#2) is
**separating source from installed**, FreeBSD-style — and the right way to reach
it is to refactor the *repo*, not to remap dirs at staging time:

- **Repo refactor** — make the repo top-level already FreeBSD-shaped (it nearly
  is: `sys bin lib libexec contrib share tools include`). Tidy-ups (e.g. folding
  `mk/` into `share/mk`) happen in the repo, so `cp repo → /usr/src` keeps
  yielding the right shape and relative includes survive.
- **Real `installworld`/`installkernel`** — targets that map **source →
  installed homes**: `share/mk → /usr/share/mk`, `etc → /etc`, built binaries →
  `/usr/bin`, headers → `/usr/include`. Then `/usr/src` holds only source,
  `/usr/obj` holds objects, and the running system uses `/etc` + `/usr/share/mk`
  + `/usr/bin`. This *is* the #2 intuition (etc/mk under their installed homes),
  correctly framed as install locations rather than staging remaps.
- **Sequencing** — Phase C ships #1. The `installworld` mapping and any repo
  tidy land incrementally, each keeping both arches green. No big-bang move.

## Representative files to change

- `Makefile.incl` — robust `SRCTOP`; confirm `OBJ_DIR`/`UBIX_MK` export.
- `Makefile` (root) — reconcile `OBJ_DIR` default; fix `install-world` relative
  `build/` paths; add `installworld` (later).
- `share/mk/ubix.target.native.mk` (new) — native clang/lld/llvm-* profile.
- `share/mk/ubix.platform.mk` — in-OS detection → native profile.
- `share/mk/ubix.musl.prog.mk` / `ubix.musl.vars.mk` — verify clang/lld link
  rule (minimal).
- `tools/stage-src.sh` (new) + image-target wiring — stage `/usr/src` +
  `/usr/obj/<arch>` + `/usr/share/mk`.
- `bin/find/` (new) + `contrib/busybox` wrapper — port `find` (Phase D).
- `sys/arch/{aarch64,x86_64}/kern/execfile.c` — only if Phase E shows the 99 MB
  read-all load needs segment streaming.
- `CLAUDE.md` + `self-hosting-plan.md` — reconcile layout claims; fold Phases
  E/F into the self-hosting roadmap.

## Verification (end-to-end)

1. **Host unaffected:** `bmake kernel world TARGET=aarch64` and `TARGET=x86_64`
   both green with no override — the regression gate.
2. **Relocatable host build:** `bmake world TARGET=aarch64 OBJ_DIR=/tmp/obj`
   populates `/tmp/obj`, not `build/`.
3. **Image staging:** rebuild image; `ubfs ls …:/usr/src`,
   `…:/usr/obj/aarch64`, `…:/usr/share/mk` present; staged size < pool.
4. **On-device toolchain:** boot serial; `clang --version`, `clang -S t.c`,
   `clang hello.c -o hello && ./hello` succeed.
5. **On-device bmake:** `cd /usr/src && bmake -C bin/cat
   OBJ_DIR=/usr/obj/aarch64` → `/usr/obj/aarch64/bin/cat`; then `bmake world`,
   then `bmake kernel`.
6. **Reproducibility:** in-OS `/usr/obj/aarch64` artifacts match host
   `build/aarch64` (sizes/symbols).

## Risks / open items

- **99 MB clang load** under the read-all-`kmalloc` loader may OOM/fault →
  segment-streaming refinement (Phase E.2) is the scoped fallback (real kernel
  work).
- **musl-via-gmake** is the one world component not bmake-buildable in-OS →
  deferred (rebuild world against prebuilt musl first).
- **Image authoring** (`python3`/`mtools` in mkimage) stays host-side — fine;
  it's not part of an in-OS *world/kernel* build, only of producing a fresh
  image.
- **mkimage.sh contention** — another agent has an in-flight hunk there; stage
  source via a separate `stage-src.sh`.
- **libcxx 72 MB** — stage a trimmed subset (headers + needed sources), not the
  whole contrib dir.
