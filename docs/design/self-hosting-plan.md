# uBixOS Self-Hosting Plan

**Goal**: rebuild uBixOS — world *and* kernel — *from its own source, inside
uBixOS*, with no macOS/host cross-toolchain. "Self-hosting" means the whole build
pipeline runs on-device: a **compiler**, an **assembler**, a **linker**,
**ar/ranlib**, **make**, a **shell**, and the **coreutils** that drive a
`bmake world` / `bmake kernel` — plus the headers, libraries, and source already
staged on the image.

> **This is a multi-month research goal, not a sprint.** The compiler is
> **Clang/LLVM + lld** — a real, optimizing, production toolchain — *not* TCC.
> Read "Why Clang", the "Toolchain inventory", and the "Verified foundation"
> sections before picking up any phase. This v3 supersedes the i386-only v1
> (Clang-only, 2026-05-22) and the TCC-userland v2 (2026-06-20).

---

## Why Clang/LLVM (the compiler decision)

The self-hosting compiler is **Clang**, with **lld** as the linker and the LLVM
binutils (`llvm-ar`, `llvm-ranlib`, `llvm-objcopy`, `llvm-nm`). Reasons:

- **It's a real compiler.** Optimizing codegen, full C **and C++** — so the C++
  parts of the system (`views`, `objgfx`, `term`, the file manager) build
  on-device, not just C programs.
- **Both forward arches.** LLVM has mature **x86_64** *and* **AArch64** backends.
  uBixOS's primary forward target is aarch64; Clang can self-host it directly.
  (This is the decisive reason TCC was rejected — TCC 0.9.26 has no aarch64
  backend at all.)
- **lld unlocks the kernel.** lld honors GNU **linker scripts**, so it can drive
  the kernel's `sys/compile/ldscript.{x86_64,aarch64}` link. That makes *kernel*
  self-host reachable — no GPLv3 binutils `ld` required.
- **License-clean.** Clang/LLVM is **Apache-2.0** — permissive, compatible with
  uBixOS's BSD license. GCC/binutils (GPLv3) would contaminate a from-source
  toolchain; FreeBSD, OpenBSD, and macOS all made this same choice.
- **Native dev tooling.** `clang-format` / `clang-tidy` (already used off-device
  via `tools/mcr.sh`) become on-device.

> **TCC's role:** the vendored TCC (`contrib/tcc/`, staged to `/usr/bin/tcc`)
> stays as a *tiny on-device C compiler* for quick C and as a possible bootstrap
> shim — but it is **not** the self-hosting toolchain. The pipeline below is
> Clang/lld end-to-end.

---

## Reality check (what's true today, 2026-06-20)

1. **Arches.** i386 is being **frozen** (the x86_64 migration makes both forward
   arches 64-bit/LP64; the x86_64 port — `sys/arch/x86_64/`, the `bin/Makefile`
   x86_64 SUBDIRS — is already underway). **aarch64 is the primary forward
   target.** Self-hosting i386 is a dead end; **target x86_64 and aarch64.**
2. **The win Clang gives on arch:** unlike the TCC framing, there is **no missing
   backend** — Clang self-hosts the *primary* (aarch64) target. See the
   arch-ordering note.
3. **Self-hosting is the whole toolchain, not just a compiler** — Clang replaces
   cc+as+ld+ar+ranlib in one project, but a `bmake world` still needs **make** and
   a **shell** and **coreutils**. See the inventory.

---

## Toolchain inventory — what self-hosting actually needs

