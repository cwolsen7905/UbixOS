# Software Display Environment (SDE)

The SDE is UbixOS's kernel-side graphics subsystem. It owns the physical
framebuffer, manages VESA mode initialization, and acts as a compositor for
userland windows. It is the kernel-side counterpart to the `objgfx40` graphics
library used by userland programs.

---

## Layer Overview

```
Userland                         Kernel
────────────────────────────────────────────────────────────────────
bin/muffin, bin/launcher
  │
  ├─ lib/objgfx40/               objgfx40 — pixel operations, surfaces,
  │    vWindow, ogSurface,         fonts, image loading (BMP), drawing
  │    ogImage, ogBitFont          primitives (lines, rects, gouraud fill)
  │
  └─ vWindow::vSDECommand()      ──► int 0x81, slot 40 ──► sysSDE()
                                                              │
                                                       sys/sde/sde.cc
                                                              │
                                                    sdeThread() compositor
                                                              │
                                                   sys/sde/ogDisplay_UbixOS.cc
                                                              │
                                                    biosCall() → VM86 → INT 10h
                                                              │
                                                       VESA BIOS (LFB)
```

---

## VESA Mode Initialization

### The Problem: Protected Mode Cannot Call BIOS

Once the CPU is in 32-bit protected mode, BIOS interrupt services (`INT 10h`
for VESA/VBE) are inaccessible. Protected mode segments and real-mode segments
are incompatible.

### The Solution: VM86 Mode via a Kernel Task

`biosCall()` in `sys/arch/i386/bioscall.c` works around this by spawning a
disposable kernel task configured to run in **Virtual 8086 (VM86) mode** —
an x86 feature that emulates real mode inside protected mode.

The mechanism step by step:

1. `biosCall()` locates `bios16Code` (a stub that issues the real `INT` instruction)
   and computes its real-mode segment:offset address.

2. A new kernel task is created via `schedNewTask()` with its TSS configured
   so that `EFLAGS.VM = 1`. This puts the task into VM86 mode when scheduled.

3. The TSS registers (`eax`, `ebx`, `ecx`, `edx`, `es`, `ds`, etc.) are loaded
   with the arguments that the BIOS call expects. `cs:eip` points at `bios16Code`.

4. The task state is set to `READY`. The calling thread spins on `sched_yield()`
   until the VM86 task completes (state returns to 0).

5. The VM86 task runs `bios16Code`, which issues the real `INT 10h`. The BIOS
   handles the call and returns. The task exits.

```c
/* biosCall signature — sys/arch/i386/bioscall.c */
void biosCall(int biosInt, int eax, int ebx, int ecx,
              int edx,    int esi, int edi, int es,  int ds);
```

This is a **synchronous, blocking call** from the kernel's perspective — the
caller yields until the VM86 task finishes. It is only safe to call during
early init before the compositor loop is running.

---

## VESA/VBE Call Sequence

All three VBE calls go through `biosCall(0x10, ...)`:

| Function | VBE call | Purpose |
|---|---|---|
| `GetVESAInfo()` | `INT 10h, AX=4F00h` | Read VBE controller info into `0x11000` |
| `GetModeInfo(mode)` | `INT 10h, AX=4F01h` | Read mode descriptor into `0x11200` |
| `initVESAMode(mode)` | `INT 10h, AX=4F02h` | Set the video mode (enables LFB) |

VBE info structures are placed at fixed physical addresses (`0x11000` for
`ogVESAInfo`, `0x11200` for `ogModeInfo`) that are below the 1 MB line and
accessible from both real mode and protected mode.

### Mode Selection

`ogDisplay_UbixOS::FindMode(xRes, yRes, BPP)` iterates mode numbers
`0x100`–`0x1FF`, calling `GetModeInfo()` on each until it finds a mode that
satisfies the requested resolution and bit depth. If a 24 bpp mode is not
found it retries with 32 bpp (and vice versa).

### Linear Framebuffer Mapping

Once a mode is selected, `SetMode()`:

1. Sets bit 14 (`0x4000`) of the mode number to request the **Linear
   Framebuffer** (LFB) instead of banked mode.
2. Reads `modeInfo->physBasePtr` — the physical address of the LFB.
3. Calls `vmm_remapPage()` for each 4 KB page of the framebuffer to map the
   physical LFB address into the kernel's virtual address space at the same
   address (identity map).
4. Calls `initVESAMode()` to actually switch the hardware to the new mode.

After this, `ogSurface::buffer` points directly at the LFB and pixel writes
go straight to the screen.

---

## The SDE Compositor (`sys/sde/`)

### sdeThread

`sdeThread()` in `sys/sde/main.cc` is a kernel thread that owns the display.
It is **not currently spawned on boot** — the call in `sys/init/main.c` is
commented out:

```c
/* execThread(&sdeThread, 0x2000, 0x0); */
```

When enabled, `sdeThread`:

1. Creates an `ogDisplay_UbixOS` (triggering VESA init via `biosCall`).
2. Calls `ogCreate(800, 600, OG_PIXFMT_24BPP)` to find and set the best
   matching VESA mode.
3. Clears the screen to a slate-blue background (`0x92, 0xA8, 0xD1`).
4. Stores the screen surface pointer in `systemVitals->screen` so the rest
   of the kernel (and `kprintf`) can use it.
5. Spawns `sdeTestThread` as a second kernel thread (demo / test rendering).
6. Enters a compositor loop that services the `windows` linked list.

