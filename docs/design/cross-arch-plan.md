# Cross-Architecture Portability Plan

> This is the single consolidated cross-arch plan. It absorbs the former
> `cross-arch-port-plan.md` (the musl arch-shim and per-arch porting notes are
> now in Track B below); that file has been removed to avoid two conflicting
> roadmaps.

## North Star

Evolve UbixOS toward a **research mobile-style OS on arm64 (AArch64)**, first
under emulation (QEMU `virt`) and then on open single-board hardware —
**Raspberry Pi and Orange Pi**. The long-term inspiration is the Android/iOS
*user experience* (touch-first GUI on an ARM device), reusing the existing
`views` compositor and `objGFX` graphics library.

This plan has two tracks:

- **Track A — Architecture isolation** (Phases 1–10). Restructure the tree so
  arch-specific code is quarantined and generic code is 64-bit-clean. *Mostly
  done* (see Status) — originally framed for x86_64, but every phase is
  arch-neutral and is exactly the groundwork arm64 needs.
- **Track B — arm64 bring-up** (Phases 11+). The actual AArch64 port: boot,
  MMU, interrupts, timer, context switch, virtio drivers, framebuffer GUI, then
  real Pi/Orange Pi hardware.

Each Track-A phase ends in a bootable i386 kernel. Track-B phases build a
*second* kernel (`TARGET_ARCH=aarch64`) and never touch the i386 build.

## Honest scope — reachable vs aspirational

Keep the goal motivating but grounded. These differ by orders of magnitude:

| Target | Reality |
|--------|---------|
| arm64 kernel boots in QEMU `virt` | **Reachable.** The cleanest ARM bring-up target — GIC, generic timer, PL011 UART, virtio, a QEMU-provided device tree. |
| Runs on a **Raspberry Pi / Orange Pi** | **Reachable, harder.** Public docs, mainline support, hackable bootloaders. Pi is among the best-documented bare-metal arm64 targets; Orange Pi (Allwinner/Rockchip) is similar but more per-board. |
| Runs on a **phone** (Pixel/iPhone) | Out of scope. Locked bootloaders, undocumented SoCs, proprietary modem/GPU. iPhone is effectively impossible. |
| **Competitor** to Android/iOS (the product) | Not a hobby-OS goal — app ecosystem, GPU/baseband drivers, OEM deals are thousands of person-years. Android/iOS are the *UX model*, not a market. |

The realistic, satisfying destination: **boots on a Pi/Orange Pi and drives a
touchscreen GUI.** Everything below is the staged path there, each rung bootable.

## Why this is closer than it looks

- The biggest blocker this plan originally identified — *"hardware task
  switching leaks everywhere"* (Problem #1) — is **solved**. UbixOS now uses a
  **software context switch** (`cpu_switch`), so the i386-only `ljmp`-to-TSS
  scheme is gone and the scheduler core is arch-neutral. arm64 (which has no HW
  task switching) inherits this directly.
- Track-A Phases 1–6 are **done**: the `machine/` forwarding layer, the
  `sched_core`/`sched_switch` split, `struct md_proc` hiding the TSS, and
  arch-parameterized pointer types. Generic code no longer sees i386 internals.
- The build system already targets by name: `TARGET_ARCH ?= i386` selects the
  arch dir, linker script, and per-arch ISA flags via
  `share/mk/ubix.target.${ARCH}.mk`. Adding arm64 is "write
  `ubix.target.aarch64.mk` + `sys/arch/aarch64/` + a linker script."
- The **newbus** driver model and an lwIP stack that can ride **virtio-net**
  are arch-neutral foundations arm64 reuses as-is.

---

## Status matrix

At-a-glance view of both tracks plus the cross-cutting userland prerequisite.
Detail is in the per-phase sections; the per-track tables at the bottom carry the
same status.

