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
| C1 | `tools/stage-src.sh`: `ubfs cpr` repo → `…:/usr/src` (excl build/.git/images) | ☑ | MBR-parse for pool offset; resilient loop |
| C2 | Create `/usr/obj/${ARCH}` + `/usr/share/mk` on pool | ☑ | full tree staged (~200 MB, fits) |
| C3 | `bmake stage-src` target (standalone — no mkimage.sh edit) | ☑ | arch-dispatched |
| C4 | Reconcile CLAUDE.md "Installed layout" table | ☑ | added obj/share/mk + stage-src column |
| C5 | Verify via `ubfs ls` (`/usr/src/sys`, `/usr/obj/aarch64`, `/usr/share/mk`) | ☑ | all present; pool intact |

> **Plan revision (2026-06-24, user-directed):** do the gcc→clang switch on the
> **host first** (fast edit-compile loop), get world + kernel green under
> `TOOLCHAIN=clang` on both arches, *then* run the (already-debugged) clang build
> in-OS. The portability bugs reproduce identically on the host, so the VM is not
> needed to find them. Phases D/E below are now the **host** clang-green
> milestone; the in-OS bring-up is consolidated into Phase F. `TOOLCHAIN`
> defaults to gcc, so all of this is additive — the gcc build is untouched.

### Phase D — Host world green under `TOOLCHAIN=clang` (both arches)
| # | Step | Status | Note |
|---|------|--------|------|
| D1 | Host cross-drive seam in `ubix.toolchain.mk` (`.export TOOLCHAIN`; auto-detect homebrew clang) | ☑ | `f6bce7cc7`; no manual CC |
| D2 | **All 77 aarch64 world binaries build clean under clang** (C + C++) | ☑ | `-linux-musl` triple (GNU link driver) + `-U__linux__/__unix__/__gnu_linux__` (match gcc macros) |
| D3 | musl libc — reuse gcc-built `libc.a` (clang apps link it; ABI-compatible) | ☑ | musl self-rebuild under clang deferred (its gmake) |
| D4 | libcxx/libcxxabi + objgfx under clang (currently gcc-built; apps link fine) | ☐ | optional polish; C++ apps already green vs gcc libs |
| D5 | **x86_64 world green under clang (79 bins)** | ☑ | `-none-elf` picks Darwin linker for x86_64 on macOS → use `-linux-musl`+`-U` uniformly |
| D6 | netsurf clean-rebuild `mv` bug in `build-netsurf.sh` (pre-existing, not toolchain) | ☐ | only bites a clean rebuild; nsfb caches |

### Phase E — Host kernel green under `TOOLCHAIN=clang` (both arches)
| # | Step | Status | Note |
|---|------|--------|------|
| E1 | Kernel recipe uses `${KERN_CC}`/`ld.lld`/`llvm-objcopy` under clang | ☐ | the `find … | kbuild-cc.sh` loop + link |
| E2 | Inline asm / `-mgeneral-regs-only` / ISA flags clang-clean | ☐ | aarch64 + x86_64 |
| E3 | `ldscript.${ARCH}` links under `ld.lld` | ☐ | lld honors GNU scripts |
| E4 | Both kernels boot under QEMU (clang-built) | ☐ | the real green gate |

### Phase F — In-OS build (now that the clang build is host-green)
| # | Step | Status | Note |
|---|------|--------|------|
| F1 | In-OS tool gaps: `find` (port), `sed`/`objcopy`(llvm) under oksh | ☐ | was old Phase D |
| F2 | Boot image, `clang --version`/`-S` on-device (99 MB load; stream PT_LOAD if OOM) | ☐ | RAM bump + console driving |
| F3 | `bmake -C bin/cat OBJ_DIR=/usr/obj/aarch64` then `bmake world` in-OS | ☐ | runs the host-green build |
| F4 | `bmake kernel` in-OS; reproducibility vs host; fold into `self-hosting-plan.md` | ☐ | |

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

> Reordered 2026-06-24 (user-directed): shake out the gcc→clang bugs on the
> **host** (fast loop) before touching the VM. Phases D/E are the host
> clang-green milestone; Phase F is the in-OS bring-up that runs the result.

### Phase D — Host world green under `TOOLCHAIN=clang` (both arches)
Make `bmake world TARGET=<arch> TOOLCHAIN=clang` build the entire userland on the
host, fixing every gcc→clang portability bug in a fast edit-compile loop.

1. **Host cross-drive seam** — extend `ubix.toolchain.mk` so the clang profile
   works on the host (which defaults to the macOS target): inject
   `--target=${_ARCH}-unknown-linux-musl` + `--ld-path=…/ld.lld` via the compile
   and link **flags** (keeping `CC=clang` a single word so it passes cleanly
   through `WORLD_FLAGS`, musl's gmake, and the libcxx Makefiles). In-OS this is
   inert (native clang already defaults to the right target + lld).
2. **C world** — sweep all `bin/*` C apps; `cat` already builds. Deltas so far:
   `-nostdlibinc`, `-std=gnu23`.
3. **musl libc** — pass `CC=clang …` to musl's GNU-make build (Step 0).
4. **libcxx/libcxxabi + C++ apps** — C++ codegen + lld for objgfx/views/etc.
5. **x86_64** — the same, keeping both arches green.

### Phase E — Host kernel green under `TOOLCHAIN=clang` (both arches)
Build + boot a clang/lld kernel on the host.

1. **Recipe toolchain** — the `find … | kbuild-cc.sh` loop + link in the root
   `Makefile` use `${CROSS_PREFIX}gcc`/`ld`/`objcopy` directly; route them through
   `${KERN_CC}`/`${LD}`/`${OBJCOPY}` so clang/ld.lld/llvm-objcopy apply under the
   clang profile.
2. **ISA/asm** — `-mgeneral-regs-only`, inline asm, and the no-SIMD constraints
   must be clang-clean on both arches.
3. **Linker script** — `sys/compile/ldscript.${ARCH}` links under `ld.lld`.
4. **Boot** — both clang-built kernels boot under QEMU (the real green gate).

### Phase F — In-OS build (runs the host-green clang build on-device)
Now that the clang build is debugged on the host, bring it up in-OS.

1. **Tool gaps** — port `find`; confirm `sed`/`llvm-objcopy` run under oksh
   (was the old Phase D).
2. **Boot + run clang** — bump RAM, drive the console (or a boot-time self-test),
   `clang --version`/`-S` on-device. The 99 MB read-all-into-`kmalloc` load is the
   prime OOM suspect → stream PT_LOAD segments in `kern/execfile.c` if needed.
3. **In-OS world** — `cd /usr/src && bmake world OBJ_DIR=/usr/obj/aarch64
   TOOLCHAIN=clang` reproduces the host-green world.
4. **In-OS kernel** + reproducibility vs host; fold into `self-hosting-plan.md`
   Phases 4/6/7.

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
