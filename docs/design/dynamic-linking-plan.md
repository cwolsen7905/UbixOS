# UbixOS Dynamic Linking Plan

**Goal**: Build musl as `libc.so`, install it to the image, and link all world binaries
dynamically so processes share a single copy of libc in physical RAM rather than each
carrying their own 500 KB+ static copy.

**Memory benefit**: with 10 processes running, dynamic linking saves roughly 10 × 400 KB =
4 MB of physical RAM — significant on a 256 MB QEMU machine, critical for self-hosting.

**Prerequisite for self-hosting**: Stage 0 Clang expects a shared libc by default.

---

## Current State

More infrastructure exists than expected:

| Component | State |
|---|---|
| `sys_mmap` — anonymous, no hint | Works — uses `vmm_getFreeVirtualPage` |
| `sys_mmap` — anonymous, with hint | Works — maps at the given VA |
| `sys_mmap` — file-backed | Partial — reads entire file eagerly (not demand-paged), but correct |
| `sys_munmap` | **Stub** — returns 0, frees nothing |
| `mprotect` | **Stub** — returns 0, changes nothing |
| `mmap2` (slot 192, musl i386) | **Not wired** — musl uses this instead of mmap |
| `ldEnable()` kernel loader | Works — loads ld.so at `LD_START` (0xAAA00000), applies relocations |
| `PT_INTERP` detection | Works — kernel reads interp path and calls `ldEnable()` |
| `libexec/rtld-elf/` | FreeBSD rtld source present, not built for UbixOS |
| musl `libc.so` build target | In musl's Makefile, not invoked by UbixOS build |
| `sys:/lib/` on image | Does not exist |
| Binary Makefiles | All use `-static` |

---

## Phase 1 — Wire `mmap2`

musl on i386 always uses `mmap2` (syscall 192) rather than `mmap` (syscall 90/477).
`mmap2` is identical except the offset parameter is in 4096-byte pages, not bytes.

**`sys/kernel/syscalls_posix.c`** — slot 192 currently marked `SYSCALL_INVALID`.
Change to route through `sys_mmap` with the offset multiplied by `PAGE_SIZE`:

```c
/* sys/vmm/vmm_mmap.c — add mmap2 wrapper */
int sys_mmap2(struct thread *td, struct sys_mmap_args *uap) {
    uap->pos = uap->pos * PAGE_SIZE;   /* convert page offset to byte offset */
    return sys_mmap(td, uap);
}
```

Wire slot 192 in `syscalls_posix.c` to `sys_mmap2`.
Add `__NR_mmap2 → 192` to `contrib/musl/arch/i386/bits/syscall.h.in` if not present.

**Test**: `malloc(1)` from a dynamically-linked binary should not segfault.

---

## Phase 2 — Implement `munmap`

The current stub leaks all mapped memory. This is not fatal for initial dynamic linking
(shared libraries are mapped once and never unmapped during normal operation) but causes
runaway memory use in any program that does repeated mmap/munmap cycles.

**`sys/vmm/vmm_mmap.c`**:

```c
int sys_munmap(struct thread *td, struct sys_munmap_args *uap) {
    uint32_t base = (uint32_t)uap->addr & ~0xFFF;
    uint32_t end  = base + round_page(uap->len);

    for (uint32_t va = base; va < end; va += PAGE_SIZE) {
        uint32_t phys = vmm_getPhysAddr(va);
        if (phys != 0)
            vmm_unmapPage(va, VMM_FREE);
    }
    td->td_retval[0] = 0;
    return (0);
}
```

Note: without a `vm_map` (list of active mappings per process), `munmap` cannot validate
that the range was actually mapped. That is acceptable for now — an incorrect munmap just
silently does nothing, which is safe.

---

## Phase 3 — Implement `mprotect`

musl's dynamic linker calls `mprotect` to make text segments read-only and non-writable
after relocation (RELRO). A no-op stub means RELRO is silently skipped — not a security
issue for a hobby OS but the missing PTE updates can cause subtle bugs if writable pages
are left in place when the linker expects them to be read-only.

**`sys/kernel/gen_calls.c`** — replace the stub:

```c
int mprotect(struct thread *td, struct mprotect_args *uap) {
    uint32_t base  = (uint32_t)uap->addr & ~0xFFF;
    uint32_t end   = base + round_page(uap->len);
    int      prot  = uap->prot;
    uint32_t flags = PAGE_DEFAULT;

    if (!(prot & PROT_WRITE))
        flags = PAGE_READ_ONLY;   /* clear R/W bit in PTE */

    for (uint32_t va = base; va < end; va += PAGE_SIZE)
        vmm_setPageFlags(va, flags, _current->id);

    td->td_retval[0] = 0;
    return (0);
}
```

This requires a `vmm_setPageFlags(va, flags, pid)` helper that walks the page directory
and updates the PTE without remapping the page.

---

## Phase 4 — Build musl as `libc.so`

musl's Makefile already has the shared library target. The UbixOS build just needs to
invoke it and capture the output.

**`contrib/musl/Makefile`** already produces:
- `lib/libc.so` — the shared library (also contains the dynamic linker)
- `lib/ld-musl-i386.so.1` — symlink to `libc.so`
- `lib/libc.a` — unchanged static archive

