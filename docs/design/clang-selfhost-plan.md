# UbixOS Self-Hosting: Clang Bootstrap Plan

**Goal**: UbixOS builds its own Clang toolchain natively, without a Mac cross-compiler.

**License rationale**: Clang/LLVM (Apache 2.0) is permissive and compatible with UbixOS's
BSD license. GCC (GPL v3) is not — the copyleft would contaminate the toolchain. FreeBSD,
OpenBSD, and macOS all made this same choice.

---

## Status Matrix

Legend: ✅ done & verified · 🟡 partial · ⬜ not started

| Phase | Item | Status | Notes |
|-------|------|--------|-------|
| 1 | Anonymous mmap + munmap + mprotect | ✅ | demand-zero VMAs (`vmm-plan` P1/2); `mprotect` real (`gen_calls.c:185`) |
| 2 | File-backed mmap + FAT random-access | ✅ | demand-paged + shared file-page cache (`vmm-plan` 2.2) |
| 3 | waitpid + SIGCHLD + zombie reaping | ✅ | signal Phases 1–5 complete |
| 4 | Supporting POSIX syscalls | ✅ | the build-tool syscall surface |
| 5 | Disk + filesystem capacity (4 GB image, FAT write stress) | ⬜ | |
| 6 | Stage 0 — cross-compile Clang for UbixOS on macOS | ⬜ | |
| 7 | Stage 1 — first native Clang build inside UbixOS | ⬜ | |
| 8 | Stage 2 — verification (Stage 1 vs Stage 2 output) | ⬜ | |

(Per-phase effort estimates are in the Milestone Summary at the bottom.)

---

## The Three-Stage Bootstrap

```
Stage 0  macOS cross-compile → clang binary that runs on UbixOS
Stage 1  UbixOS runs Stage 0 clang to compile clang from /usr/src
Stage 2  UbixOS runs Stage 1 clang to compile clang again
         Stage 1 == Stage 2  →  reproducibly self-hosted ✓
```

After Stage 2 the Mac cross-compiler is never needed for Clang again.

---

## Phase 1 — Anonymous mmap

**Blocks**: everything. Clang's internal allocator, musl's malloc, jemalloc all bottom out
on anonymous mmap. Nothing meaningful runs without it.

### Kernel work (`sys/vmm/`)

- `sys_mmap` POSIX syscall (FreeBSD slot 197):
  - `MAP_ANON | MAP_PRIVATE`: allocate zeroed pages from the free pool, wire them into the
    calling process's address space at a kernel-chosen VA (or hint if provided).
  - `MAP_FIXED`: map at the exact requested VA, replacing any existing mapping.
  - Return the mapped VA on success, `MAP_FAILED` on error.
