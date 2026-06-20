# UbixOS Dynamic Linking Plan

**Goal**: Build musl as `libc.so`, install it to the image, and link all world binaries
dynamically so processes share a single copy of libc in physical RAM rather than each
carrying their own 500 KB+ static copy.

**Memory benefit**: with 10 processes running, dynamic linking saves roughly 10 × 400 KB =
4 MB of physical RAM — significant on a 256 MB QEMU machine, critical for self-hosting.

> ⚠️ **KNOWN GAP (2026-06-03): the physical-RAM sharing above is NOT yet
> realized.** File-backed `mmap` (`sys/vmm/vmm_mmap.c`) allocates *fresh private
> pages per process and `fread`s the file into them* — there is no page cache or
> shared file-backed mapping, and `MAP_SHARED`/`MAP_PRIVATE` are ignored. So each
> process linking `libc.so` gets its own physical copy; RAM use is ~the same as
> static linking. What IS delivered: smaller on-disk binaries (~50 KB vs ~800 KB)
> and the full rtld/interp/auxv machinery (the self-hosting prerequisite).
> Realizing the RAM benefit needs a **unified page cache with shared read-only
> file mappings** (libc text/rodata shared across all mappers; only the data/GOT
> segment COW-private). Tracked as future VMM work — see `vmm-plan.md`. This plan
> is archived as "dynamic linking works"; the *memory-sharing* objective is
> explicitly deferred.

**Prerequisite for self-hosting**: Stage 0 Clang expects a shared libc by default.

---

## Current State

| Component | State |
|---|---|
| `sys_mmap` — anonymous, no hint | Works — uses `vmm_getFreeVirtualPage` |
| `sys_mmap` — anonymous, with hint | Works — maps at the given VA |
| `sys_mmap` — file-backed | Partial — reads entire file eagerly (not demand-paged), but correct |
| `sys_munmap` | ✅ Real impl — walks VA range, calls `vmm_unmapPage` |
| `mprotect` | ✅ Real impl — updates PTE flags via `vmm_setPageAttributes` |
| `mmap2` (slot 477, musl i386) | ✅ Wired — `sys_mmap2` converts page-offset to bytes |
| `ldEnable()` kernel loader | ✅ Fixed — translates POSIX paths, no double-relocation |
| `PT_INTERP` detection | Works — kernel reads interp path and calls `ldEnable()` |
| musl `libc.so` build | ✅ Built — 801 KB ELF32 DYN at `build/lib/libc.so` |
| `sys:/lib/libc.so` on image | ✅ Installed by `mkimage.sh` |
| `sys:/lib/ld-musl-i386.so.1` on image | ✅ Installed as second copy (FAT32, no symlinks) |
| Binary Makefiles | ✅ Dynamic by default — `clock` and `tcc` intentionally stay `-static` (Phase 6 exceptions) |

---

## Phase 1 — Wire `mmap2` ✅ DONE

musl on i386 always uses `mmap2` (syscall 192) rather than `mmap` (syscall 90/477).
`mmap2` is identical except the offset parameter is in 4096-byte pages, not bytes.

Added `sys_mmap2` wrapper in `sys/vmm/vmm_mmap.c` and wired slot 477 (musl's
`__NR_mmap2` override) in `sys/kernel/syscalls_posix.c`. The `contrib/musl/arch/i386/bits/syscall.h.in`
`__NR_mmap2` already mapped to 477 in the UbixOS-specific override.

**Test**: `malloc(1)` from a dynamically-linked binary should not segfault.

---

## Phase 2 — Implement `munmap` ✅ DONE

Replaced the no-op stub in `sys/vmm/vmm_mmap.c` with a real implementation that walks
the VA range and calls `vmm_unmapPage(va, VMM_FREE)` for each page. Without a `vm_map`
we cannot validate whether the range was actually mapped, but incorrect calls are silent
no-ops which is safe.

---

## Phase 3 — Implement `mprotect` ✅ DONE

Replaced the no-op stub in `sys/kernel/gen_calls.c` with a real implementation using
`vmm_setPageAttributes(va, flags)` to update PTE flags. Sets `PAGE_WRITE` if `PROT_WRITE`
is in the requested protection, otherwise `PAGE_PRESENT | PAGE_USER` (read-only).

---

## Phase 4 — Build musl as `libc.so` ✅ DONE

`build/obj/musl/` reconfigured without `--disable-shared` and with `--syslibdir=/lib
--libdir=/lib`. musl's Makefile link rule patched to use `$(LD) -m elf_i386 -shared`
(instead of `$(CC)`) so the output is `ET_DYN`, not `ET_EXEC`. `tools/libgcc32.c` provides
the missing i386 runtime helpers (`__udivdi3`, `__divdi3`, etc.) that `x86_64-elf-gcc -m32`
doesn't ship.

`build/lib/libc.so` — 801 KB ELF32 DYN, entry at `_dlstart`.

Top-level `Makefile` target renamed from `musl` to `musl-libc` to prevent bmake's
`MK_AUTO_OBJ` from redirecting to `build/obj/musl/` and trying to parse the GNU Makefile
there. Actual build is delegated to `/usr/bin/make -C build/obj/musl`.

---

## Phase 5 — Install `libc.so` to the Image ✅ DONE

