# uBixOS Self-Hosting Plan

**Goal**: rebuild uBixOS — world first, kernel eventually — *from its own source,
inside uBixOS*, with no macOS/host cross-toolchain. "Self-hosting" here means the
whole build pipeline runs on-device: a **compiler**, an **assembler**, a
**linker**, **ar/ranlib**, **make**, a **shell**, and the **coreutils** that drive
a `bmake world` — plus the headers, libraries, and source already staged on the
image.

> **This is a multi-month research goal, not a sprint.** It is split into two
> tracks so there is a *reachable* near-term win (Track A, a TCC-built userland)
> ahead of the *production* end-state (Track B, a Clang/LLVM toolchain). Read the
> "What changed" and "Toolchain inventory" sections before picking up any phase —
> the v1 of this doc (a single Clang plan, 2026-05-22) was i386-only and is
> superseded by what follows.

---

## What changed since v1 (and why this was rewritten)

The original plan targeted only Clang/LLVM on i386. Three things invalidated that
framing:

1. **i386 is being frozen.** Per the x86_64 migration decision (both arches go
   64-bit/LP64, killing the ILP32/LP64 width tax) the forward 64-bit target is
   **x86_64**; i386 stays as the reference arch on `master` until x86_64 reaches
   desktop parity, then freezes on a release branch (the in-tree x86_64 port —
   `sys/arch/x86_64/`, the `bin/Makefile` x86_64 SUBDIRS — is already underway).
   **Self-hosting i386 is a dead end** — effort spent there does not carry forward.
2. **aarch64 is the primary forward target**, but our vendored **TCC 0.9.26 has no
   aarch64 backend** (`contrib/tcc/` ships `i386-gen.c`, `x86_64-gen.c`,
   `arm-gen.c` — *no* `arm64-gen.c`). So the cheap, in-tree compiler can self-host
   **x86_64** today but **not** aarch64 without a TCC upgrade or Clang.
3. **TCC is already in the tree and partly wired to run in-OS** — vendored at
   `contrib/tcc/` (v0.9.26, built by `bin/tcc/`), and `tools/mkimage.sh` already
   stages it to `/usr/bin/tcc` with its headers at `/lib/tcc/include`. The v1 plan
   never mentioned it. A TCC-built userland is *years* closer than a Clang one.

**Net arch decision:** self-host **x86_64 first** (TCC backend + migration
target), bring **aarch64** along via Clang (Track B) or a TCC-with-arm64 upgrade,
and treat **i386 as never-self-hosted** (frozen reference).

---

## Toolchain inventory — what self-hosting actually needs

Self-hosting is not "a compiler"; it is the whole pipeline `bmake world` shells
out to. Current state:

| Component | Have? | Gap / notes |
|---|---|---|
| **C compiler** | 🟡 TCC (i386 today; x86_64 backend exists) | TCC has a *built-in* assembler + linker, so it covers cc+as+ld for **userland** in one binary. No aarch64 backend in 0.9.26. |
| **Assembler** | ✅ via TCC | TCC assembles inline + `.s`. A standalone `as` is only needed if we keep GNU-style hand-written asm that TCC's asm can't parse. |
| **Linker (userland)** | 🟡 via TCC | TCC links PIE/`.so`/static for normal programs. Good enough to relink the world. |
| **Linker (kernel)** | ⬜ **hard gap** | The kernel links with a **custom linker script** (`sys/compile/ldscript.{x86_64,aarch64}`) + freestanding flags. TCC's linker can't drive a GNU `ld` script. Kernel self-host needs a real `ld` (binutils) **or** `lld` (Track B). |
| **ar / ranlib** | 🟡 via `tcc -ar` | TCC creates `.a` archives; ranlib is a no-op for TCC archives. Adequate for static libs. |
| **make** | ⬜ **only a toy** | `bin/make` is a 558-line "Minimal POSIX-subset make" — it cannot parse the tree's BSD Makefiles (`.if`/`.for`/`!=`/`.CURDIR`/`.include`). **Real `bmake` must be ported.** (User-requested; see the bmake section.) |
| **shell** | 🟡 `bin/shell` + tcsh | bmake shells every command out to a **POSIX `/bin/sh`** (and uses `sh -c` for `!=`). tcsh is *not* `sh`-compatible. Need a verified `/bin/sh`. |
| **coreutils** | 🟡 partial | `cp`/`cat`/`ls`/`mkdir`/`mount`/… exist; a `bmake world` also leans on `rm`, `mv`, `test`/`[`, `sed`/`echo`, `install`, `mkdir -p`, `cmp`. Gap-fill as the build surfaces them. |
| **headers / libs / src** | ✅ staged | `/usr/include`, `/usr/src` (full tree, no build artifacts), `/lib` (musl + libs) are installed on the image per the CLAUDE.md layout. musl's own headers + `/lib/tcc/include` cover the compiler's needs. |

