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

**Phase 1 — Console-device abstraction. ✅ DONE (2026-06-09).**
Introduce `struct kconsole` + `kconsole_register` + suspend/resume. Make
`kprintf` walk registered sinks. i386 registers a serial sink + a (suspendable)
VGA/primary sink; aarch64 registers a PL011 serial sink. Remove the per-arch
`#if` from `kprintf.c`. *Behaviour-preserving; lowest risk; prerequisite for
Phase 2.*

Landed as:
- `sys/include/lib/kconsole.h` / `sys/lib/kconsole.c` — the registered-sink
  layer (FreeBSD `consdev`-style `void (*putc)(int)`): `kconsole_register`,
  `kconsole_emit`, `kconsole_suspend_primary` / `kconsole_resume_primary`, and a
  generic per-arch `kconsole_arch_init()` contract.
- `sys/lib/kprintf.c` — the per-arch console `#if` is gone; one arch-neutral
  `kprintf` that formats then calls `kconsole_emit`. (The only remaining `#if`
  is the i386 64-bit quad-division helpers — an ISA concern, not console.)
- `sys/arch/i386/dev/console.c` — COM1 sink (`KC_SERIAL`, CR/LF) + VGA sink
  (`KC_PRIMARY | KC_SUSPENDABLE`, honours the legacy `printOff` mute). The VGA
  sink calls the new `kprint_putc()` in `sys/sys/video.c`, which preserves the
  cursor / scroll / `tty_foreground` reroute behaviour of `kprint()`.
- `sys/arch/aarch64/dev/uart.c` — PL011 sink (`KC_SERIAL`); its old `kprintf`
  is deleted (the entry point is now the shared one).
- Registration: `kconsole_arch_init()` runs early in `kmain` (i386) and
  `kmain_aarch64`, before the first `kprintf`.

**Design note — line endings stay per-sink.** Stock FreeBSD centralises
`\n`→CR/LF in `cnputc`; uBixOS does not, because the VGA sink gives `\n`
full-newline semantics (column reset + line advance) and a centralised stray
`\r` would render as a glyph. So the serial sinks translate and the VGA sink
does not — each console device owns its own line discipline.

*Verified:* i386 boots to `Login:` (COM1 + VGA sinks); aarch64 boots through all
bring-up demos over the PL011 sink. The `tty_foreground` reroute is preserved
(Phase 2 removes it). The suspend/resume API is in place but not yet wired to
`views`' framebuffer claim — that is Phase 2 work.

**Phase 2 — Drop the multi-VT / ttyd stack. 🟢 Mostly done (2026-06-09).**
Delete `ttyd`, the VGA console slots, `tty_change`/`tty_foreground`, the Alt-Fn
switching and `vesa_text_*` dance. Collapse the tty slot array to "one system
console + pty pool" (retire `TTY_PTY_BASE`). Pre-desktop login uses the single
system console; the desktop owns the display via `views`. Removes a large block
of i386-only complexity and the leaky `tty.c` guards shrink. *Mostly deletion.*

Landed (each boot-verified on both arches):
- **2.1** (commit `ca1bc2c69`) — removed the user-switchable VT switching:
  the Alt-Fn (`tty_switch_slot`) and Ctrl+Alt+Fn (`vesa_text_slot`) keyboard-ISR
  handlers in `atkbd.c`/`hid_kbd.c`, their `sys_read` poll-loop consumers, and
  the `tty_change()` VGA-buffer-swap function.
- **2.2** (commit `bc035eccb`) — retired `ttyd`, `/etc/ttys`, and
  `/etc/init.d/30-ttyd`. `init` now launches a single system-console primary
  directly: `start_console()` forks a child that claims slot 0 with `settty(0)`
  and respawns `CONSOLE_PRIMARY` (`/bin/views`), matching the aarch64 model
  (its kernel `boot.c` already spawns `/bin/views` directly). One system
  console, no getty-per-VT, no switchable text VTs.
- **2.3 partial** (commit `cde4288ed`) — removed the dead `/dev/ttyv1-3`
  multi-VT device nodes (no getty serves them). Kept `ttyv0` (system console)
  and `com1` (serial console).