### Window Management

The compositor maintains a doubly-linked list of `sdeWindows` nodes:

```c
struct sdeWindows {
    struct sdeWindows *next;
    struct sdeWindows *prev;
    void   *buf;       /* ogSurface* allocated in kernel space */
    pidType pid;       /* owning process */
    uint8_t status;    /* registerWindow | windowReady | drawWindow | killWindow */
};
```

Status values (defined in `sys/include/sde/sde.h`):

| Constant | Value | Meaning |
|---|---|---|
| `registerWindow` | 1 | New window being registered; needs buffer mapping |
| `windowReady` | 2 | Window mapped and idle |
| `drawWindow` | 3 | Userland requested a blit to screen |
| `killWindow` | 4 | Userland requested window destruction |

The compositor loop services each window in order:

- **`registerWindow`**: Copies the userland `ogSurface` descriptor into kernel
  memory, then calls `vmm_mapFromTask()` to map the userland pixel buffer and
  `lineOfs` table into the kernel's address space. Sets status → `windowReady`.
- **`drawWindow`**: Blits the window's surface onto the screen via
  `ogCopyBuf()`. Sets status → `windowReady`.
- **`killWindow`**: Unmaps the pixel buffer and `lineOfs` with
  `vmm_unmapPages()`, removes the node from the list, frees it.
- **default (idle)**: Calls `sched_yield()` to avoid busy-spinning.

---

## Userland Side: vSDECommand

Userland programs communicate with the compositor via UbixOS native syscall
slot 40 (`int 0x81`). The `vWindow` class in `lib/objgfx40/` wraps this:

```c
void vWindow::vSDECommand(uint32_t command) {
    asm("int %0" : : "i"(0x81), "a"(40), "b"(command), "c"(realWindow));
}
```

The `realWindow` pointer (an `ogSurface*`) is passed as the data argument.
`sysSDE()` in the kernel receives it via `args->ptr` and dispatches on
`args->cmd`:

| Command | vSDECommand arg | Kernel action |
|---|---|---|
| 1 (`registerWindow`) | surface ptr | Map userland buffers into kernel, add to window list |
| 3 (`drawWindow`) | surface ptr | Blit window to screen |
| 4 (`killWindow`) | surface ptr | Unmap buffers, remove from list |

The call is **synchronous and spin-waiting** — `sysSDE` for `drawWindow`
sets the window status to `drawWindow` and then busy-waits (`nop` loop) until
`sdeThread` sets it back to `windowReady`.

---

## Class Hierarchy (objgfx40)

```
ogSurface               — pixel buffer, drawing primitives, pixel format
    │
    ├─ ogDisplay_UbixOS — kernel-side: VESA init, LFB mapping  (sys/sde/)
    │
    └─ vWidget          — userland base widget                  (lib/objgfx40/)
         │
         ├─ vWindow     — top-level window, vSDECommand glue
         └─ vButton     — clickable button widget
```

`ogSurface` is shared between kernel (`sys/sde/`) and userland
(`lib/objgfx40/`) — the same class compiles into both, with the kernel build
adding `ogDisplay_UbixOS` on top.

---

## Key Source Files

| File | Role |
|---|---|
| `sys/sde/main.cc` | `sdeThread` — compositor loop and `sdeTestThread` demo |
| `sys/sde/sde.cc` | `sysSDE` — syscall handler, window list management |
| `sys/sde/ogDisplay_UbixOS.cc` | VESA mode detection, LFB mapping, `initVESAMode` |
| `sys/sde/colours.cc` | `sdeTestThread` and `sdeTestThreadOld` — rendering demos |
| `sys/arch/i386/bioscall.c` | `biosCall` — VM86 task dispatch for BIOS INT calls |
| `sys/include/sde/sde.h` | `sdeWindows` struct, window status constants, `sysSDE` decl |
| `sys/include/sde/ogDisplay_UbixOS.h` | `ogModeInfo`, `ogVESAInfo` VBE structs, `ogDisplay_UbixOS` class |
| `sys/include/lib/bioscall.h` | `biosCall` declaration, EFLAG constants |
| `lib/objgfx40/vWindow.cpp` | Userland `vWindow`, `vSDECommand` inline asm |
| `lib/objgfx40/objgfx40.cpp` | `ogSurface` drawing primitives |

---

## Current Status and Known Gaps

- **SDE is not started on boot.** The `execThread(&sdeThread, ...)` call in
  `sys/init/main.c` is commented out. Enabling it is the first step to getting
  graphics working.

- **`muffin`** (`bin/muffin/`) is the intended test program. It creates a
  `vWindow`, registers it with the SDE, loads a BMP background, and runs a
  color-cycling render loop. It is not wired into the world build yet.

- **`launcher`** (`bin/launcher/`) is an embryonic desktop/taskbar. Its
  `vDraw()` methods are empty stubs.

- **The `drawWindow` path is spin-waiting**, not interrupt/signal driven.
  Under load this wastes CPU in both the userland caller and the compositor.

- **No mouse or keyboard input routing** exists in the SDE layer. The ISA
  keyboard and mouse drivers in `sys/isa/` produce events but there is no path
  from those events to a focused window.

- **`vmm_mapFromTask`** used by `registerWindow` maps the userland pixel buffer
  into kernel space permanently until `killWindow`. There is no reference
  counting; a crashed userland process leaks the mapping.