| # | Item | Track | Status |
|---|------|-------|--------|
| 0 | `TARGET_ARCH` knob + per-arch ISA flags | A | ✅ Done |
| 1 | `_ARCH?=i386`, linker script parameterized | A | ✅ Done |
| 2 | `sys/include/machine/` forwarding headers | A | ✅ Done |
| 3 | Move `idt.c`/`io.c` to `sys/arch/i386/` | A | ✅ Done |
| 4 | Split `sched.c` → `sched_core.c` + arch switch | A | ✅ Done |
| 5 | `struct md_proc` hides the TSS in `kTask_t` | A | ✅ Done |
| 6 | `machine/ansi.h`, pointer types parameterized | A | ✅ Done |
| 9 | `machine/vmm_layout.h` address-space constants | A | ✅ Done |
| — | Software context switch (`cpu_switch`) — *the historic #1 blocker* | A | ✅ Done |
| **7** | **`u_int32_t` → `uintptr_t` for address-typed values** | A | 🟡 **Core done** (frame allocator, paging map/translate API, frame-map field, `vm_map`); mmap/exec inline i386 page-table walks deferred to the aarch64-MMU work |
| **T** | **Quarantine i386 TLS (`set_thread_area`/LDT/`%gs`) behind `<machine/tls.h>`** | A | ⬜ **Not started — blocker** |
| 8 | Move `start.S`/`main.c` to `sys/arch/i386/` | A | ⬜ Not started |
| 10 | `sys/arch/aarch64/` skeleton + `ubix.target.aarch64.mk` | A | ⬜ Not started |
| — | musl world arch shim + de-hardcode i386 in `lib/Makefile` | Userland | ⬜ Not started — blocker for `world` |
| 11 | Boot to PL011 UART on QEMU `virt` | B | ✅ **Done** — `uBixOS aarch64` banner verified on serial (`bmake run-debug TARGET=aarch64`) |
| 12 | Exceptions + GICv2 + generic timer | B | ⬜ Not started |
| 13 | MMU (TTBR0/1) + AArch64 `cpu_switch` + syscall entry | B | ⬜ Not started |
| 14 | virtio-blk + virtio-net | B | ⬜ Not started |
| 15 | virtio-gpu framebuffer + virtio-input (touch) | B | ⬜ Not started |
| 15a | **virtio-sound** (audio) → existing `aural` layer | B | ⬜ Not started — *was missing* |
| 16 | Raspberry Pi 4 hardware (**optional** — QEMU is the target) | B | ⬜ Deferred |

### Major blockers (reviewed 2026-06-06)

**No new architectural blocker** — the structure holds and the historically
biggest one (i386 hardware task switching) is *gone*, replaced by the
arch-neutral software `cpu_switch`. The remaining gates, in order:

1. **Phase 7 — address types → `uintptr_t`. 🟡 Core done.** The *shared* VMM
   surface that aarch64's generic code consumes is converted and verified
   codegen-identical on i386: the physical-frame allocator (`vmm_find_free_page`,
   `free_page`, `adjust_cow_counter`, `vmm_share_ref`), the frame-map `pageAddr`
   field, the public paging map/translate API (`vmm_get_physical_addr`,
   `vmm_remap_page`, `vmm_set_page_attributes`, `vmm_clean_virtual_space`,
   `vmm_page_fault`, `kernelPageDirectory`, …), and `vm_map` (was already clean).
   **Deferred, intentionally:** `vmm_mmap.c` and the exec loader walk the i386
   page tables *inline* (`PD_BASE_ADDR`/`PT_BASE_ADDR` recursive-mapping), so they
   are i386-specific code that aarch64 *replaces* rather than recompiles —
   retyping their locals adds risk for no aarch64 benefit. They get handled as
   part of writing the aarch64 MMU (Phase 13), where the page-table access is
   abstracted, not as a typedef sweep. `kernelStack` is a *pointer* (already
   64-bit-safe); its one truncating use (`(u_int32_t)kernelStack` for
   `md_tss.esp0`) is an i386-TSS arch-isolation leak in generic `sched_core.c`
   (Phase 4/5), not a Phase 7 item.
2. **i386 TLS leaked into generic code — and it just grew.** `sys_set_thread_area`
   (writes LDT[1], the `0x0F` selector) lives in generic `sys/posix/gen_calls.c`,
   and `kTask_t.tls_base` is in generic `sched.h`; the recent threads/TLS work
   deepened this coupling. arm64 uses `TPIDR_EL0`, a completely different
   mechanism. Quarantine behind `<machine/tls.h>` (i386 → LDT, aarch64 → TPIDR)
   before the port — this was a "Key Trap" and is now an active blocker.
