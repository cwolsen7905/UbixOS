# Console + Architecture-Convergence Plan

> Companion to `cross-arch-plan.md`. That plan got AArch64 *booting* (Track B);
> this one is about **paying down the bring-up shortcuts** so i386 and aarch64
> ride the same generic mechanisms, and about **modernising the console/tty
> layer** toward the macOS-style model the product actually wants. Every phase
> below ends in a kernel that boots on **both** arches.

## North Star

uBixOS is a **graphical-first OS**: the `views` compositor owns the display and
the real terminal is a pty-backed app (like `Terminal.app`), not a text VT. The
console/tty layer should reflect that, and the i386/aarch64 split should be
defined by a **generic interface that both arches implement** — not two parallel
implementations that happen to share headers.

Two intertwined goals:

1. **Console/tty modernisation** — one selectable boot/system console + serial
   debug sink; drop the legacy multi-VT / `ttyd` stack; keep the portable pty +
   VT100 engine.
2. **Arch convergence** — collapse the divergences the AArch64 bring-up left
   behind (hand-rolled syscall dispatch, duplicated fork/exec, `bringup/`
   scaffolding, hard-coded VA layout) onto the existing clean MD interfaces.

These reinforce each other: items in goal 1 *remove* i386-only complexity and
shrink the arch asymmetry, which is also goal 2.

## Product identity (decided 2026-06-09)

uBixOS is **console-first, graphical-optional, one tree, profile-driven.**

- **Console-first.** The kernel and base system are *display-agnostic*. The
  kernel never requires a screen; it requires a *console abstraction* (serial
  always; framebuffer optional). "Is uBixOS graphical?" is the wrong question —
  graphical is a layer, not the foundation.
- **Graphical-optional.** The graphical stack (`views` + `objGFX` + `vlogin` +
  apps) is a **userland layer**. `init` starts a console shell on a headless
  profile, or `views` on a desktop profile. Same kernel either way.
- **One tree, profile-driven.** Device classes differ by *userland profile +
  compiled-in drivers*, not by separate trees. Forks diverge and tax forever; a
  single tree with clean layering is cheaper. The real cost is the test matrix +
  decoupling discipline (never let the desktop leak an assumption into the base).

### Scope boundary

- **In scope: MMU-class only** — single-board computers (Raspberry Pi, Orange
  Pi) and x86 PCs: paging, processes, fork/exec, dynamic linker, FAT32, lwIP.
  Here **"IoT" and "desktop" are the same hardware** running different profiles
  (a Pi gateway vs. a Pi desktop) — there is barely a split to fear.
- **Out of scope: MCU-class** (ESP32, Cortex-M, no MMU, KB of RAM). uBixOS's
  entire model doesn't fit; that would be a *different* OS, not this tree.
- **Out of scope: locked phone hardware** (proprietary GPU/baseband). "Mobile"
  means an ARM SBC with a touchscreen — still the same MMU-class tree.

### Consequence for this plan

`fbcon` (on-screen text *without* the desktop) is **promoted from "defer" to a
real phase**: it is what serves safe/recovery mode and screen-equipped headless
SBCs — a *console* feature, not a graphical one. Build the `kconsole`
abstraction first; `fbcon` is then just a `kconsole` sink the **base** profile
can use, independent of `views`.

## Strategy: AArch64 primary, i386 as the abstraction anchor

- **AArch64 is the primary feature target.** New features land there first; it
  is already the cleaner model (serial console + graphical desktop + pty
  terminal, *no* text VTs).
- **i386 stays — deliberately — as the abstraction anchor.** A second live
  architecture is the only thing that *proves* the MD interface is real. Drop it
  and "the interface" silently becomes "whatever aarch64 needs," and the split
  rots. Keeping i386 honest is a feature, not a tax.
- **Invert the polarity.** Stop treating i386 as the reference that aarch64
  chases. The **generic interface** is the reference; both arches implement it.
- **When is it OK to drop i386 support for something?** Rarely, and the answer is
  usually *modernise i386*, not *delete it*. Legitimate triggers: a genuinely
  obsolete mechanism blocking a clean generic design (V86/BIOS real-mode; the
  hardware-TSS task switch — already solved). Stable isolated drivers
  (NE2000/floppy/PIC/PIT/AT-kbd in `sys/isa/` + `sys/arch/i386/dev/`) are low
  ongoing cost and stay.

## Audit — where the split stands today

### Clean (preserve as-is)

- `<machine/X.h>` → `<arch/X.h>` forwarding headers (`sys/include/machine/`).
- The `md_proc` / `md_new_task` / `switch_to` / `md_setup_initial_frame`
  interface — same signatures both arches.
- The `g_*` hook pattern (`g_console_ops`, `g_tty_ops`, `g_tty_signal`,
  `g_tty_inject`): generic code queries a pointer; arch/driver code installs it.