- `sys_munmap` (slot 73): unmap the range, free pages if COW refcount reaches zero.
- `sys_mprotect` (slot 74): update PTE permission bits for a range (needed by Clang's JIT
  and by musl's thread-local storage setup).
- `sys_mmap2` (Linux-compat slot 192): same as mmap but offset is in pages not bytes —
  musl uses this on i386.

### VM bookkeeping

- Add a `vm_map` structure to `taskStruct` — a sorted list of `vm_region` entries
  (start, end, prot, flags, backing).  Currently UbixOS tracks nothing about what a process
  has mapped; munmap and mprotect need this.
- `vm_region` entries are allocated with `kmalloc`, freed on munmap and process exit.
- `vmm_cleanVirtualSpace` (called on exit/execve) must walk the vm_map and free all
  anonymous pages before tearing down the page directory.

### musl wiring

- `contrib/musl/arch/i386/bits/syscall.h.in`: map `__NR_mmap2` → 192, `__NR_munmap` → 73,
  `__NR_mprotect` → 74.
- musl's `mmap.c` already uses `mmap2` on i386 — no musl changes needed once the syscall
  numbers are correct.

### Test

```c
// selfhost/test_mmap.c — run on UbixOS
void *p = mmap(NULL, 4096, PROT_READ|PROT_WRITE,
               MAP_ANON|MAP_PRIVATE, -1, 0);
assert(p != MAP_FAILED);
memset(p, 0xAB, 4096);
assert(((char*)p)[0] == (char)0xAB);
munmap(p, 4096);
```

---

## Phase 2 — File-backed mmap

**Blocks**: ELF dynamic loading (ld.so), large source file processing, shared libraries.
Clang mmaps its own ELF sections and header files during compilation.

### Kernel work

- Extend `vm_region` to carry a backing `fileDescriptor_t *` and byte offset.
- Page fault handler (`sys/vmm/pagefault.c`): on access to an unmapped page in a
  file-backed region, read the appropriate page from the file into a fresh physical frame,
  map it into the PTE, return.
- Write-back / `msync` (slot 26): flush dirty file-backed pages back to the VFS.  For the
  bootstrap this can be a no-op (MAP_PRIVATE copy-on-write is sufficient for read-only
  mmap of object files).
- `MAP_PRIVATE` file mapping: copy-on-write — writes do not propagate to the file.

### VFS integration

- `vfsRead` must support an `off_t` offset parameter (already does per the `procfs` fix
  from earlier).  File-backed fault handler calls `vfsRead(fd, buf, PAGE_SIZE, page_offset)`.
- FAT driver (`sys/fs/fat/`) must support random-access reads at arbitrary offsets —
  verify `fat_file_read` handles non-sequential access correctly.

### Test

```c
int fd = open("sys:/usr/src/hello.c", O_RDONLY);
void *p = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
assert(p != MAP_FAILED);
assert(((char*)p)[0] == '/');   // first char of a C comment
munmap(p, 4096);
close(fd);
```

---

## Phase 3 — waitpid and SIGCHLD

**Blocks**: the Clang driver (`clang` binary) forks child processes for each compilation
and linking step (cc1, ld, as). Without waitpid these children become zombies and the
driver hangs.

### Kernel work (`sys/kernel/`)

- `sys_wait4` (FreeBSD slot 7, also covers `waitpid`):
  - Block the calling thread until any child (pid=-1) or a specific child exits.
  - Fill the `status` out-parameter with the exit code packed per POSIX `W*` macros.
  - Remove the zombie entry from the process table.
- SIGCHLD delivery: when a child calls `sys_exit`, post `SIGCHLD` to the parent.  The
  signal plan (Phase 2 completed) covers signal posting — wire it up in `sys_exit`.
- Zombie state: `sys_exit` must leave the `taskStruct` in a zombie state (status saved,
  resources freed, not yet reaped) until the parent calls `wait4`.

### musl wiring

- `__NR_wait4` → FreeBSD slot 7 (already the correct Linux number on i386, verify).

### Test

```c
pid_t pid = fork();
if (pid == 0) { exit(42); }
int status;
waitpid(pid, &status, 0);
assert(WEXITSTATUS(status) == 42);
```

---

## Phase 4 — Supporting POSIX syscalls

Small gaps that configure scripts and compiler drivers exercise.

| Syscall | FreeBSD slot | Notes |
|---|---|---|
| `rename` | 128 | atomic output file replacement |
| `readdir` / `getdents` | 196 | header search path traversal |
| `getcwd` | 326 | build system cwd tracking |
| `symlink` | 57 | some install scripts need it |
| `chmod` | 15 | setting executable bit on output |
| `utime` / `utimes` | 88 / 138 | make dependency timestamps |
| `getrlimit` / `setrlimit` | 194 / 195 | stack size for compiler threads |
| `sigaction` full | — | SIGCHLD, SIGSEGV in compiler |

Most of these are small (< 20 lines) and follow the same pattern as the syscalls already
added (`dup`, `setsid`, `getrusage`).

---

## Phase 5 — Disk and filesystem capacity

LLVM source is ~800 MB; a full build produces ~2 GB of objects before stripping.

- **Grow the disk image**: change `tools/mkimage.sh` to create a 4 GB or 8 GB FAT32
  image.  `bmake image` already calls this script; update the `dd` size parameter.
- **FAT32 write path**: verify `fat_file_write` handles files > 512 KB and correctly
  extends the FAT chain.  This is the most likely bug to surface during a real build.
- **Long filename support**: LLVM source paths exceed 8.3 — LFN is already in the FAT
  driver but needs stress testing with deep `llvm/` directory trees.

---

## Phase 6 — Stage 0: Cross-compile Clang for UbixOS on macOS

Build a Clang binary on the Mac that produces i386 UbixOS executables and that itself
runs on UbixOS.

```sh
# On macOS — approximate, target triple TBD
cmake -S llvm -B build-ubixos \
  -DCMAKE_CROSSCOMPILING=ON \
  -DLLVM_TARGET_ARCH=X86 \
  -DLLVM_TARGETS_TO_BUILD=X86 \
  -DLLVM_DEFAULT_TARGET_TRIPLE=i386-ubixos \
  -DCMAKE_SYSTEM_NAME=UbixOS \
  -DCMAKE_C_COMPILER=x86_64-elf-gcc \
  -DCMAKE_C_FLAGS="-m32 -nostdinc ..." \
  -DCMAKE_EXE_LINKER_FLAGS="..." \
  -DLLVM_ENABLE_PROJECTS="clang" \
  -DLLVM_BUILD_STATIC=ON

make -C build-ubixos clang -j$(nproc)
mcopy -o -i ubixos.img@@1M build-ubixos/bin/clang ::/bin/clang
```

Key decisions:
- **Static binary**: avoid dynamic linking complexity until ld.so is solid.
- **Minimal targets**: build only the X86 backend — reduces binary size by ~60%.
- **No LLVM JIT**: disable MCJIT/ORC — saves 30 MB and avoids mmap-executable pages.

A UbixOS-specific CMake toolchain file (`tools/ubixos.cmake`) should encode the sysroot,
include paths, and linker flags so the build is reproducible.

---

## Phase 7 — Stage 1: First native Clang build inside UbixOS

Boot UbixOS with the Stage 0 Clang installed. From the shell:

```sh
# Inside UbixOS
cd /usr/src/llvm
cmake -S llvm -B /tmp/build \
  -DCMAKE_C_COMPILER=/bin/clang \
  -DLLVM_TARGETS_TO_BUILD=X86 \
  -DLLVM_ENABLE_PROJECTS="clang" \
  -DLLVM_BUILD_STATIC=ON
make -C /tmp/build clang
```

Expected pain points:
- **Memory pressure**: LLVM compilation is RAM-hungry. QEMU `-m 512` or `-m 1024` may be
  needed. Add swap if the VMM supports it, or ensure Clang's arena allocator stays within
  physical RAM via `ulimit`.
- **Compile time**: a full LLVM build on an i386 at QEMU speed will take many hours.
  Build one sub-project at a time (`clang-tblgen` first, then `libLLVM`, then `clang`).
- **Missing headers**: any POSIX header gaps will surface here. Fix and rebuild as needed.

---

## Phase 8 — Stage 2: Verification

```sh
# Inside UbixOS, using Stage 1 clang
make -C /tmp/build2 clang   # built with Stage 1 clang

# Compare Stage 1 and Stage 2 outputs
sha256sum /tmp/build/bin/clang /tmp/build2/bin/clang
```

If the hashes match: **UbixOS is self-hosting**. The Mac cross-compiler is no longer
needed for the toolchain.

If they differ: there is a code-generation bug in Stage 0 or Stage 1. Use the diff to
narrow down which translation unit diverges.

---

## Milestone Summary

| Phase | Deliverable | Est. effort |
|---|---|---|
| 1 | Anonymous mmap + munmap + mprotect | 2–3 weeks |
| 2 | File-backed mmap, FAT random-access | 2–4 weeks |
| 3 | waitpid + SIGCHLD + zombie reaping | 1–2 weeks |
| 4 | Supporting POSIX syscalls | 1–2 weeks |
| 5 | 4 GB image, FAT write stress | 1 week |
| 6 | Stage 0 cross-compiled Clang | 2–4 weeks |
| 7 | Stage 1 native build inside UbixOS | 2–4 weeks |
| 8 | Stage 2 verification | 1 week |
| **Total** | | **~4–5 months at hobby pace** |

---

## What This Unlocks

Once self-hosting:
- Rebuild the entire OS from source inside UbixOS — no Mac required.
- Port any Apache-2.0 or BSD-licensed software that builds with Clang.
- Use `clang-format`, `clang-tidy`, and the LLVM toolchain natively for OS development.
- The C++ apps (views, term, objgfx) can be compiled on-device.
- Foundation for eventually porting a BSD ports/packages system.

---

*Last updated: 2026-05-22*