3. **Userland hardcodes i386.** `lib/Makefile` + the `musl-libc` target bake in
   i386, so `bmake world TARGET_ARCH=aarch64` can't run yet. The AArch64 musl
   shim itself is small (3 files), but de-hardcoding the world build is the
   prerequisite. (Kernel and userland port independently; this only gates `world`.)

Phases 8 and 10 are mechanical (a file move and an empty skeleton) and carry no
design risk. Track B is greenfield. Net: with Phase 7's shared core done, the
**remaining substantive Track-A task is the TLS quarantine**; Phase 8/10 are
mechanical; and the i386-coupled mmap/exec page-table walks fold into the
aarch64-MMU work (Phase 13) rather than blocking bring-up.

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

### Phase 9 — Add `sys/include/machine/vmm_layout.h` ✅ DONE
**Boot risk:** Low

> Done: `sys/include/i386/vmm_layout.h` holds the layout constants (VMM_USER_*,
> VMM_KERN_*, STACK_ADDR, PD_BASE_ADDR, PT_BASE_ADDR, VMM_CHILD_PD_WINDOW, …);
> `sys/include/machine/vmm_layout.h` includes it; `vmm.h` and `paging.h` include
> `<machine/vmm_layout.h>` instead of defining the constants inline.

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

### Phase 10 — Add `sys/arch/aarch64/` Skeleton
**Boot risk:** Zero

Create the arm64 arch directory and stub headers so the tree communicates
intent and `TARGET_ARCH=aarch64` has a place to resolve `<machine/*>` to.
Nothing compiles from here under `TARGET_ARCH=i386`. (An `x86_64/` skeleton can
be added the same way later if the LP64 rehearsal is ever wanted — but arm64 is
the active target.)

**Files to create:**
- `share/mk/ubix.target.aarch64.mk` — cross prefix `aarch64-elf-`/`aarch64-linux-gnu-`, `KERN_TARGET_CFLAGS = -march=armv8-a -mgeneral-regs-only` (no FP/NEON in the kernel), `LDSCRIPT_SUFFIX = aarch64`
- `sys/arch/aarch64/Makefile` — empty OBJS, NOT YET IMPLEMENTED comment
- `sys/include/aarch64/ansi.h` — `__intptr_t = long`, `__uintptr_t = unsigned long` (LP64)
- `sys/include/aarch64/cpu.h` — stub: system-register accessors (`mrs`/`msr`) TBD
- `sys/include/aarch64/proc.h` — stub: `md_proc` = software context frame (x0–x30, sp, pc, pstate); no TSS
- `sys/include/aarch64/vmm_layout.h` — stub: TTBR0 (user, low) / TTBR1 (kernel, high) VA split

---

# Track B — arm64 (AArch64) bring-up

Track-A leaves a clean, 64-bit-ready generic kernel. Track B writes the AArch64
arch code and drivers. Build with `TARGET_ARCH=aarch64`; the i386 build is never
touched.

### Target & dev environment (current effort)

**QEMU `virt` is the target**, not a waypoint — the whole desktop (graphics,
sound, network, input) runs on QEMU's virtio devices. Real hardware (Phase 16) is
now **optional/deferred**; none of the board-specific work (GIC-400, mailbox FB,
GENET, xHCI) is required to reach a working GUI.

- **Host:** macOS on Apple Silicon (M5). Because the guest *and* host are both
  AArch64, QEMU runs under the **HVF hardware accelerator at near-native speed** —
  no slow x86→ARM TCG emulation. This is the best-case porting host.
  ```sh
  qemu-system-aarch64 -machine virt -accel hvf -cpu host -m 512 \
    -kernel build/boot/kernel \
    -drive if=none,file=ubixos-arm.img,format=raw,id=hd0 -device virtio-blk-device,drive=hd0 \
    -netdev user,id=n0 -device virtio-net-device,netdev=n0 \
    -device virtio-gpu-device -device virtio-keyboard-device -device virtio-mouse-device \
    -audiodev coreaudio,id=a0 -device virtio-sound-device,audiodev=a0 \
    -serial mon:stdio
  ```
- **Toolchain:** `brew install aarch64-elf-gcc aarch64-elf-binutils` (kernel,
  freestanding); the world cross-compiles via the musl AArch64 shim. QEMU itself:
  `brew install qemu`.