- The shared `AARCH64_GENERIC_SRCS` core (~60 files: scheduler, VFS, VMM memory,
  ELF64 loader, lwIP, MPI, and now `tty.c` / `signal.c`).

### Leaky (the convergence backlog, by priority)

1. **Two syscall-dispatch philosophies.** i386 is 100% table-driven from the
   `.S` stub (`systemCalls[]` / `systemCalls_posix[]`). aarch64 hand-maps ~15
   hot-path calls in a 548-line `switch` in `sys/arch/aarch64/kern/syscall.c`,
   *then* falls through to the same shared table. Grows with every new syscall.
2. **fork/exec fully duplicated.** `aarch64_fork()` (`kern/proc.c`) and i386
   `fork.c` share nothing; the fd-table/signal-state/pgrp/cwd inheritance and
   proc setup are arch-neutral phases that aren't factored out. Same for
   `aarch64_exec_replace()` (`bringup/execfile.c`) vs `i386_exec.c`.
3. **`bringup/` naming debt.** `execfile.c` (701 LOC, the real exec path) and
   `ksupport.c` are production code sitting next to ~1,400 LOC of `*demo.c`
   scaffolding in a dir named "bringup."
4. **VA-layout magic numbers** (`MMAP_BASE`, `BRK_BASE`) hard-coded in
   `syscall.c` instead of a per-arch `vmm_layout.h`.
5. **Leaky `#if` guards** in generic files: `gen_calls.c` (`sys_invalid`,
   `sys_sysarch`), `tty.c` (VGA console, RS232 ISR, AT-kbd), `signal.c` (frame
   delivery — intentional, mirrors `tty.c`), `sched_dispatch.c` (IRQ restore).
   The `kprintf` per-arch sinks (below) are another.

Meta-point: **the interfaces are the right shape; what's missing is the
discipline to converge both arches onto them.** The fix is not more abstraction
layers — it is implementing-the-interface instead of reimplementing.

## Console / TTY redesign

### Target model (macOS-aligned)

aarch64 is *already* this model; i386 carries the legacy. The target for both:

- **One graphical console**, owned by the `views` compositor.
- **A boot/system console** for early boot, the pre-desktop login, and panics.
- **Serial** as an always-on debug sink.
- **All interactive shells are pty-backed GUI terminal apps** (the existing pty
  pool + VT100 line discipline) — the `Terminal.app` model.
- **No** user-switchable text VTs, **no** `ttyd`/getty-per-VT, **no** Alt-Fn
  console switching.

Dropping the legacy stack makes i386 converge onto what aarch64 already does —
so it shrinks the arch asymmetry *and* matches the product vision.

### Console-device abstraction (replaces hard-coded kprintf sinks)

Today `kprintf` hard-codes its sinks per arch (i386: VGA + COM1 always;
aarch64: PL011 only). Replace with a registered-sink model:

```c
struct kconsole {
    void (*putc)(char c);
    const char *name;
    unsigned flags;              /* KC_SERIAL | KC_PRIMARY | KC_SUSPENDABLE */
    struct kconsole *next;
};
void kconsole_register(struct kconsole *kc);
void kconsole_suspend_primary(void);   /* compositor claims the framebuffer */
void kconsole_resume_primary(void);    /* logout — or panic reclaims the screen */
```

`kprintf` formats to its buffer (as now), then walks the registered sinks. Each
arch registers what it has at boot:

- **Serial** (COM1 / PL011): always registered, never suspended.
- **Primary visible console**: selected at boot (config / boot-arg / detection).
  `views` calls `kconsole_suspend_primary()` when it maps the framebuffer; a
  **panic force-reclaims** the display (macOS verbose/panic behaviour).

This makes `kprintf` generic (removes a per-arch `#if`) and gives a single,
well-defined answer to "what is *the* console."

### Open decision — what backs the primary visible console

objGFX/font rendering is userland today, so an on-screen *kernel* text console
needs new kernel-side code. Two options:

- **Option A — serial-only kernel console; graphical-first screen.** Kernel
  prints to serial only; the screen is black until `views`/`vlogin` paints.
  Panics go to serial (later: a reclaimed-framebuffer red screen). *Simplest,
  most macOS-like, least code.* Downside: bare-metal box with no serial shows a
  black boot.
- **Option B — tiny built-in kernel framebuffer text console.** A small
  arch-neutral 8x16 bitmap-font blitter writing the linear framebuffer (i386
  VESA LFB / aarch64 virtio-gpu). On-screen boot log + on-screen panics on real
  hardware; unifies "VGA text" and "fb text" under one generic sink. *More work
  (kernel font + blitter), but the complete answer.* **This is exactly the
  deferred `fbcon.md` spec** — which already notes it should be revived for
  arm64 and generalised over a linear-framebuffer descriptor rather than the
  VESA/multiboot specifics. Option B = build `fbcon.md` as a `kconsole` sink.

