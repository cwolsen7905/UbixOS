# Cross-Architecture Portability Plan

## Goal

Prepare UbixOS for a future x86_64 port without breaking the i386 build.
The immediate goal is NOT to port to 64-bit — it is to restructure the tree,
harden build-system abstractions, and isolate arch-specific code so that when
the 64-bit work begins, there is a clean place for it to land.

Each phase ends in a bootable i386 kernel. No phase leaves the tree broken.

---

## Current Problem Areas

### 1. Hardware task switching leaks everywhere
`kTask_t` embeds `struct tssStruct tss` directly. Every file that touches
`_current->tss.eip` etc. is coupled to i386. x86_64 has no hardware task
switching at all.

### 2. `sys/sys/` contains i386-specific code
`idt.c`, `io.c` are in a generically-named directory but contain raw x86
descriptor table manipulation and `in`/`out` instructions.

### 3. Boot entry is generic in name only
`sys/init/start.S` and `sys/init/main.c` are 100% i386 (multiboot,
32-bit GDT, `ubixGDT[]` table) but live in a directory called `init/`.

### 4. Pointer types are hard-coded as 32-bit
`sys/include/sys/_types.h` hard-codes `__intptr_t = __int32_t` as a literal.
`uintptr_t` is typedef'd to `uint32_t` in many places. An x86_64 build would
need these to be 64-bit with no source changes to generic code.

### 5. VMM address arithmetic uses `uint32_t`
`sys/vmm/paging.c` uses `uint32_t *` for page directory entries, hard-codes
1024-entry tables, 4GB address space, and recursive paging constants that are
specific to the i386 two-level page table scheme.

### 6. No `machine/` abstraction layer
FreeBSD/NetBSD convention: `<machine/cpu.h>` redirects to the current arch.
UbixOS includes `<i386/cpu.h>` directly from generic headers like `<sys/trap.h>`.

---

## Phases

### Phase 1 — Make the Build System Architecture-Aware
**Boot risk:** None

**Changes:**
- `Makefile.incl`: change `_ARCH=i386` to `_ARCH?=i386` (overridable)
- `sys/compile/Makefile`: change `ldscript.i386` to `ldscript.${_ARCH}`

**Result:** `ARCH=i386` is explicit; the linker script is selected by variable.
A future `bmake ARCH=x86_64` will fail cleanly at link time, not silently
produce a broken binary.

---

### Phase 2 — Introduce `sys/include/machine/` Abstraction Layer
**Boot risk:** Very low

**New files:**
- `sys/include/machine/cpu.h` → forwards to `<i386/cpu.h>`
- `sys/include/machine/param.h` → arch-neutral page size / address-space constants
- `sys/include/machine/signal.h` → forwards to `<i386/signal.h>`
- `sys/include/machine/limits.h` → forwards to `<i386/limits.h>`
- `sys/include/machine/elf.h` → forwards to `<i386/elf.h>`

**Files to modify (include path only):**
- `sys/include/sys/trap.h`: `<i386/cpu.h>` → `<machine/cpu.h>`
- `sys/include/sys/limits.h`: `<i386/limits.h>` → `<machine/limits.h>`
- `sys/include/sys/elf.h`: `<i386/elf.h>` → `<machine/elf.h>`
- `sys/arch/i386/trap.c`: `<i386/signal.h>` → `<machine/signal.h>`
- `sys/vmm/vmm_memory.c`: `<i386/cpu.h>` → `<machine/cpu.h>`

**Result:** Generic code says `<machine/cpu.h>`. An x86_64 port adds
`sys/include/x86_64/cpu.h` and the forwarding header redirects there.

---

### Phase 3 — Move i386-Specific `sys/sys/` Files to `sys/arch/i386/`
**Boot risk:** Low

**Files to move:**
- `sys/sys/idt.c` → `sys/arch/i386/idt.c`
- `sys/sys/io.c` → `sys/arch/i386/io.c`

**Makefile changes:**
- `sys/arch/i386/Makefile`: add `idt.o io.o`
- `sys/sys/Makefile`: remove `idt.o io.o`

**Files that stay in `sys/sys/`:** `dma.c`, `video.c`, `device.c`, `elf.c`
(with comments marking `dma.c`/`video.c` as PC-specific).

**Result:** `sys/sys/` is genuinely generic. IDT/GDT/IO manipulation is
classified as arch code.

**Note:** `sys/include/sys/idt.h` and `sys/include/sys/io.h` stay generic —
they are the interface contracts. Only the implementations move.

---

### Phase 4 — Split `sys/arch/i386/sched.c` into Generic + Arch
**Boot risk:** Medium

`sched.c` today mixes two distinct concerns:

**Generic (move to `sys/kernel/sched_core.c`):**
`sched_init`, `schedNewTask`, `sched_deleteTask`, `sched_addDelTask`,
`sched_getDelTask`, `schedFindTask`, `sched_setStatus`, `sched_killTree`,
`wake_up`, `wake_up_interruptible`, `add_wait_queue`, plus the globals
`_current`, `_usedMath`, `taskList`, `nextID`, `schedulerSpinLock`.