- **Transport:** `-machine virt` exposes both **virtio-mmio** slots and a **PCIe**
  bus. Bring-up uses virtio-mmio (simplest — fixed MMIO slots from the device
  tree, no PCIe enumeration); virtio-pci can come later if wanted.

### QEMU `virt` device coverage — what each subsystem rides on

| Subsystem | QEMU device | UbixOS layer it plugs into | Phase |
|-----------|-------------|----------------------------|-------|
| Console | PL011 UART | `kprintf` / serial tty | 11 |
| Interrupts/timer | GICv2 + ARM generic timer | IRQ dispatch + scheduler tick / callouts | 12 |
| Storage | virtio-blk | existing VFS + FAT stack | 14 |
| Network | virtio-net | existing lwIP stack | 14 |
| Graphics | virtio-gpu (linear FB) | `sys_mapfb` → `views` + `objGFX` (CPU rendering) | 15 |
| Input / touch | virtio-input | mouse/keyboard event queues; touch → pointer | 15 |
| **Sound** | **virtio-sound (virtio-snd)** | existing **`aural`** audio abstraction (AC97 driver is i386/PCI-only, does not port) | 15a |

All of these attach through the arch-neutral **newbus** model, exactly like the
PC drivers — so the driver *framework* is reused; only the per-device virtio
backends are new.

### Phase 11 — Minimal arm64 boot to UART on QEMU `virt` ✅ DONE
**Boot risk:** N/A (new kernel)

Shipped & verified — the `uBixOS aarch64 (QEMU virt) - boot OK` banner prints
over the PL011 serial (confirmed under TCG; HVF on Apple Silicon for speed).
- `sys/arch/aarch64/start.S`: EL-agnostic entry — mask IRQs (DAIF), set SP, zero
  BSS, branch to `kmain_aarch64`; park on `wfi` if it returns. (QEMU `-kernel`
  loads the ELF at its link address; no multiboot.)
- `sys/arch/aarch64/boot.c`: PL011 UART (0x09000000) putc/puts + the banner.
- `sys/compile/ldscript.aarch64` (link at the `virt` RAM base 0x40000000) +
  `share/mk/ubix.target.aarch64.mk` (done earlier).
- Built standalone via the arch-dispatched `kernel` target (`kernel:
  kernel-${_ARCH}`); the full sys/ tree is **not** compiled for aarch64 yet.
- **Run:** `bmake kernel TARGET=aarch64 && bmake run-debug TARGET=aarch64`.
  Caveat: i386 and aarch64 both link to `build/boot/kernel`, so rebuild the arch
  you intend to run.

Next (Phase 12): wire `kprintf` to this UART, then EL1 setup + exception vectors.

### Phase 12 — Exceptions, GIC, generic timer
- AArch64 exception vector table (VBAR_EL1), sync/IRQ/FIQ/SError handlers.
- GICv2 driver (distributor + CPU interface) — this is what `-machine virt`
  and the **Pi 4** both expose (GIC-400). Replaces the i386 8259/APIC.
- ARM generic timer (CNTP) → the scheduler tick + the callout subsystem
  (already arch-neutral) ride on it.

### Phase 13 — MMU + context switch
- 4-level (or 3-level, 4 KB granule) page tables; TTBR0_EL1 (user, low half) /
  TTBR1_EL1 (kernel, high half); MAIR/TCR setup. Map the kernel, enable the MMU.
- AArch64 `cpu_switch`: save/restore x19–x30, sp, and switch TTBR0. The
  software-switch scheduler core is reused unchanged — **the i386 work already
  proved this path.** `md_proc` (Phase 5/10) becomes the AArch64 register frame.
- Port `fork`/`exec` arch glue and the syscall entry (SVC → exception handler).

### Phase 14 — virtio drivers (storage + net)
- virtio-mmio transport (what `virt` exposes), then virtio-blk (root fs via the
  existing VFS/FAT stack) and virtio-net (the existing lwIP stack rides on top).
- These attach through the **newbus** model, same as the PC NIC drivers.

### Phase 15 — Framebuffer GUI + input
- virtio-gpu (or the Pi firmware mailbox framebuffer on real hw) → a linear
  framebuffer for `sys_mapfb`. The `views` compositor + `objGFX` (software
  rendering — no GPU 3D needed) come up on arm64.
- virtio-input → keyboard + **touch/pointer**, the first step toward the
  touch-first mobile UX.