**Decided (2026-06-09): both, staged.** Ship Option A as Phase 1 (the `kconsole`
abstraction with serial + a suspendable primary). Then build Option B (`fbcon`)
as a Phase — it is required by the product identity (safe-mode + screen-equipped
headless SBCs), not optional. A is the foundation; B is a sink that plugs into
it.

### What we keep vs delete

| Keep (portable / valuable) | Delete (i386-only legacy) |
|---|---|
| pty pool + VT100 line discipline (`tty.c`) | `bin/ttyd/`, `/etc/ttys`, getty-per-VT |
| `g_tty_ops` / `g_tty_inject` / `g_tty_signal` hooks | `tty_change`, `tty_foreground`, VGA console slots |
| One system/boot console + serial | `vesa_text_slot` / `tty_switch_slot` / `vesa_text_mode` |
| Graphical login (`vlogin`) + GUI terminal | Alt-Fn VT switching in `atkbd.c`; `kbd_gui_mode` dance |

## Phased plan (each phase boots on **both** arches)

**Phase 1 — Console-device abstraction.**
Introduce `struct kconsole` + `kconsole_register` + suspend/resume. Make
`kprintf` walk registered sinks. i386 registers a serial sink + a (suspendable)
VGA/primary sink; aarch64 registers a PL011 serial sink. Remove the per-arch
`#if` from `kprintf.c`. *Behaviour-preserving; lowest risk; prerequisite for
Phase 2.*

**Phase 2 — Drop the multi-VT / ttyd stack.**
Delete `ttyd`, the VGA console slots, `tty_change`/`tty_foreground`, the Alt-Fn
switching and `vesa_text_*` dance. Collapse the tty slot array to "one system
console + pty pool" (retire `TTY_PTY_BASE`). Pre-desktop login uses the single
system console; the desktop owns the display via `views`. Removes a large block
of i386-only complexity and the leaky `tty.c` guards shrink. *Mostly deletion.*

**Phase 3 — Unify syscall dispatch.**
Collapse the aarch64 hand-rolled `switch` into a thin trap shim that fills
`register_t args[]` and calls the shared table-driven `ksyscall_dispatch`. Move
the arch-special calls (mmap VA layout, brk, fork, execve) behind `md_*` hooks
the table's handlers call — so they are *implementations of the interface*, not
dispatcher special-cases. Highest structural value; do with care.

**Phase 3.5 — `fbcon` as a `kconsole` sink.**
Build the deferred `fbcon.md` spec, generalised over a linear-framebuffer
descriptor (i386 VESA LFB / aarch64 virtio-gpu) with a small built-in 8x16
bitmap font, registered as a `KC_PRIMARY | KC_SUSPENDABLE` sink. Gives on-screen
boot log, safe/recovery-mode text, and on-screen panic — independent of `views`.
This is what makes the *base* (non-graphical) profile usable on a
screen-equipped SBC.

**Phase 4 — Image profiles.**
Formalise the userland split into image profiles built from one tree: a **base**
profile (no `views`; `init` runs a console shell — the headless/IoT/safe-mode
config) and a **desktop** profile (base + `views` + `vlogin` + apps). Same
kernel; the profile selects what `mkimage*` stages and what `init` launches. The
existing `mkimage-arm.sh` / `mkimage.sh` grow a profile knob.

**Phase 5 — Factor fork/exec + graduate `bringup/` + centralise VA layout.**
Extract the arch-neutral fork/exec phases into generic code with
`md_copy_space()` / `md_setup_exec_frame()` hooks. Move `execfile.c` /
`ksupport.c` out of `bringup/` into `kern/`; gate or remove the `*demo.c` files
behind a build flag. Put `MMAP_BASE` / `BRK_BASE` / stack base into a per-arch
`vmm_layout.h`.

## Status

- **Plan drafted** (this doc). No phases started.
- Prereqs already in place from prior work: dual-arch `signal.c` + tty job
  control (see `project_aarch64_signals`), the `g_*` hook pattern, `md_proc`,
  the `machine/` forwarding headers.

## Decisions (resolved 2026-06-09)

1. **Console Option A *and* B, staged.** A (`kconsole`) is Phase 1; B (`fbcon`)
   is Phase 3.5 — required by the product identity, not optional.
2. **Keep a single system console** for boot / pre-desktop login / panic (the
   console-first foundation). The base profile's `init` runs its shell there; no
   switchable VTs.
3. **No emergency text VT.** Recovery is serial or the `fbcon` base-profile
   console — not a switchable Unix VT.
4. **Product identity locked:** console-first, graphical-optional, one tree,
   profile-driven, MMU-class only (see *Product identity* above).