**i386-specific (stays as `sys/arch/i386/sched_switch.c`):**
`sched()` (writes `ubixGDT[4]`, executes `ljmp $0x20,$0`), `sched_yield()`,
`schedEndTask()`.

**Makefile changes:**
- `sys/arch/i386/Makefile`: replace `sched.o` with `sched_switch.o`
- `sys/kernel/Makefile`: add `sched_core.o`

**New internal header `sys/include/ubixos/sched_internal.h`:**
Exposes `taskList`, `nextID`, `schedulerSpinLock` to both `sched_core.c`
and `sched_switch.c` without putting them in the public `sched.h`.

**Invariant to enforce:** `sched_core.c` must NOT include `<sys/gdt.h>` or
`<sys/idt.h>`. If it does, the split is incomplete.

---

### Phase 5 — Hide the TSS Behind `struct md_proc`
**Boot risk:** High — most invasive change

This is the key abstraction. `kTask_t` currently embeds `struct tssStruct tss`
directly. Every file touching `_current->tss.eip` is i386-coupled.

**New file `sys/include/machine/proc.h`** (i386 version):
```c
#include <sys/tss.h>
struct md_proc {
    struct tssStruct md_tss;
    struct i387Struct md_i387;
};
#define TASK_TSS(t) ((t)->md.md_tss)
```

**Modify `sys/include/ubixos/sched.h`:**
Replace `struct tssStruct tss; struct i387Struct i387;` with:
```c
#include <machine/proc.h>
struct md_proc md;
```

**Update all call sites:** `_current->tss.X` → `_current->md.md_tss.X`
(or `TASK_TSS(_current).X`). From the grep, affected files in `sys/arch/i386/`:
`sched_switch.c`, `fork.c`, `i386_exec.c`, `bioscall.c`, `trap.c`, `idt.c`.

**Verify no leakage:** After the change, run:
```sh
grep -rn "->tss\." sys/ --include="*.c" | grep -v "arch/i386"
```
Any hits are bugs.

**Result:** Generic code (`sched_core.c`, `kernel/`, `fs/`, etc.) never sees
`struct tssStruct`. An x86_64 `machine/proc.h` would define `md_proc` with a
software context frame instead of a TSS.

---

### Phase 6 — Arch-Parameterize Pointer Types
**Boot risk:** Medium

**New file `sys/include/machine/ansi.h`** (i386 version):
```c
typedef int          __intptr_t;
typedef unsigned int __uintptr_t;
typedef unsigned int __size_t;
typedef int          __ptrdiff_t;
```

**Modify `sys/include/sys/_types.h`:**
Remove hard-coded `__intptr_t = __int32_t` literal chain.
Add `#include <machine/ansi.h>`.

**Also fix in `sys/include/sys/types.h`:**
`typedef uint32_t uintptr_t;` → `typedef __uintptr_t uintptr_t;`

**Effect on i386:** Zero — `__uintptr_t` resolves to `unsigned int` (32-bit),
identical to before. Effect on future x86_64: `__uintptr_t` becomes
`unsigned long` (64-bit) with no source changes to generic code.

---

### Phase 7 — Replace `uint32_t` with `uintptr_t` for Address-Typed Values
**Boot risk:** Medium (same machine code, different semantic type)

Audit and fix every place where `uint32_t` holds a virtual or physical address
rather than a genuinely-32-bit integer.

**Key locations:**

`sys/include/vmm/paging.h` — function signatures:
- `vmm_getPhysicalAddr(uint32_t)` → `uintptr_t`
- `vmm_getRealAddr(uint32_t)` → `uintptr_t`
- `vmm_remapPage(uint32_t, uint32_t, ...)` → `uintptr_t, uintptr_t`
- `uint32_t *kernelPageDirectory` → `uintptr_t *`

`sys/include/vmm/vmm.h` — function signatures:
- `vmm_findFreePage`, `freePage`, `vmm_allocPageTable`, etc.

`sys/vmm/paging.c` — local variables holding page directory/table addresses.

`sys/include/ubixos/sched.h`:
- `uint32_t *kernelStack` → `uintptr_t *`

**What NOT to change:**
Page flag bitfields (PRESENT, WRITE, etc.) — these are always 32-bit.
PID, UID, GID types — not addresses.
Fields inside `struct tssStruct` (arch-specific, live in `machine/proc.h`).

---

### Phase 8 — Move `sys/init/start.S` and `main.c` to `sys/arch/i386/`
**Boot risk:** Medium

`sys/init/start.S` is pure i386: `.code32`, multiboot header, GDT load.
`sys/init/main.c` contains `ubixGDT[]` (the i386 GDT table), `loadGDT`, and
`kmain`.

**Move:**
- `sys/init/start.S` → `sys/arch/i386/start.S`
- `sys/init/main.c` → `sys/arch/i386/main.c`

**Makefile changes:**
- `sys/arch/i386/Makefile`: add `start.o main.o`
- `sys/Makefile`: remove `(cd init; ${MAKE})` from build sequence
- `sys/compile/Makefile`: remove `${OBJ_DIR}/obj/sys/init/*.o` from `KPARTS`
  (the `${_ARCH}/*.o` glob already picks them up)