### Phase 15a — virtio-sound (audio)
- virtio-snd driver (PCM playback stream over the virtqueues) attached via
  newbus, presented under the existing **`aural`** sound abstraction so the
  Settings Sound pane, volume tray, and `sndcfg` all work unchanged.
- The i386 **AC97** driver does not port (PC PCI device); virtio-snd replaces it.
  QEMU side: `-audiodev coreaudio,id=a0 -device virtio-sound-device,audiodev=a0`.
- Milestone: a tone / WAV plays through the host's CoreAudio output.

### Phase 16 — Real hardware: Raspberry Pi / Orange Pi (optional / deferred)
**QEMU `virt` is the destination for this effort; this phase is optional** and
only needed if/when the project wants to leave emulation for a physical board.
- Swap QEMU `virt` peripherals for the board's: Pi 4 uses a GIC-400 (matches
  virt — least surprise), PL011 UART (over the GPIO header + USB-TTL cable),
  the firmware mailbox framebuffer, BCM GENET gigabit ethernet, and xHCI USB
  (VL805 over PCIe). Orange Pi (Allwinner/Rockchip) is a parallel board port
  once the Pi path is solid.
- Boot path: kernel on the SD card's FAT boot partition (Pi firmware loads it),
  `config.txt` set to load our image. This reuses the existing FAT tooling.

### Userland — the musl arch shim (runs alongside Track B)

The kernel is the hard part; **userland mostly recompiles untouched.** All
world C/C++ (libc consumers, libc++, the `views`/`objGFX` apps) is
arch-neutral once musl has an AArch64 shim. That shim is three files:

| File | Contents |
|------|----------|
| `contrib/musl/arch/aarch64/syscall_arch.h` | inline `__syscall0`…`__syscall6` using `svc #0`; args in x0–x5, number in x8, return in x0 |
| `contrib/musl/arch/aarch64/bits/syscall.h.in` | POSIX name → UbixOS kernel slot map for the AArch64 ABI |
| `contrib/musl/arch/aarch64/bits/alltypes.h.in` | LP64 primitive types |

musl already ships a reference `arch/aarch64/`; the UbixOS-specific part is the
syscall-number map (must match our kernel slots) and pointing the `svc` path at
our exception handler from Phase 12. **Done when** `bmake world TARGET_ARCH=aarch64`
produces `ELF 64-bit LSB executable, ARM aarch64`. (World/musl still hardcode
i386 in `lib/Makefile` + the `musl-libc` target — that cleanup, noted in the
build section, is the prerequisite for this.)

---

# Hardware targets — what to actually buy

Picked for **maximum public documentation and a real bare-metal community**, so
"fully support the hardware" is achievable by a small team:

**Buy now (bring-up board): Raspberry Pi 4 Model B (4 GB), ~$55.**
- Cortex-A72 (AArch64), **GIC-400 = standard GICv2**, so interrupt code written
  for QEMU `virt` ports with minimal change — this is the key reason to prefer
  Pi 4 over Pi 3 (Pi 3 uses a non-standard BCM interrupt controller).
- PL011 UART, ARM generic timer, firmware **mailbox framebuffer** (linear FB
  for the GUI, no closed GPU 3D needed — `objGFX` is CPU rendering).
- Huge bare-metal community + published BCM2711 peripheral docs.

**Also buy:**
- **USB-to-TTL serial cable** (3.3 V, CP2102/FTDI), ~$8 — the serial console for
  `kprintf` during bring-up. *Essential.*
- microSD (32 GB A1) ~$8, official USB-C PSU ~$8.
- Later, for the touch milestone: official **Pi 7″ touchscreen** (~$60) or an
  HDMI monitor + a USB touch panel (USB HID touch is easier than DSI).

≈ **$80 to start.**

**Orange Pi:** a good *second* board (you mentioned both). Allwinner/Rockchip
SoCs are well-documented (sunxi / Rockchip communities), but boards vary more
and the bring-up is more per-SoC — tackle after the Pi path is solid.

**The eventual fully-open *phone* (much later, much harder): PinePhone /
PinePhone Pro.** Unlike a Pixel or iPhone, Pine64 publishes schematics and it
runs mainline Linux — so it is the *only* phone you could realistically "fully
support." Still a big lift (DSI display, capacitive touch, PMIC, the modem),
but it is documented and open. This is the honest bridge from "SBC with a touch
screen" to "actual handset," and the right north-star device for the mobile
dream — after the Pi milestones land.