- Also (commit `9a2df0986`) — arch-dispatched the `image` target so
  `bmake image TARGET=aarch64` writes `ubixos-arm.img`, not the i386
  `ubixos.img`. Previously `bmake TARGET=aarch64` (which runs `all`) silently
  clobbered the i386 image with aarch64 binaries, which then failed every i386
  exec with `e_type != ET_EXEC` and looked like a kernel regression.

**Deferred from Phase 2 (do with the console-read-path rework, with interactive
verification):**
- **Slot-array collapse** — renumber so it is just "slot 0 = VGA system console,
  slot 1 = COM1 serial, pty pool from slot 2" (retire `TTY_PTY_BASE 5`). This is
  entangled: `sys_settty` (`sys/kern/fb.c`) hard-codes "slots 0-3 = VGA, 4+ =
  serial", the pty pool starts at slot 5, and renumbering shifts the
  **GUI-terminal pty pool** — which the headless boot smoke-test does *not*
  exercise. Needs an interactive terminal test (open the GUI terminal, type, run
  `echo | cat`) before it can be trusted.
- **`tty_foreground` → `&terms[0]`** — unsafe to fold blindly: `tty_foreground`
  is `NULL` until `tty_init`, and `kprint` keys off that to route early-boot
  output to raw VGA instead of dereferencing an unallocated `terms[0]`.
- **`kbd_gui_mode`** — still load-bearing: it is what stops the VGA console's
  `sys_read` loop from draining the keyboard while `views` owns the screen.
  Cannot go until the VGA console-read path itself is reworked.
- **`vesa_text_mode()`** — *not* retired; the remaining caller (`systemtask`, on
  GUI-process exit) is the legitimate GUI→text transition, not the
  already-removed Ctrl+Alt+Fn machinery.

**Phase 3 — Unify syscall dispatch. 🟢 Largely done (2026-06-09).**
Collapse the aarch64 hand-rolled `switch` into a thin trap shim that fills
`register_t args[]` and calls the shared table-driven `ksyscall_dispatch`. Move
the arch-special calls (mmap VA layout, brk, fork, execve) behind `md_*` hooks
the table's handlers call — so they are *implementations of the interface*, not
dispatcher special-cases. Highest structural value; do with care.

Landed (each boot-verified on both arches, headlessly on real FAT via the new
`tools/aarch64-user/dirtest.c` harness):
- **Return convention** (`c32aad5fc`): both dispatchers already return
  `td_retval[0]`; the C-return getters (`sys_getUID`/`getGID`/`getEUID`) were
  converted to set it, and 5 getter pre-cases dropped.
- **The HVF `isv` blocker is gone** (`206f8b8d4`): it was a downstream symptom of
  the `sc_args=0` table bug (pipe2/readlink), not a real handler problem. Once
  fixed, the redundant pointer-arg pre-cases route through the table cleanly —
  pruned statx/open/read/close/fcntl/ioctl/getdents (`206f8b8d4`/`20a1fdee2`/
  `c02f0914d`) and uname/sched_yield/set_tid_address (`d9839176b`, after making
  `sys_uname` report the arch machine name).
- **One time source** (`23414a610`): `md_uptime(sec,nsec)` (aarch64 CNTVCT / i386
  PIT) backs a single shared `gettimeofday`, `clock_gettime`, and lwIP `sys_now`;
  the `clock_gettime` pre-case is pruned with its ns resolution preserved.

The aarch64 dispatch `switch` is now only the irreducibly arch-special calls.
**Remaining (deferred):** `mmap`/`brk`/`munmap` need real `md_*` VA-layout hooks
(a larger VMM refactor); `nanosleep` (yield-once today) → a real timed sleep is a
*feature*, not a prune; `fork`/`execve`/`wait4`/`exit` are trampoline-level and
stay by design; `openat` (maps to `open`) and `setuid`/`setgid` (euid/egid
reconcile) are minor, low-value cleanups.

**Scoping (2026-06-09): it's a refactor, not a prune.** The dispatch is *already*
table-driven (the `switch` in `arch/aarch64/kern/syscall.c` falls through to
`ksyscall_dispatch`), but the ~25 explicit pre-cases are **not** all redundant.
Three real obstacles, each must be cleared before a pre-case can be deleted:

