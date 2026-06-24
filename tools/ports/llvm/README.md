# LLVM / Clang / lld — uBixOS port (self-hosting Phase 3)

The compiler half of self-hosting. Cross-builds a **static Clang + lld + LLVM
binutils** (`llvm-ar`/`ranlib`/`objcopy`/`nm`) that **run on uBixOS**, targeting
**aarch64 + x86_64**. Comes in as a **port** (pinned tarball + SHA + gitignored
`build/ports/` cache), per `docs/design/third-party-ports-plan.md` — LLVM is its
worked example; ~GB of source is not vendored into `contrib/`.

Pinned: **LLVM 18.1.8** (Apache-2.0 — permissive, BSD-compatible).

## Why Clang/lld
- A real optimizing compiler with full **C and C++** (so `views`/`objgfx`/`term`
  build on-device, not just C).
- Mature **x86_64 *and* AArch64** backends — uBixOS's primary forward target
  self-hosts directly (the decisive reason over TCC, which has no aarch64).
- **lld honors GNU linker scripts**, so it can drive the kernel's
  `sys/compile/ldscript.${ARCH}` — *kernel* self-host becomes reachable, no
  GPLv3 binutils.

## Layout
| File | Role |
|------|------|
| `Makefile` | port recipe (fetch+SHA via `mk/ports.mk`, then `do-build`) |
| `ubixos.cmake` | CMake toolchain file: target = uBixOS musl cross-gcc + sysroot |
| `build.sh` | host-tblgen → cross-build clang+lld orchestration |

## The build (HEAVY — deliberate, not part of `bmake world`)
```sh
bmake -C tools/ports/llvm              # aarch64 → build/aarch64/usr/bin/{clang,ld.lld,...}
bmake -C tools/ports/llvm TARGET=x86_64
```
Host prereqs: `cmake`, `ninja`, and a C++17 host compiler (for the tblgen phase).

**Two phases** (a CMake cross-build can't run target binaries, but LLVM runs
`tblgen` *at build time*):
1. **Host tblgen** — build `llvm-tblgen` + `clang-tblgen` for the build host.
2. **Cross build** — configure with `ubixos.cmake` + `-DLLVM_TABLEGEN`/
   `-DCLANG_TABLEGEN` pointing at the host tblgens; build static `clang`+`lld`.

## Known challenges (this is SCAFFOLDING — expect iteration)
- **C++ runtime.** LLVM is C++17. Built `-fno-exceptions -fno-rtti`
  (`LLVM_ENABLE_EH/RTTI=OFF`) because uBixOS's libc++ is currently
  exception-free — see the "full C++ runtime (exceptions/RTTI + libunwind)"
  question in the self-hosting plan. libc++ headers come from `contrib/libcxx`;
  whether a target libc++.a links cleanly is the first thing to find out.
- **tblgen bootstrap.** The host phase needs a working host LLVM build; version
  must match the cross tree (same tarball).
- **Capacity.** Multiple GB of objects; GB-class link RAM; *hours* under QEMU.
  Build sub-targets in stages; aarch64 under HVF on Apple Silicon is the fast
  path. The disk image must grow to 4–8 GB and the FAT/pool write path survive
  large files (see the plan's Capacity section).
- **No JIT.** MCJIT/ORC off — saves size + the exec-page `mmap` churn uBixOS
  avoids.

## Pinning the SHA
`PORT_SHA256` is the sha256 of the release tarball
(`shasum -a 256 build/ports/distfiles/llvm-project-18.1.8.src.tar.xz`); the
framework verifies it on every build.

## Roadmap (self-hosting plan Phases 3–7)
- **3 (here):** Stage-0 — host cross-builds a uBixOS Clang+lld. + compiler-rt.
- **4:** Stage-1 — that Clang rebuilds Clang *in uBixOS*.
- **5:** Stage-2 — Stage-1 == Stage-2 (reproducible self-host).
- **6:** rebuild the **world** in-OS with the native toolchain.
- **7:** rebuild the **kernel** in-OS via lld + `ldscript.${ARCH}`.