---

## Sequencing Summary

### Track A — architecture isolation (mostly done)

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
| 10 | `sys/arch/aarch64/` skeleton + target mk, no code | None |

Phases 1–3 and 9–10 are safe warm-ups.
Phase 4 is the first real surgery.
Phase 5 is the hardest — do it last among the arch-isolation phases.

### Track B — arm64 bring-up (`TARGET_ARCH=aarch64`, never touches i386)

| Phase | Key Action | Lands on |
|-------|-----------|----------|
| 11 | Boot to PL011 UART (`kprintf`) | QEMU `virt` |
| 12 | Exception vectors, GICv2, generic timer | QEMU `virt` |
| 13 | MMU (TTBR0/1) + AArch64 `cpu_switch` + syscall entry | QEMU `virt` |
| 14 | virtio-mmio + virtio-blk + virtio-net (VFS/lwIP ride on top) | QEMU `virt` |
| 15 | virtio-gpu framebuffer + virtio-input → `views`/`objGFX` + touch | QEMU `virt` |
| 15a | virtio-sound → `aural` audio abstraction | QEMU `virt` |
| 16 | Board peripherals (GIC-400, mailbox FB, GENET, xHCI) — **optional** | Raspberry Pi 4 |

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
- **i386 TLS syscall still in generic code** — `sys_set_thread_area` (LDT[1],
  the `0x0F` selector) lives in `sys/kernel/gen_calls.c`. Before/with the arm64
  port, move it to `sys/arch/i386/tls.c` behind `<machine/tls.h>` (arm64 uses
  `TPIDR_EL0`, a completely different mechanism). Same for any other i386-only
  syscalls that accreted in `gen_calls.c`.

---

## Status

**Track A — architecture isolation**

| Phase | Name | Status |
|-------|------|--------|
| 0 | `TARGET_ARCH` knob + `share/mk/ubix.target.${ARCH}.mk` (ISA flags per arch) | **Done** |
| 1 | `_ARCH?=i386`, linker script parameterized | Done |
| 2 | `sys/include/machine/` forwarding headers | Done |
| 3 | Move `idt.c`, `io.c` to `sys/arch/i386/` | Done |
| 4 | Split `sched.c` → `sched_core.c` + `sched_switch.c` | Done |
| 5 | `struct md_proc` hides TSS in `kTask_t` | Done |
| 6 | `machine/ansi.h`, pointer types arch-parameterized | Done |
| 9 | `machine/vmm_layout.h` for address-space constants | Done |
| 7 | `u_int32_t` → `uintptr_t` for address-typed values | 🟡 Core done (allocator + paging API + vm_map); mmap/exec deferred to Phase 13 |
| 8 | Move `start.S`, `main.c` to `sys/arch/i386/` | Not started |
| 10 | `sys/arch/aarch64/` skeleton + `ubix.target.aarch64.mk` | Not started |

Remaining Track-A work before arm64: **Phase 7** (address-typed values must be
`uintptr_t`, not `u_int32_t`, or they truncate on 64-bit), the **TLS quarantine**
(move `set_thread_area`/LDT/`%gs` and `tls_base` behind `<machine/tls.h>` — the
recent threads work deepened this i386 coupling in generic code), **Phase 8**
(quarantine the i386 boot entry), then **Phase 10** (arm64 skeleton). Phase 7 is
the real prerequisite — it is where a 64-bit build would otherwise silently
corrupt addresses. See the Status matrix near the top for the at-a-glance view.

**Track B — arm64 bring-up**

| Phase | Name | Status |
|-------|------|--------|
| 11 | Boot to PL011 UART on QEMU `virt` | ✅ Done (banner verified, TCG + HVF) |
| 12 | Exceptions + GICv2 + generic timer | Not started |
| 13 | MMU + `cpu_switch` + syscall entry | Not started |
| 14 | virtio-blk + virtio-net | Not started |
| 15 | virtio-gpu framebuffer + virtio-input (touch) | Not started |
| 15a | virtio-sound (audio) → `aural` abstraction | Not started |
| 16 | Raspberry Pi 4 hardware (optional — QEMU is the target) | Deferred |