1. **Return-convention mismatch (the big one).** `ksyscall_dispatch`
   (`sys/kern/syscall_dispatch.c`) returns `td->td_retval[0]`; the i386 asm
   dispatch returns the handler's **C return value** (in `eax`).  Many handlers
   use the C-return convention and never set `td_retval[0]` — e.g. `sys_getUID`
   (`sys/kern/access.c`: `return _current->uid`), and the other getters.  Routed
   through `ksyscall_dispatch` they'd return 0, so the aarch64 pre-cases return
   the value directly to compensate.  Fix: pick ONE convention (FreeBSD sysent:
   set `td_retval[0]`, return 0/-errno), convert the C-return handlers, and make
   the i386 dispatch also return `td_retval[0]`.  Then both dispatchers are
   identical and the getters' pre-cases delete cleanly.
2. **Syscall-number gaps.** Some pre-cases paper over a wrong/absent table slot
   — e.g. aarch64 `getdents` is FreeBSD **272**, but `getdirentries` sits at table
   slot **196**; deleting that pre-case would dispatch 272 to the wrong/empty
   entry.  Put the handler at the number musl actually emits (or alias it).
3. **HVF `isv` host assertion.** A prior trial prune (statx / clock_gettime /
   fcntl / munmap / nanosleep / sched_yield) tripped `assert(isv)` in HVF during
   `ls`.  Likely a downstream effect of #1/#2 feeding a handler a bad arg/return,
   but unconfirmed — bisect under `-accel tcg` (which reports the faulting
   address) by routing one syscall at a time through the table and exercising
   `ls`/`getdents`/`stat` in the terminal.

Irreducibly arch-special (keep, ideally behind `md_*` hooks): the ABI trampoline,
fork (needs the trapframe), execve (dynamic loader), mmap/brk/munmap (arch VA),
the `write`/`writev` UART-vs-fileops shortcut, exit.  Start with #1 (it's the
foundation and unblocks the bulk of the prune); #2 and #3 follow.

**Phase 3.5 — `fbcon` as a `kconsole` sink. ✅ DONE (aarch64, 2026-06-10, `6db8e3dc1`).**
`sys/arch/aarch64/dev/fbcon.c`: an 8x8-glyph `KC_PRIMARY | KC_SUSPENDABLE`
kconsole sink rendering into the virtio-gpu scanout, registered after GPU init,
suspended by `sys_mapfb` when `views` claims the screen.  On-screen boot log /
panic / base-profile console, independent of `views`, with serial always on.
i386 keeps VGA text + COM1 (its VESA LFB is owned by `views`; a kernel fb
console would conflict) — done for the arch that needs it.

**Phase 4 — Image profiles. ✅ DONE (aarch64, 2026-06-10, `a7837bcdb`).**
`mkimage-arm.sh PROFILE=base|desktop`: a **base** image ships the CLI world only
(no compositor/apps/desktop libs/resources); a **desktop** image is the full
stack.  `boot.c` branches on `/bin/views` being staged — present → the
desktop chain, absent → the BASE console chain (`authd` + `/bin/login` on the
kernel console, fbcon + serial).  Same kernel.  Verified: base image boots to the
console login; desktop image unchanged.  *Follow-up:* the parallel i386 profile
(init-side branch in `mkimage.sh`), and on-screen keyboard input for a
screen-equipped (non-serial) base console.

**Phase 5 — Factor fork/exec + graduate `bringup/` + centralise VA layout.**
Extract the arch-neutral fork/exec phases into generic code with
`md_copy_space()` / `md_setup_exec_frame()` hooks. Move `execfile.c` /
`ksupport.c` out of `bringup/` into `kern/`; gate or remove the `*demo.c` files
behind a build flag. Put `MMAP_BASE` / `BRK_BASE` / stack base into a per-arch
`vmm_layout.h`.

*Progress (2026-06-10):*
- ✅ **VA layout centralised** (`b4b73d62f`) — `MMAP_BASE`/`BRK_BASE` +
  `DYN_*`/`USER_STACK_*` consolidated into `<aarch64/vmm_layout.h>`; syscall.c +
  execfile.c drop their local defines, no value change.