**Optional refinement (not required for phase):**
Split `main.c` into `sys/arch/i386/locore.c` (GDT table data) and
`sys/kernel/main.c` (`kmain` generic OS bringup). Add a TODO comment for now.

---

### Phase 9 — Add `sys/include/machine/vmm_layout.h`
**Boot risk:** Low

Extract the i386 virtual address space layout constants from `vmm.h` into
an arch-parameterized header.

**New file `sys/include/i386/vmm_layout.h`:**
```c
#define VMM_USER_START   0x00800000
#define VMM_USER_END     0xBFFFFFFF
#define VMM_KERN_START   0xC0800000
#define VMM_KERN_END     0xFDFFFFFF
#define PD_BASE_ADDR     0xC0400000  /* i386 recursive page dir trick */
#define PT_BASE_ADDR     0xC0000000  /* i386 recursive page table base */
```

**New file `sys/include/machine/vmm_layout.h`:**
```c
#include <i386/vmm_layout.h>
```

**Modify `sys/include/vmm/vmm.h`:** remove inline defines, add:
```c
#include <machine/vmm_layout.h>
```

---

### Phase 10 — Add `sys/arch/x86_64/` Skeleton
**Boot risk:** Zero

Create directory and stub files so the tree communicates intent.
Nothing compiles from here under `_ARCH=i386`.

**Files to create:**
- `sys/arch/x86_64/Makefile` — empty OBJS, NOT YET IMPLEMENTED comment
- `sys/include/x86_64/ansi.h` — `__intptr_t = long`, `__uintptr_t = unsigned long`
- `sys/include/x86_64/cpu.h` — stub, comment: "CR register accessors — TBD"
- `sys/include/x86_64/proc.h` — stub, comment: "md_proc (software context frame, no TSS) — TBD"
- `sys/include/x86_64/vmm_layout.h` — stub with x86_64 VA layout comment

---

## Sequencing Summary

| Phase | Key Action | Boot Risk |
|-------|-----------|-----------|
| 1 | `_ARCH?=i386`, linker script parameterized | None |
| 2 | `sys/include/machine/` forwarding headers | Very low |
| 3 | Move `idt.c`, `io.c` to `sys/arch/i386/` | Low |
| 4 | Split `sched.c` → `sched_core.c` + `sched_switch.c` | Medium |
| 5 | `struct md_proc` hides TSS in `kTask_t` | High |
| 6 | `machine/ansi.h`, pointer types arch-parameterized | Medium |
| 7 | `uint32_t` → `uintptr_t` for address-typed values | Medium |
| 8 | Move `start.S`, `main.c` to `sys/arch/i386/` | Medium |
| 9 | `machine/vmm_layout.h` for address-space constants | Low |
| 10 | `sys/arch/x86_64/` skeleton, no code | None |

Phases 1–3 and 9–10 are safe warm-ups.
Phase 4 is the first real surgery.
Phase 5 is the hardest — do it last among the arch-isolation phases.

---

## Tree Shape After All Phases

```
sys/
  arch/
    i386/        start.S, main.c, idt.c, io.c, sched_switch.c,
                 fork.c, i386_exec.c, trap.c, bioscall.c,
                 sys_call.S, sys_call_posix.S, timer.S
    x86_64/      Makefile stub only
  kernel/
    sched_core.c (NEW — generic task list, _current, schedNewTask, etc.)
  sys/           dma.c, video.c, device.c, elf.c (io.c/idt.c moved out)
  vmm/           paging.c — unchanged logic, uintptr_t for addresses
  include/
    i386/        cpu.h, signal.h, limits.h, elf.h, vmm_layout.h, proc.h
    x86_64/      ansi.h, cpu.h, proc.h, vmm_layout.h (stubs)
    machine/     ansi.h, cpu.h, signal.h, limits.h, elf.h,
                 vmm_layout.h, proc.h (all forward to i386/)
    sys/
      _types.h   arithmetic types + #include <machine/ansi.h>
      trap.h     #include <machine/cpu.h>
    ubixos/
      sched.h    kTask_t.md is struct md_proc (not tss directly)
```

---

## Key Traps

- **armv6 directory** has copy-paste `ubixGDT[]` references — add FIXME
  comments but do not fix in this work (it's a dead branch)
- **`sched_core.c` must not include `<sys/gdt.h>`** — enforce by grep in CI
- **`ldscript.i386` load address `0x20000`** — below 1MB, BIOS convention;
  x86_64 will need >= 1MB; document in the linker script
- **Phase 5 field rename** — `->tss.X` → `->md.md_tss.X` in 25+ places;
  verify with `grep -rn "->tss\." sys/ --include="*.c" | grep -v arch/i386`
  after the change

---

## Status

| Phase | Status |
|-------|--------|
| 1 | Done |
| 2 | Not started |
| 3 | Not started |
| 4 | Not started |
| 5 | Not started |
| 6 | Not started |
| 7 | Not started |
| 8 | Not started |
| 9 | Not started |
| 10 | Not started |