**UbixOS musl build integration** (`contrib/musl/Makefile` UbixOS wrapper or the top-level
`Makefile`):

```makefile
# After musl static build, also build the shared library
musl-shared:
    $(MAKE) -C contrib/musl lib/libc.so
    cp contrib/musl/lib/libc.so    build/lib/libc.so
    cp contrib/musl/lib/libc.so    build/lib/ld-musl-i386.so.1
```

The shared build needs `-fPIC` in musl's CFLAGS — musl handles this automatically when
building `libc.so` via its own Makefile.

**Installed layout on image**:
```
sys:/lib/libc.so              ← the shared library + embedded dynamic linker
sys:/lib/ld-musl-i386.so.1    ← symlink (or copy) — PT_INTERP path in ELF binaries
```

FAT32 does not support symlinks. Both must be installed as separate copies, or the build
must set the `PT_INTERP` string to `/lib/libc.so` directly in all binaries (configure
musl with `--syslibdir=/lib` and `--libdir=/lib`).

---

## Phase 5 — Install `libc.so` to the Image

Add to `tools/mkimage.sh` and the incremental install targets:

```sh
# In mkimage.sh, after copying world binaries:
mcopy -o -i "$IMG"@@1M build/lib/libc.so           ::/lib/libc.so
mcopy -o -i "$IMG"@@1M build/lib/libc.so           ::/lib/ld-musl-i386.so.1
```

Add `bmake install-libs` target to the top-level Makefile:
```makefile
install-libs:
    mcopy -o -i ${IMAGE}@@1M build/lib/libc.so        ::/lib/libc.so
    mcopy -o -i ${IMAGE}@@1M build/lib/libc.so        ::/lib/ld-musl-i386.so.1
```

---

## Phase 6 — Switch Binaries to Dynamic Linking

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

## Phase 7 — Fix `ldEnable()` for musl's Dynamic Linker

The existing `ldEnable()` loads ld.so at fixed address `LD_START = 0xAAA00000` and
applies its relocations. musl's `ld-musl-i386.so.1` is a position-independent shared
object that expects to be loaded at an address it chooses (or at 0 with a `LOAD_BIAS`).

Key differences from the current FreeBSD-rtld assumption:

1. **musl rtld entry point**: musl's dynamic linker entry is `_dlstart` (in
   `ldso/dlstart.c`), not `e_entry` relative to a fixed base. The kernel must pass
   control to `_dlstart` with the correct auxiliary vector (AT_PHDR, AT_PHNUM, AT_ENTRY,
   AT_BASE, AT_PAGESZ) on the stack.

2. **Auxiliary vector**: musl's `_dlstart` reads `auxv[]` from the process stack to find
   the program headers and app entry point. The kernel must push a proper `auxv` array
   after `argv` and `envp` when setting up the initial stack.

3. **`AT_BASE`**: must be set to `LD_START` (the address where ld.so was loaded) so musl
   can apply its own RELATIVE relocations at startup.

**`sys/arch/i386/i386_exec.c`** — update the stack setup to push `auxv`:

```c
/* Push auxiliary vector after envp NULL terminator */
uint32_t *auxv = stack_ptr;
*auxv++ = AT_PAGESZ;  *auxv++ = PAGE_SIZE;
*auxv++ = AT_PHDR;    *auxv++ = (uint32_t)phdr_user_addr;
*auxv++ = AT_PHNUM;   *auxv++ = phnum;
*auxv++ = AT_ENTRY;   *auxv++ = app_entry;
*auxv++ = AT_BASE;    *auxv++ = ldAddr;   /* LD_START */
*auxv++ = AT_NULL;    *auxv++ = 0;
```

AT_ENTRY and AT_PHDR values are already computed during ELF loading — they just need to
be saved and placed in the auxv rather than discarded.

---

## Phase 8 — Verify and Test

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

| Phase | Work | Est. effort |
|---|---|---|
| 1 | Wire `mmap2` syscall | 1–2 hours |
| 2 | Implement `munmap` (page freeing) | 2–4 hours |
| 3 | Implement `mprotect` (PTE flag update) | 4–8 hours |
| 4 | Build musl `libc.so` | 2–4 hours |
| 5 | Install `libc.so` to image | 1–2 hours |
| 6 | Switch world binaries to dynamic linking | 2–4 hours |
| 7 | Fix `ldEnable()` auxv and musl entry | 1–2 days |
| 8 | Verify and debug | 1–2 days |
| **Total** | | **~1 week** |

Phase 7 (auxv / musl entry point) is the most likely source of surprises — musl's dynamic
linker startup is strict about the auxiliary vector and will silently fail or crash if
AT_BASE, AT_ENTRY, or AT_PHDR are wrong. Having serial debug output (`kprintf` in
`ldEnable`) is essential.

---

## Relationship to Self-Hosting Plan

This plan is a prerequisite for `docs/design/clang-selfhost-plan.md` Phase 6 (Stage 0
Clang). The self-hosting plan's Phase 1 (anonymous mmap) is satisfied by Phase 1–2 here.
File-backed mmap (self-hosting Phase 2) is already partially working and will be hardened
as part of Phase 7 testing when musl's rtld mmaps `libc.so` segments.

Once dynamic linking works, update `clang-selfhost-plan.md` to reflect that Phases 1–2
are complete.

---

*Last updated: 2026-05-22*