**Reading of the table:** the **world** (userland) is self-hostable with **TCC +
bmake + sh** — no binutils required. The **kernel** is *not*, until there is a
linker that honors the ldscript. That split structures the two tracks.

---

## Verified foundation (re-checked against current code, 2026-06-20)

The v1 status matrix claimed Phases 1–4 "done"; here is what the syscall table
(`sys/posix/syscalls_posix.c`) and VMM actually provide today. Corrections from v1
are flagged.

| Capability | Status | Evidence / correction |
|---|---|---|
| Anonymous mmap | ✅ | via **`mmap2` slot 477** (`sys_mmap2`), which is what musl/i386 calls. ⚠️ v1 cited slot 197 — that entry ("old mmap") is `SYSCALL_INVALID`. Document the real slot. |
| munmap / mprotect | ✅ | `munmap` (73), `mprotect` (74), both `SYSCALL_VALID`. |
| File-backed mmap + shared page cache | ✅ | demand-paged; shared file-page cache landed with the aarch64 memory work (`vm_filecache`/`PTE_SHARED`). |
| fork / execve | ✅ | `fork` (2), `execve` (59). COW fork on both arches. |
| wait4 / waitpid + SIGCHLD + zombies | ✅ | `wait4` (7) `SYSCALL_VALID`; signal Phases 1–5 complete. |
| rename / getdirentries / getcwd | ✅ | `rename` (128), `getdirentries` (196), `__getcwd` (326). |
| symlink | ✅ (syscall) | `symlink` (57) wired — but the **ubixfs pool root has no symlinks**; install scripts that symlink may fail on the pool FS. |
| chmod | ✅ | `chmod` (15). |
| getrlimit / setrlimit | ✅ | (194 / 195) — compiler thread stack sizing. |
| sigaction (full) | ✅ | (416). |
| **utimes / futimes / lutimes** | 🟡 **NOTIMP** | (138 / 206 / 276) are `SYSCALL_NOTIMP`. make uses mtimes for dependency tracking — **a real gap for incremental builds**; first-build-from-clean is fine. |
| **fchmod** | 🟡 **NOTIMP** | (124) — `install`-setting-exec-bit may need it (plain `chmod` by path works). |

**Verdict:** Phases 1–3 hold; **Phase 4 is partial, not done** — `utimes`/`fchmod`
are the concrete near-term syscall work. A clean in-OS self-test suite (below, A0)
should be written rather than trusting the matrix.

---

## Track A — TCC-built userland self-host (x86_64) · *near-term, reachable*

The achievable milestone: **uBixOS rebuilds its own world** with an on-device
compiler + make + shell. No binutils, no Clang. Target **x86_64** (TCC backend +
migration arch).

### A0 — Verify the foundation in-OS
Write `selfhost/test_*.c` run on-device: anon + file-backed `mmap`; `fork`+`exit`
→ `wait4` status decode; `getdents`/`rename`/`getcwd`/`chmod`; `getrlimit`. Green
= the matrix above is real. Surfaces the `utimes`/`fchmod` gaps as failing cases
to fix.

### A1 — TCC compiles + runs a C program in-OS
Bring up `bin/tcc` for **x86_64** (`-DTCC_TARGET_X86_64`), stage to `/usr/bin/tcc`
+ `/lib/tcc/include` (mkimage already does this for i386 — mirror for x86_64).
Goal: `tcc hello.c -o hello && ./hello` works on-device. The i386 path already
needed the `R_386_GOT32X` relocation patch (`contrib/tcc/tccelf.c`); expect the
x86_64 path to surface its own relocation/PIE quirks.

