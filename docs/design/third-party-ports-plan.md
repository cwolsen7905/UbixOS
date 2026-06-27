# Third-Party Dependency Policy — Ports vs Vendor

**Decision (2026-06-20):** going forward, a **ports-style recipe** is the
**default** way uBixOS pulls in third-party source — a pinned version + checksum +
a small committed patch series + a gitignored extraction cache, fetched and
patched by a reusable make framework. **Direct vendoring** (committing upstream
source into `contrib/`) remains an explicit, *documented* exception for a narrow
class of deps. Existing vendored deps are **not** retrofitted.

> Scope note: "ports" here means *build-time recipes for the deps the OS builds
> from* (toolchain, libraries) — not a user-facing package manager. An on-device
> `pkg`-style system is a separate, much later thing.

---

## Why ports-by-default

- **Lean repo.** No more committing large upstreams into our history forever. The
  current convention (vendor everything into `contrib/`) was fine up to **libcxx
  at 72 MB**; it does not scale — LLVM alone is ~10× all of `contrib/` combined,
  and git history never forgets a committed blob.
- **Explicit pins + reproducibility.** A tag + SHA-256 is the single source of
  truth for *what version we build*; a clone can't silently drift.
- **Isolated, reviewable patches.** Our changes live as `*.patch` files we can
  diff against pristine upstream — unlike today's baked-in edits (e.g. the TCC
  `R_386_GOT32X` change lives *inside* `contrib/tcc/tccelf.c`, indistinguishable
  from upstream without archaeology).
- **Cheap updates.** Bump two lines (version + checksum), refresh patches, rebuild
  — one small commit, no giant vendor diff to review.
- **It's the proven shape.** FreeBSD's `devel/llvm*`, pkgsrc, Gentoo, Homebrew —
  none vendor LLVM into base; all carry a versioned recipe + patches.

## When direct-vendor is still correct (the exception)

Default to a port; a dep may stay/become **vendored** only with a stated reason,
from this narrow set:

1. **Foundational, ABI-coupled, must-build-offline base** — you cannot sanely
   *fetch* your libc at build time. **musl stays vendored.** (libcxx/libcxxabi are
   borderline but stay vendored as the C++ base.)
2. **Tiny / header-only / single-file** libs where a fetch+checksum+patch recipe is
   more machinery than the dependency itself (e.g. `stb_image`).
3. **Heavily forked** code where *our copy is effectively the source* and there is
   no meaningful upstream cadence to track.

The rule: **default to a port; justify a vendor exception in the recipe README.**
Size and coupling pick the model, not habit.

## Do NOT retrofit existing deps

`musl`, `libcxx`, `lwip-2.0.3`, `tcc`, `netsurf*`, `bearssl`, `tcsh`,
`doomgeneric`, `libdom`, `libcss`, `libutf8proc`, `gdtoa`, `tzcode` stay vendored
as-is — they build, their patches are baked in, and re-importing them as ports is
risk for no benefit. Migrate one to a port **only** when it independently needs a
version bump and the conversion pays for itself.

---

## The framework (lightweight, reusable)

```
share/mk/ports.mk                 # reusable fetch -> verify -> extract -> patch macro
tools/ports/<name>/
    port.mk                 # the pin + build knobs (committed)
    patches/*.patch         # KB-scale, committed
    README.md               # what this is, why a port, any vendor-exception note
build/ports/<name>-<ver>/   # extraction + build cache  (GITIGNORED)
```

- **`share/mk/ports.mk`** takes `PORT_NAME`, `PORT_VERSION`, `PORT_URL`, `PORT_SHA256`
  and a build hook. It: downloads the tarball to a cache, **verifies the SHA-256**
  (hard-fail on mismatch — no silent drift), extracts into
  `build/ports/<name>-<ver>/`, applies `tools/ports/<name>/patches/*.patch` in
  order, then runs the port's build rule. Idempotent: a present, verified, patched
  tree is skipped.
- **`tools/ports/<name>/port.mk`** is the recipe — the version pin, the source
  URL, the checksum, and any per-port CMake/configure knobs. *This* is what you
  edit to bump a version.
- **Cache lives under `build/`** (already gitignored + arch-homed), so nothing
  fetched is ever committed.
- **`mkimage` staging:** when the OS needs a port's *source* on-device (e.g. the
  self-hosting Stage-1 build needs `/usr/src/llvm`), `mkimage` copies the patched
  tree out of the cache into the pool image. The same recipe feeds both the host
  cross-build and the on-device build.
- **Offline note:** the first build of a port needs network once; afterwards the
  verified cache is as local as a vendored copy. Pin URLs to stable release
  artifacts (GitHub release tarballs), and allow a `PORT_DISTDIR` override so a
  pre-seeded tarball cache works air-gapped.

### Version-bump procedure (the "update system")

1. Edit `tools/ports/<name>/port.mk`: new `PORT_VERSION` + `PORT_SHA256`.
2. Re-run the port build; fix any patch fuzz (re-roll a `*.patch` if upstream moved
   a hunk).
3. Rebuild + test.
4. Commit: the recipe delta + refreshed patches only — typically a few KB.

---

## Worked example: LLVM / Clang (the trigger for this policy)

This is the self-hosting toolchain ([`self-hosting-plan.md`](self-hosting-plan.md)
Track/Phase 3). It is the canonical large port.

```
tools/ports/llvm/
    port.mk     # PORT_VERSION=llvmorg-XX.X.X  PORT_SHA256=...
                # PORT_URL=.../llvm-project-XX.X.X.src.tar.xz
                # build knobs: LLVM_ENABLE_PROJECTS="clang;lld"
                #              LLVM_TARGETS_TO_BUILD="X86;AArch64"
                #              compiler-rt; JIT off; static
    patches/    # ubixos target triple, sysroot, freestanding/-nostdlib glue
    ubixos.cmake# cross toolchain file (sysroot / includes / lld linker flags)
    README.md
build/ports/llvm-XX.X.X/    # extracted + patched monorepo + the build (gitignored)
```

- One pinned monorepo tarball; CMake selects only `clang`+`lld` and the
  `X86`+`AArch64` backends (keeps the on-device build tractable).
- `mkimage` stages the patched tree to **`/usr/src/llvm`** on the pool for Stage 1.
- Updating LLVM = bump `PORT_VERSION`/`PORT_SHA256` + refresh patches. Nothing else
  in the tree changes.

---

## .gitignore

```
/build/ports/
```
(plus the usual `build/` rule if not already broad). Recipes (`tools/ports/**`)
and patches are committed; fetched/extracted source never is.

---

*Design-only, 2026-06-20. No fetch/build machinery written yet — this locks the
convention so `share/mk/ports.mk` + `tools/ports/llvm/` can be built to spec. Companion
to `self-hosting-plan.md` (LLVM is the first port).*