`tools/mkimage.sh` updated: the existing `build/lib/` loop copies `libc.so` → `/lib/libc.so`
automatically. Added an explicit second `mcopy` to install it as `/lib/ld-musl-i386.so.1`
(FAT32 has no symlinks). Both files are now present on the image after `bmake image`.

---

## Phase 6 — Switch Binaries to Dynamic Linking ✅ DONE

> Done. World binaries link dynamically against `/lib/ld-musl-i386.so.1`
> (verified: 71 of 73 are `dynamically linked`). The two exceptions kept
> `-static` are `bin/clock` and `bin/tcc`. (The original PID-1 safety rationale
> below is moot — `init`/`login` are dynamic too and boot fine, since `/lib` is
> on the root FAT mounted before they exec.)

**`mk/ubix.musl.prog.mk`** — the shared Makefile fragment used by all world binaries.

Remove `-static` from the link flags. Add the dynamic linker path:

```makefile
# Before (static):
LDFLAGS += -static

# After (dynamic):
# (no -static)
LDFLAGS += -Wl,-dynamic-linker,/lib/ld-musl-i386.so.1
LDFLAGS += -Wl,-rpath,/lib
```

Link against `libc.so` instead of `musl.a`:

```makefile
# Before:
LIBS += ${OBJ_DIR}/lib/musl.a

# After:
LIBS += -L${OBJ_DIR}/lib -lc
```

**`crt1.o` / `crti.o` / `crtn.o`**: these remain from the static musl build — they are
startup object files, not archived library code, and are the same for static and dynamic.

**Exceptions** — keep these static:
- `bin/init/` — PID 1 must not depend on anything; if libc.so is missing it must still run
- `bin/login/` — same safety argument
- Any binary that runs before `sys:/lib/` is mounted

---

## Phase 7 — Fix `ldEnable()` for musl's Dynamic Linker ✅ DONE

Two bugs fixed in `sys/kernel/ld.c`:

1. **Path resolution**: PT_INTERP paths are POSIX-absolute (`/lib/ld-musl-i386.so.1`).
   `fopen()` now resolves POSIX paths via longest-prefix mount matching, so `ldEnable`
   passes the interp path directly. Falls back to `/libexec/ld.so` only if the
   primary path fails.

2. **Removed kernel relocation pass**: The old code applied R_386_RELATIVE relocations in
   the kernel. musl's `_dlstart` bootstraps its own relocations at startup — the kernel
   pass would have double-applied them (adding `LD_START` twice) and crashed. The kernel
   now only loads PT_LOAD segments and jumps to `LD_START + e_entry`; `_dlstart` takes it
   from there.

The auxv in `sys/arch/i386/i386_exec.c` already has AT_BASE = LD_START (slot 7),
AT_ENTRY (slot 9), AT_PHDR (slot 3), AT_PHNUM (slot 5), AT_PAGESZ (slot 6) — musl's
`_dlstart` reads these correctly.

---

## Phase 8 — Verify and Test ✅ DONE

> Done. The system boots with dynamically-linked `init`/`login`/`ls`/`tcsh`
> against the shared `libc.so`; `file build/bin/*` reports 71/73 dynamic.

**Incremental test sequence**:

```sh
# 1. Verify mmap2 works — ls should run dynamically
ls sys:/bin

# 2. Check symbol resolution — strace-style kprintf in ldEnable
kprintf("ldEnable: resolved %s → 0x%X\n", sym_name, addr);

# 3. Check binary sizes before/after
# Before: ls ~ 800 KB (static musl inside)
# After:  ls ~ 50 KB  (just the code, libc.so shared)

# 4. Run two processes simultaneously, verify physical RAM usage
# didn't double for libc pages (check free page count via /proc or kprintf)
```

---

## Milestone Summary

| Phase | Work | Status |
|---|---|---|
| 1 | Wire `mmap2` syscall | ✅ Done |
| 2 | Implement `munmap` (page freeing) | ✅ Done |
| 3 | Implement `mprotect` (PTE flag update) | ✅ Done |
| 4 | Build musl `libc.so` | ✅ Done |
| 5 | Install `libc.so` to image | ✅ Done |
| 6 | Switch world binaries to dynamic linking | ✅ Done |
| 7 | Fix `ldEnable()` auxv and musl entry | ✅ Done |
| 8 | Verify and debug | ✅ Done — boots; 71/73 world binaries dynamic |

Phase 7 (auxv / musl entry point) is the most likely source of surprises — musl's dynamic
linker startup is strict about the auxiliary vector and will silently fail or crash if
AT_BASE, AT_ENTRY, or AT_PHDR are wrong. Having serial debug output (`kprintf` in
`ldEnable`) is essential.

---

## Relationship to Self-Hosting Plan

This plan is a prerequisite for `docs/design/self-hosting-plan.md` Track B (Stage 0
Clang). The self-hosting plan's anonymous mmap is satisfied by Phase 1–2 here.
File-backed mmap is already working and was hardened by the shared file-page cache
(the aarch64 memory work), as the self-hosting plan's verified-foundation table notes.

---

*Last updated: 2026-06-03 — COMPLETE. All 8 phases done; world is dynamically
linked against the shared musl `libc.so` (71/73 binaries; `clock`/`tcc` kept
static by design). Archived to `completed/`.*