### A2 — TCC self-compiles (the self-hosting C-compiler win)
`tcc -o tcc2 contrib/tcc/tcc.c …` **inside uBixOS**, then `tcc2 hello.c` works, and
ideally `tcc2` rebuilds `tcc3` byte-identically. This is the first true
self-hosting result: a C compiler that reproduces itself on-device.

### A3 — Port **bmake** (the keystone; user-requested)
See the dedicated section below. Without a real make, "rebuild the world" is
hand-running compiler invocations. bmake is the piece that makes Track A a
*system* rather than a demo.

### A4 — POSIX `/bin/sh` + coreutils gap-fill
bmake executes recipes via `sh -c`. Provide a real `/bin/sh` (verify `bin/shell`
is sh-compatible enough, or vendor a small POSIX sh — e.g. a `dash`/`oksh` subset).
Gap-fill coreutils (`rm`, `mv`, `test`, `sed`, `install`, `mkdir -p`, `cmp`) as the
build demands them.

### A5 — Rebuild the **world** in-OS
`bmake world` over `/usr/src` using `/usr/bin/tcc` + `/usr/include` + `/lib`. The
load-bearing question is **libc**: can TCC rebuild musl (or do we ship musl `.a`/
`.so` prebuilt and only relink apps)? Stage incrementally — relink one simple app
(`bin/hello`), then `bin/ls`, then a libc rebuild.

### A6 — Reproducibility
A world rebuilt in-OS should match the cross-built world (modulo timestamps). Diff
the binaries to flag codegen divergence.

**Track A explicitly stops at the world.** The kernel needs Track B (or a binutils
`ld` port) — see the barrier section.

---

## Track B — Clang/LLVM bootstrap · *long-term, production*