- ✅ **`fork_copy_fdtable` factored** (`a928a1b3e`) — the open-file-table
  deep-copy (was inline in i386 `sys_fork` + a static in `aarch64_fork`) is now
  one MI helper in `sys/kern/kern_fork.c`. Byte-identical to the i386 inline, so
  i386 behaviour preserved; aarch64 boots to desktop.
- ✅ **Fork body converged** (`fc6c2da6f`) — `proc_fork_inherit_context()` +
  `proc_fork_signal_init()` in kern_fork.c; both `sys_fork`/`aarch64_fork` call
  them. i386 keeps only TSS-frame + `vmm_copy_virtual_space`; aarch64 keeps
  `pmap_fork_copy` + the kstack frame. signal_init runs after the AS copy
  (i386 hazard respected). aarch64 now also clears pending signals + inherits
  dispositions/QoS/attached-tty on fork (previously did neither). **Boot-tested
  both arches** (i386 image built this session): i386 VESA desktop + SIGCHLD
  reaping, aarch64 graphical desktop.
- ✅ **Graduate `bringup/` done** (`977a5be73` + `6d0d9e1e4`) — the `*demo.c`
  calls are behind `AARCH64_BRINGUP_DEMOS` (off by default; `procfs`/`ramfs`
  demos stay for their real side effects), and the production code
  (`execfile.c`/`ksupport.c`) moved out of `bringup/` into `kern/`, so `bringup/`
  is now pure scaffolding.  The gating blocker was NOT a race but the scheduler
  bootstrap (`sched_init`+`set_current`) buried in `aarch64_sched_demo` — moved
  to `kmain_aarch64`.  4/4 desktop with demos gated.
- ✅ **SVC dispatch at its irreducible minimum** — the aarch64 syscall switch is
  down to the genuinely arch-special calls (write/writev UART path, mmap/brk/
  munmap/mprotect over the arch VA, fork/execve/wait4/exit needing the trapframe,
  openat, nanosleep, setuid/setgid); everything else falls through to the shared
  `systemCalls_posix[]` table.  Further "routing" would just re-implement
  arch-specific behaviour in the table — so this is the convergence end-state.
- 🟢 **exec convergence — neutral glue done** (`1e961e755`) —
  `exec_set_name_cmdline()` in `sys/kern/kern_exec.c` (basename→name +
  argv-join→cmdline) shared by `sys_exec` + `aarch64_exec_replace`. aarch64 now
  gets a clean process name + cmdline (was the bare path, no args). **The ELF
  loaders stay arch-specific by design** — i386 maps ELF32 segments via the i386
  vmm; aarch64 loads ELF64 PIE via pmap. These are different ELF classes + VM
  APIs; a shared loader would be worse architecture, not convergence. So exec is
  "converged" to the extent it should be; no shared `md_setup_exec_frame()` hook
  is warranted.

## Status

- **Phase 1 done** (2026-06-09) — the `kconsole` registered-sink abstraction;
  see the Phase 1 entry above. Verified on both arches.
- **Phase 2 mostly done** (2026-06-09) — VT switching, `ttyd`/`/etc/ttys`, and
  the `/dev/ttyv1-3` nodes are gone; `init` launches the console primary
  directly. The deeper slot-array collapse + `kbd_gui_mode`/`tty_foreground`
  fold are deferred (entangled with the VGA console-read path + GUI-terminal pty
  pool; need interactive verification). See the Phase 2 entry above.
- **Phase 3 largely done** (2026-06-09) — see the Phase 3 entry.
- **Phase 5 essentially complete** (2026-06-10) — VA layout centralised; fork
  body converged (`fork_copy_fdtable` + context/signal helpers); exec name/cmdline
  glue factored (loaders stay arch-specific by design); demos gated + `bringup/`
  graduated; SVC switch at its irreducible arch-special minimum. All verified,
  both arches green. The remaining deferred items (COW fork, TTBR1 kernel/user
  split) are aarch64 VMM *refinements*, not convergence — tracked in
  cross-arch-plan.md.
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