| Component | Provided by | State / gap |
|---|---|---|
| **C / C++ compiler** | **Clang** | ⬜ must be cross-built for uBixOS (Stage 0), then built natively (Stage 1). Covers cc + integrated assembler. |
| **Assembler** | Clang integrated-as | ✅ once Clang runs (handles inline + `.s`). |
| **Linker (userland + kernel)** | **lld** | ⬜ built with Clang; honors the kernel **linker script** → no binutils. |
| **ar / ranlib / objcopy / nm** | `llvm-ar` etc. | ⬜ built alongside Clang/lld from the LLVM tree. |
| **make** | **bmake** | ⬜ **only a toy in-tree.** `bin/make` is a 558-line "minimal POSIX-subset make" — it cannot parse the tree's BSD Makefiles (`.if`/`.for`/`!=`/`.CURDIR`/`.include`). **Real bmake must be ported** (user-requested; see the bmake section). |
| **shell** | `/bin/sh` | ✅ **oksh ported** (`tools/ports/sh/`, public-domain), runs in-OS, builtins + external exec work. bmake *recipe* exec via `sh -c` is blocked on the signal-after-COW-fork kernel bug (Phase 2). |
| **coreutils** | bin/* | 🟡 `cp`/`cat`/`ls`/`mkdir`/… exist; a `bmake world` also leans on `rm`, `mv`, `test`/`[`, `sed`, `install`, `mkdir -p`, `cmp` — gap-fill as the build surfaces them. |
| **C/C++ runtime libs** | musl + libcxx/libcxxabi + compiler-rt | 🟡 musl + libcxx are vendored and cross-built today; on-device we either rebuild them with Clang or ship prebuilt and relink. **compiler-rt** (Clang's builtins/`libgcc` equivalent) must be cross-built for the target. |
| **headers / libs / src** | image | ✅ `/usr/include`, `/usr/src` (full tree), `/lib` are staged per the CLAUDE.md layout. |

The key structural point versus the old plan: **lld means one toolchain covers
both world and kernel.** There is no "TCC can't link the kernel" barrier and no
need for a GPL binutils fallback.

---

## Verified foundation (re-checked against current code, 2026-06-20)

What the syscall table (`sys/posix/syscalls_posix.c`) and VMM actually provide —
the substrate Clang/lld and the build tools run on. Corrections from v1 flagged.

| Capability | Status | Evidence / correction |
|---|---|---|
| Anonymous mmap | ✅ | via **`mmap2` slot 477**. ⚠️ v1 cited 197 — that "old mmap" entry is `SYSCALL_INVALID`. LLVM's allocators, musl malloc, and lld's mmap of inputs all need this. |
| munmap / mprotect | ✅ | `munmap` (73), `mprotect` (74) `VALID`. mprotect matters for any JIT (disable LLVM JIT/ORC to avoid exec-page churn). |
| File-backed mmap + shared page cache | ✅ | demand-paged; shared file-page cache (aarch64 memory work). lld + Clang mmap object files and headers. |
| fork / execve / **vfork** | ✅ | `fork` (2), `execve` (59); COW fork both arches. **vfork** fixed 2026-06-22 (musl was emitting the Linux clone syscall; now aliased to fork). The Clang **driver** + bmake fork per step. |
| wait4 / SIGCHLD / zombies | 🟡 | `wait4` (7) `VALID`; signal Phases 1–5 complete — **but** delivering a signal to a userland *handler* after a COW fork SIGSEGVs on aarch64 (found via bmake's SIGCHLD handler; handed to `[ls/smp]`). Blocks any forking program that handles SIGCHLD. |
| rename / getdirentries / getcwd | ✅ | (128 / 196 / 326). |
| symlink | ✅ syscall | (57) wired — but the **ubixfs pool root has no symlinks**; LLVM/CMake install steps that symlink may fail on the pool FS. |
| chmod / getrlimit / setrlimit / sigaction | ✅ | (15 / 194 / 195 / 416). rlimits size the compiler's stacks. |
| **utimes / futimes / lutimes / fchmod** | 🟡 **NOTIMP** | (138 / 206 / 276 / 124). make + LLVM's build stamp mtimes; **gap for incremental builds** (clean builds are fine). Concrete near-term syscall work. |

**Verdict:** the process/VM substrate (Phases 1–3) holds; the **mtime/`fchmod`
gap** is the first concrete fix. Write an in-OS self-test suite (Phase 0) rather
than trusting the matrix.

---

## Capacity (a hard Clang prerequisite, not optional)

LLVM is big where TCC was tiny — this is the cost of a real compiler:

- **Disk**: LLVM source ~800 MB; a clang build is multiple GB of objects. The image
  must grow to **4–8 GB** (`tools/mkimage.sh`/`-arm.sh`), and the **FAT write
  path** must survive large files + deep `llvm/` trees (the most likely bug to
  surface). The ubixfs pool root must size up to match.
- **RAM**: LLVM links want **GB-class** memory. aarch64 already lifted its
  DTB-derived RAM cap (~2 GB); the **x86_64 VMM must match**. Build sub-projects in
  stages (`llvm-tblgen` → `libLLVM` → `clang` → `lld`) and lean on swap if needed.
- **Time**: a full LLVM build under QEMU/TCG is *many hours*. On Apple-Silicon dev
  hardware, **aarch64 under HVF runs near-native** — a strong reason to self-host
  aarch64 first (below).

---

## Phases

### Phase 0 — Foundation: verify + close the gaps
In-OS `selfhost/test_*.c`: anon + file-backed `mmap`; `fork`+`exit`→`wait4` decode;
`getdents`/`rename`/`getcwd`/`chmod`/`getrlimit`. **Implement `utimes`/`fchmod`**
(NOTIMP today) so make's dependency tracking works. Grow the image + stress the FAT
write path (the capacity work above). *Independently useful regardless of compiler.*

### Phase 1 — Port bmake (keystone) — ✅ BUILT + RUNS (2026-06-21)
**Done:** `tools/ports/bmake/` (the first port; proved `share/mk/ports.mk`).  bmake
cross-builds on both arches and runs in UbixOS — verified on aarch64 over the
serial console: `bmake -r -V MACHINE` → `aarch64`, and it parses a real makefile
file + evaluates a variable, clean exit.  Two follow-ups it surfaced, both **Phase
2** (not bmake bugs): (1) bmake's default makefile **`mmap` SIGABRTs** → a
file-backed-mmap kernel bug; worked around by disabling `HAVE_MMAP`.  (2) **recipe
execution needs `/bin/sh`** + coreutils (`printf` etc.).  So: parsing/variable
eval works today; running recipes is the Phase 2 gate.

The build *is* bmake. The in-tree `bin/make` toy can't parse the tree's Makefiles.
Vendor NetBSD portable `bmake` (public-domain/BSD, ships a `boot-strap`) into
`contrib/bmake/`, **cross-build it** (host → uBixOS) so it's available before
Clang lands, point it at `/bin/sh`. Milestone: `bmake` runs a real tree Makefile
(`bin/hello/Makefile`) on-device. (See the bmake section for dependencies.)

### Phase 2 — POSIX `/bin/sh` + coreutils — 🟡 sh DONE; recipe exec blocked on one kernel bug
**`/bin/sh` DONE (2026-06-22):** ported **oksh** (portable OpenBSD ksh,
public-domain) as `tools/ports/sh/` via `share/mk/ports.mk` — cross-builds both arches,
runs in UbixOS, builtins **and external programs** work. (`bin/shell`/tcsh are not
POSIX `sh`; oksh is OpenBSD's `/bin/sh`.) Two kernel/fs bugs surfaced + fixed
porting it:
- **vfork ABI fix (committed):** musl's `vfork.s` (both arches) hardcoded the
  *Linux* clone syscall number (aarch64 `220`, x86_64 `58`), colliding with the
  FreeBSD ABI's `__semctl`/`readlink`. Aliased `vfork`→`fork` (COW makes it cheap).
- **exec-bit stat fix (committed):** the UbixFS pool stores world binaries as
  `0644`; shells that pre-check `access()`+`stat X_OK` (oksh) refused to exec them.
  `sys/fs/vfs/stat.c` now reports regular files executable (stopgap until the image
  tools store real `0755` / the security model lands).

**🔴 Blocker — bmake recipe execution:** `bmake -r -V` parses + evaluates, but
running a recipe/`!=` via `/bin/sh` hits a **signal-delivery-after-COW-fork
SIGSEGV**: bmake's `/bin/sh` child runs + exits cleanly, then the kernel
instruction-aborts delivering `SIGCHLD` to bmake's handler. Handed to the SMP
agent (`[ls/smp]` — it's their signal/sched/COW area) with a full repro; resume
recipe verification once fixed. (oksh's own fork+exec+wait works — it reaps via
SIG_DFL — so it's specific to delivering to a userland *handler* after COW fork.)

**Still TODO:** coreutils gap-fill — `rm`/`mv`/`test`/`[`/`sed`/`install`/`cmp`/
`printf` (recipes that call external tools need these once the SIGCHLD bug clears).
Also the file-backed-mmap SIGABRT from Phase 1 (re-enables bmake's `HAVE_MMAP`).

### Phase 3 — Stage 0: cross-build Clang + lld for uBixOS
**LLVM is acquired as a *port*, not vendored** — a pinned tarball + checksum +
committed patch series + a gitignored cache, per
[`third-party-ports-plan.md`](third-party-ports-plan.md) (LLVM is its worked
example: `tools/ports/llvm/`). Direct-committing ~1 GB of LLVM source into the
repo is explicitly rejected there.

On the host, build a **static** Clang + lld + `llvm-ar`/`ranlib`/`objcopy`, X86
*and* AArch64 backends, **JIT disabled** (no MCJIT/ORC — saves size + exec-page
mmaps), via the port's `ubixos.cmake` toolchain file (sysroot/includes/linker
flags). Cross-build **compiler-rt** for the target. Install to `/usr/bin/clang`,
`/usr/bin/ld.lld`, etc. `mkimage` stages the patched source to `/usr/src/llvm` for
the Stage-1 build. Goal: on-device `clang hello.c -o hello -fuse-ld=lld` runs.

### Phase 4 — Stage 1: first native Clang build in-OS
`bmake` + Stage-0 Clang build Clang+lld from `/usr/src/llvm`. Expect: memory
pressure, multi-hour builds (stage the sub-projects), and POSIX-header gaps that
surface and get fixed here.

### Phase 5 — Stage 2: verification (reproducible self-host)
Stage-1 Clang rebuilds Clang; **Stage 1 == Stage 2** (hash) ⇒ reproducibly
self-hosted. A diff localizes any codegen divergence.

### Phase 6 — Rebuild the world in-OS
`bmake world` over `/usr/src` with the native Clang/lld toolchain — relink one app
(`bin/hello`) → `bin/ls` → the C++ apps (`views`/`objgfx`) → musl + libcxx. Settle
the libc/libc++/compiler-rt "rebuild vs ship-prebuilt" question here.

### Phase 7 — Rebuild the kernel in-OS (the end-state)
`bmake kernel` with native Clang + **lld** driving `ldscript.${ARCH}`. This is the
payoff lld buys over TCC: an on-device kernel rebuild. Target the primary arch
(aarch64) once Stage 2 holds there.

---

## Arch ordering (recommendation)

**Self-host aarch64 first**, x86_64 second. Rationale: aarch64 is the **primary
forward target**; on the Apple-Silicon dev box **HVF runs aarch64 near-native**, so
the many-hour LLVM builds are tolerable where TCG x86_64 would be brutal; and
aarch64 **already lifted its RAM cap** (the DTB `/memory` work) to the GB-class
LLVM needs. x86_64 follows as the parity/validation arch (and is where the
device-model abstractions get re-proven), once its VMM RAM ceiling is raised to
match. *Defensible alternative:* x86_64 first, because LLVM's X86 backend is the
most battle-tested and the i386→x86_64 path reuses the device model — pick this if
toolchain-bring-up risk worries you more than build speed.

---

## Porting bmake (user-requested keystone)

The tree is built by **BSD make**; the Makefiles use `.if`/`.else`, `.for`, `!=`,
`.CURDIR`, `.include`, `${MAKE}` recursion. `bin/make` (558-line "minimal
POSIX-subset make") cannot parse these.

- **Source**: NetBSD portable `bmake` (public-domain/BSD; designed to build on
  foreign hosts, ships `boot-strap`). As a new dependency it comes in as a **port**
  (`tools/ports/bmake/`) per [`third-party-ports-plan.md`](third-party-ports-plan.md),
  not a `contrib/` vendor. FreeBSD `usr.bin/make` is the alternative.
- **Dependencies** (all present or near): `dirent`/`getdents` ✅, `fork`/`execve`/
  `wait4` ✅, `mmap` ✅, `stat`/`getcwd`/`rename` ✅, `sigaction` ✅, plus a working
  `/bin/sh` (Phase 2). Soft spots: `utimes` (NOTIMP — bmake stats mtimes; Phase 0
  fixes), the hard-coded shell path (point at `/bin/sh`), and `.MAKE`/`.MAKEFLAGS`
  job plumbing.
- **Bootstrap ordering**: bmake can be **cross-built** (host) and shipped on the
  image *before* Stage 0 Clang, so the in-OS build system exists from the start;
  later it is rebuilt natively in Phase 6.

---

## Milestones & effort (revised, hobby pace)

| Phase | Deliverable | Est. |
|---|---|---|
| 0 | Foundation self-tests + `utimes`/`fchmod` + 4–8 GB image / FAT stress | 1–2 wk |
| 1 | **bmake** ported (cross-built), runs a real tree Makefile in-OS | 2–4 wk |
| 2 | POSIX `/bin/sh` + coreutils gap-fill | 1–3 wk |
| 3 | Stage 0 cross-built Clang + lld + compiler-rt (x86_64 + aarch64) | 4–8 wk |
| 4 | Stage 1 native Clang build in-OS | 4–8 wk |
| 5 | Stage 2 reproducibility | 1–2 wk |
| 6 | Rebuild the **world** in-OS (libc/libc++/compiler-rt) | 3–6 wk |
| 7 | Rebuild the **kernel** in-OS via lld | 2–4 wk |
| **Total** | | **~5–7 months at hobby pace** |

This is longer than a TCC userland would have been — the deliberate cost of a real
compiler that also gives C++, optimization, aarch64, and (via lld) the kernel.

---

## What this unlocks

- Rebuild the **entire OS** — world *and* kernel — from source on-device, no Mac.
- C++ on-device (`views`/`objgfx`/`term` build natively); `clang-format`/
  `clang-tidy` native.
- Port any Apache-2.0 / BSD-licensed software that builds with Clang.
- Foundation for a BSD-style ports/packages system.

---

## Open questions to resolve before committing engineering time

1. **Arch first:** aarch64 (primary, HVF-fast, RAM ready) vs x86_64 (most mature
   LLVM backend, reuses device model). Recommendation: aarch64.
2. **Runtime libs on-device:** rebuild musl + libcxx + compiler-rt with Clang, or
   ship prebuilt and only relink? (Rebuilding is the complete answer.)
3. **`/bin/sh`:** ✅ **RESOLVED (2026-06-22)** — ported **oksh** (`tools/ports/sh/`);
   `bin/shell`/tcsh are not POSIX `sh`. Runs in-OS.
4. **Build-time mitigation:** how much can be cross-built once (Stage 0) vs must be
   native? Stage sub-projects; consider distcc-style offload later.
5. **LLVM version + size trimming:** which LLVM release; X86+AArch64 only; JIT off;
   minimal projects (clang, lld) — to keep the on-device build tractable.

---

*v3 — 2026-06-20. Compiler decision: **Clang/LLVM + lld**, not TCC (per request).
One toolchain covers world *and* kernel (lld drives the kernel ldscript), targets
**aarch64 + x86_64** (i386 frozen). Supersedes the TCC-userland v2 and the
i386-only v1.*

*v3.1 — 2026-06-22. Phase 1 (**bmake**) + most of Phase 2 (**`/bin/sh`** = oksh
port) built and running in-OS; **vfork** + **exec-bit** kernel/fs fixes committed.
Phase 2 now gated on one delegated kernel bug (signal-delivery-after-COW-fork,
with `[ls/smp]`) before bmake runs recipes; coreutils gap-fill is the remaining
Phase 2 work.*