The v1 three-stage Clang plan, retained as the *production* end-state. What Clang
buys over TCC: an **optimizing** compiler, **C++** (so `views`/`objgfx`/`term`
build on-device), **aarch64** codegen (TCC 0.9.26 can't), and **`lld`** — a linker
that *does* honor linker scripts, which is the thing that unlocks **in-OS kernel
self-host**.

```
Stage 0  x86_64 host cross-compiles Clang+lld → runs on uBixOS (static, X86 backend only)
Stage 1  uBixOS runs Stage 0 clang to compile clang from /usr/src
Stage 2  uBixOS runs Stage 1 clang to compile clang again;  Stage1 == Stage2  → self-hosted ✓
```

Unchanged in spirit from v1, but re-scoped: **x86_64 first**, then add the AArch64
backend for the primary target. Pain points remain memory pressure (LLVM wants
GB-class RAM — the DTB RAM-cap lift helps on aarch64; the x86_64 VMM must match)
and multi-hour QEMU build times (build `clang-tblgen` → `libLLVM` → `clang` in
stages). The disk-capacity work (old Phase 5: 4–8 GB image, FAT write-path stress
on large files / deep trees) is a **Track B prerequisite** — LLVM source is
~800 MB and a build is multi-GB.

---

## The kernel-rebuild barrier (read before promising "self-hosting")

`bmake kernel` links with `${ld} -T sys/compile/ldscript.${ARCH}` — a **custom GNU
linker script** placing the kernel at a fixed load address with explicit section
ordering, plus freestanding/`-nostdlib` flags. **TCC's built-in linker cannot
drive a linker script**, so Track A (TCC) **cannot relink the kernel**. Options:

- **B-track `lld`** — honors linker scripts; the clean, license-compatible answer
  (Apache-2.0). Preferred. Makes kernel self-host fall out of Track B.
- **Port GNU `ld`/binutils** — works and is well-trodden, but binutils is GPLv3;
  as a *build tool* (not linked into BSD-licensed output) it is arguably fine, the
  same way the host cross-`ld` is used today — but it muddies the "no-copyleft
  toolchain" goal the Clang choice was made to preserve. Treat as a fallback.
- **Teach TCC minimal ldscript support** — a research spike; likely more work than
  it's worth versus lld.

**Therefore:** *world* self-host = Track A (reachable). *Kernel* self-host = Track
B's lld (or a binutils fallback). Don't conflate the two when reporting progress.

---

## Porting bmake (user-requested keystone)

The tree is built by **BSD make (bmake)** — the Makefiles use `.if`/`.else`,
`.for`, `!=` shell assignment, `.CURDIR`, `.include`, `${MAKE}` recursion. The
in-tree `bin/make` (558-line "minimal POSIX-subset make") **cannot** parse these.
A real bmake is non-negotiable for in-OS builds.

- **Source**: vendor NetBSD `bmake` (the portable distribution, public-domain /
  BSD) or FreeBSD `usr.bin/make` into `contrib/bmake/`. NetBSD's portable bmake is
  designed to build on foreign hosts and ships a `boot-strap` script — a good fit.
- **Dependencies** (all present or near): `dirent`/`getdents` ✅, `fork`/`execve`/
  `wait4` ✅, `mmap` ✅, `stat`/`getcwd`/`rename` ✅, `sigaction` ✅, and a working
  `/bin/sh` (A4). The known soft spots: `utimes` (NOTIMP — bmake stats mtimes; A0
  flags this), job-control `.MAKE`/`.MAKEFLAGS` plumbing, and the hard-coded shell
  path (point it at our `/bin/sh`).
- **Build it with TCC** (A1) against musl → `/usr/bin/bmake`. First milestone:
  `bmake` parses and runs a *real* tree Makefile (e.g. `bin/hello/Makefile`)
  on-device.
- bmake is the gate for A5 (rebuild the world) — sequence it right after A2
  (a self-hosting compiler to build bmake with) and A4 (a shell for it to drive).

---

## Milestones & effort (revised, hobby pace)

| Track | Milestone | Est. |
|---|---|---|
| A0 | In-OS foundation self-tests (+ fix `utimes`/`fchmod`) | ~1 wk |
| A1 | TCC compiles + runs a program in-OS (x86_64) | 1–2 wk |
| A2 | TCC self-compiles reproducibly | 1–2 wk |
| A3 | **bmake** ported, runs a real tree Makefile | 2–4 wk |
| A4 | POSIX `/bin/sh` + coreutils gap-fill | 1–3 wk |
| A5 | Rebuild the **world** in-OS (incl. libc question) | 3–6 wk |
| A6 | World reproducibility | 1 wk |
| — | **Track A subtotal (userland self-host)** | **~2–4 months** |
| B5 | 4–8 GB image + FAT write-path stress | 1 wk |
| B6 | Stage 0 cross-built Clang+lld (x86_64) | 3–6 wk |
| B7 | Stage 1 native Clang build in-OS | 4–8 wk |
| B8 | Stage 2 verification + **kernel self-host via lld** | 2–4 wk |
| — | **Track B subtotal (production + kernel)** | **~3–5 months** |

Track A is the recommended near-term focus — it produces a genuinely
self-hosting *userland* with in-tree-only software (TCC + a vendored bmake), and
every step is QEMU-verifiable on a few-hundred-MB image. Track B is the larger,
later investment that brings C++, optimization, aarch64, and the kernel.

---

## What this unlocks

- **Track A:** rebuild and hack on the **userland** entirely on-device — edit
  `/usr/src`, `bmake`, run. A real "the OS can grow itself" milestone.
- **Track B:** an optimizing, C++-capable, multi-arch toolchain; `clang-format`/
  `clang-tidy` natively; on-device kernel rebuilds (via lld); the foundation for a
  BSD-style ports/packages system.

---

## Open questions to resolve before committing engineering time

1. **libc on-device (A5):** rebuild musl with TCC, or ship musl prebuilt and only
   relink apps? (Rebuilding musl with TCC is the harder, more complete answer.)
2. **`/bin/sh` (A4):** is `bin/shell` POSIX-`sh` enough for bmake, or vendor a
   small sh? Decide before A3 lands (bmake needs it to run recipes).
3. **aarch64 compiler:** upgrade TCC to a version with `arm64-gen.c` for a TCC
   aarch64 self-host, or wait for Track B's Clang? (TCC-upgrade is cheaper but
   off the vendored 0.9.26.)
4. **Kernel linker:** commit to lld (Track B, license-clean) vs a binutils `ld`
   fallback (faster, GPLv3 build tool). Affects when kernel self-host is reachable.

---

*v2 — 2026-06-20. Supersedes the i386-only Clang-only v1 (2026-05-22). Scope
broadened to the full toolchain (compiler + bmake + sh + linker), re-targeted to
x86_64/aarch64, split into a reachable TCC userland track and a production Clang
track.*
