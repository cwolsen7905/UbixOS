# UbixOS Display System Design Plan

## Goal

Build a graphical display system that gets a taskbar on screen quickly, then
evolves cleanly toward a macOS-style composited window server — without ever
being locked into a command-protocol architecture like BeOS/X11.

The core design principle: **shared memory buffers, MPI as signaling only.**
Apps render however they want into their own buffer. The display server
composites. The MPI protocol never carries drawing commands — only
"I'm ready" / "give me a region" / "take it back." This keeps the upgrade
path to GPU-accelerated compositing open without protocol changes.

---

## How Other Systems Did It

| System | Who owns framebuffer | App draws how | Compositor |
|--------|---------------------|---------------|------------|
| Linux/X11 | X server (userspace) | Sends draw commands over socket | X server |
| Linux/Wayland | Compositor owns it | Client renders into shared buffer, signals | Compositor |
| macOS/Quartz | WindowServer (userspace) | Render into IOSurface, signal via Mach port | WindowServer |
| BeOS/app_server | app_server (userspace) | Sends draw commands over IPC | app_server |
| **UbixOS** | `display` server (userspace) | Render into shared vmm region, signal via MPI | `display` server |

UbixOS is closest to Wayland in concept, but uses MPI instead of Unix sockets
and `vmm_share_region` instead of DMA-BUF. The simplicity of MPI makes the
protocol trivial to implement and trivial to replace later.

---

## Current State

- VESA framebuffer working in kernel. `vesa_fb_paddr`, `vesa_width`,
  `vesa_height`, `vesa_bpp`, `vesa_pitch` populated after `vesa_init()`.
- SDE (`sys/sde/`) is a kernel-side C++ graphics layer (objgfx40). Build is
  broken due to a path issue with `ogFont.cpp`. Contains the UbixOS boot
  splash logic and MPI listener stub.
- No userspace framebuffer access path exists yet.
- MPI mailbox system is working and used by `init`/`login`.

---

## Phases

### Phase 1 — Fix SDE Build
**Visible result:** None. Prerequisite.
**Risk:** None.

Fix the `ogFont.cpp` path error in `sys/sde/Makefile` so the kernel C++
graphics layer compiles cleanly. The SDE is needed for the boot splash and
for the kernel-side display initialisation that hands ownership to userspace.

---

### Phase 2 — `sys_mapfb()` Kernel Syscall
**Visible result:** Userspace can write pixels.
**Risk:** Low.

New POSIX syscall (slot TBD) that maps the VESA framebuffer physical pages
into the calling process's virtual address space. Returns a struct:

```c
struct fb_info {
    void    *base;    /* virtual address of framebuffer in caller's space */
    uint32_t width;
    uint32_t height;
    uint16_t pitch;   /* bytes per scanline */
    uint8_t  bpp;
};
```

Initial implementation: no access control. First process to call it wins.
Phase 5 tightens this to display-server-only.

**New files:**
- `sys/kernel/syscalls_posix.c` — add `sys_mapfb` entry
- `sys/include/sys/fb.h` — `struct fb_info` definition

---

### Phase 3 — `lib/libfb/` Userspace Graphics Library
**Visible result:** Can draw from any userspace program.
**Risk:** None (pure userspace).

Thin library on top of `sys_mapfb()`. No dependencies beyond libc.

**API:**
```c
int      fb_open(struct fb_info *out);   /* calls sys_mapfb */
void     fb_pixel(int x, int y, uint32_t color);
void     fb_rect(int x, int y, int w, int h, uint32_t color);
void     fb_rect_outline(int x, int y, int w, int h, uint32_t color);
void     fb_blit(int dx, int dy, int w, int h,
                 const uint32_t *src, int src_pitch);
void     fb_text(int x, int y, const char *s, uint32_t fg, uint32_t bg);
void     fb_clear(uint32_t color);
```

Font: 8×16 bitmap font embedded as a C `uint8_t[]` array in `libfb`.
No external font files needed.

**New files:**
- `lib/libfb/fb.c`, `lib/libfb/font8x16.c`, `lib/libfb/Makefile`
- `include/fb.h`

---

### Phase 4 — `bin/display/` Server + Hardcoded Taskbar ★ TASKBAR ON SCREEN ★
**Visible result:** Taskbar drawn at the bottom. Clock. Launch button.
**Risk:** Low (pure userspace).

The `display` server is the first graphical process. It:

1. Calls `sys_mapfb()` to own the framebuffer.
2. Draws the desktop background.
3. Draws a hardcoded taskbar at the bottom (32px high):
   - Left: UbixOS logo / launcher button
   - Right: clock (updated each second via `sleep(1)` loop)
4. Registers MPI mailbox `"display"` and enters its event loop.

The taskbar is **compiled into** `display` at this phase — no separate process.
This gets pixels on screen with minimum moving parts.

**New files:**
- `bin/display/display.c` — main loop, fb ownership, taskbar draw, MPI listener
- `bin/display/Makefile`
- `bin/Makefile` — add `display` to SUBDIRS

**Install:** `display` added to `bin/init`'s startup sequence so it launches
before `login` returns to the shell. Or run manually from the shell first.

---

### Phase 5 — MPI Display Protocol
**Visible result:** Nothing new yet. Foundation for apps to draw windows.
**Risk:** None (protocol only).

Define the message types `display` will honour. Keep them minimal.

```c
/* Client → display */
#define DISPLAY_CLAIM   1  /* request a window region */
#define DISPLAY_FLIP    2  /* buffer updated, please composite */
#define DISPLAY_RELEASE 3  /* window closing */

/* display → client */
#define DISPLAY_ACK     4  /* region granted; carries window_id + shm token */
#define DISPLAY_DENIED  5
#define DISPLAY_KEY     6  /* keyboard event forwarded to focused window */
#define DISPLAY_MOUSE   7  /* mouse event */

struct display_claim_req {
    int x, y, w, h;
    char title[64];
};

struct display_ack {
    uint32_t window_id;
    void    *shm_base;   /* mapped into client's space — Phase 6 */
    uint16_t pitch;
};
```

`display` adds a stub handler that receives CLAIM messages and logs them.
No shared memory yet — that's Phase 6.

---

### Phase 6 — Shared Memory: `vmm_share_region()`
**Visible result:** Apps can draw into their own buffer and have it composited.
**Risk:** Medium (kernel VMM change).

New kernel primitive:

```c
/* Map [vaddr, vaddr+size) from src_pid into dst_pid's address space.
 * Returns the virtual address in dst_pid's space, or 0 on failure. */
uintptr_t vmm_share_region(pidType src_pid, uintptr_t vaddr,
                            size_t size, pidType dst_pid);
```

`display` uses this on receiving a CLAIM request:
1. Allocates a buffer (via `vmm_allocPageTable` or `kmalloc`).
2. Calls `vmm_share_region` to map it into the client's address space.
3. Sends the mapped address back in `DISPLAY_ACK`.

Client now writes pixels directly to its buffer at user speed — no syscall per
pixel. When ready, sends `DISPLAY_FLIP`. `display` reads from the buffer and
blits to the framebuffer.

---

### Phase 7 — `bin/taskbar/` as a Separate Process
**Visible result:** Taskbar moves from being compiled into `display` to its own
process with its own window.
**Risk:** Low.

`taskbar` is now just another display client:
1. Sends `DISPLAY_CLAIM` for a 32px strip at the bottom.
2. Gets a shared buffer back.
3. Renders the bar into its buffer, sends `DISPLAY_FLIP`.
4. Listens for `DISPLAY_KEY` / `DISPLAY_MOUSE` to handle clicks.

The launch button sends an MPI message to `init` to spawn a new process.

`display` no longer has any hardcoded taskbar code.

---

### Phase 8 — `bin/terminal/` Windowed Terminal
**Visible result:** A terminal window you can open from the taskbar.
**Risk:** Medium (PTY wiring).

`terminal`:
1. Claims a window from `display`.
2. Renders an 80×25 (or larger) VT100 terminal into its buffer.
3. Forks a shell subprocess; wires its stdin/stdout to the terminal's input
   handling loop.
4. On each character received from the shell, updates the buffer and sends
   `DISPLAY_FLIP`.

Font: reuse the `libfb` 8×16 bitmap font.

This is the first app that proves the whole stack works end to end.

---

### Phase 9 — Compositor Upgrade in `display`
**Visible result:** Proper window layering, overlap, shadow, alpha.
**Risk:** Low (display server internal change only — protocol unchanged).

`display` adds:
- Z-order list of windows
- Damage tracking (only re-composite dirty regions)
- Alpha blending (simple src-over for now)
- Window decorations (title bar, close button drawn by `display`)

No client changes needed. The MPI protocol is identical to Phase 5.

---

### Phase 10 — Graphics Stack Refactor (C++ + libfb merge)
**Visible result:** None — internal refactor only. Behaviour identical.
**Risk:** Low (pure userspace, build-time only).

Consolidate the graphics stack to match the macOS two-layer model:
- **views** becomes a full C++ compositor (`Framebuffer` + `Window` + `WindowManager` classes). libfb dissolves into views as a private `Framebuffer` class.
- **objGFX** becomes the stable public app-side rendering API (equivalent to Core Graphics).
- **taskbar** ported to C++; renders via `ogSurface` instead of libfb directly.
- **libfb** removed as a public library.

Target layer diagram:
```
┌─────────────────────────────────────┐
│  Apps (term, taskbar, future apps)  │
│  render with objGFX (ogSurface)     │
├─────────────────────────────────────┤
│  views (WindowManager)              │
│  ┌─────────────┐  ┌───────────────┐ │
│  │   Window    │  │  Framebuffer  │ │
│  │  (per win)  │  │ (owns screen) │ │
│  └─────────────┘  └───────────────┘ │
├─────────────────────────────────────┤
│  Kernel: VESA + sys_mapfb           │
└─────────────────────────────────────┘
```

Sub-steps:

**10a — Port views to C++** ✓
- [x] `bin/views/views.c` → `views.cc`
- [x] Wrap C headers in `extern "C" {}`
- [x] `Framebuffer` class (wraps libfb internally for now)
- [x] `Window` class (replaces `win_t` struct + free functions)
- [x] `WindowManager` class (replaces globals + event dispatch)
- [x] Update `bin/views/Makefile` for `.cc`
- [x] Build and test — identical behaviour

**10b — Absorb libfb into views** ✓
- [x] Move libfb pixel primitives into `Framebuffer` class directly
- [x] Move `font8x8` data into views as a private static array
- [x] Remove libfb from views Makefile link line
- [x] Build and test

**10c — Port taskbar to C++** ✓
- [x] `bin/taskbar/taskbar.c` → `taskbar.cc`
- [x] Wrap C headers in `extern "C" {}`
- [x] Replace `fb_set_target` + libfb calls with `ogSurface` rendering
- [x] Remove libfb from taskbar Makefile link line
- [x] Build and test

**10d — Remove libfb as public library** ✓
- [x] Confirm no app outside views links libfb
- [x] Remove `lib/libfb/` from world build
- [x] Update CLAUDE.md architecture section

**10e — Clean up objGFX** ✓
- [x] Remove launcher, objgfx40, and sunlight (all dead code)
- [x] Merge headers from lib/objgfx/objgfx/ into include/objgfx/
- [x] Update all app Makefiles to -I../../include
- [x] Drop lib/objgfx internal include path; single canonical header location

---

### Phase 11 — Future: GPU Surface Backing
**Visible result:** Hardware-accelerated compositing.

When a GPU driver exists:
- Replace the `malloc`-backed shared buffer with a GPU surface.
- `Framebuffer` uses the GPU blitter instead of CPU blit.
- Protocol unchanged: clients still just call `DISPLAY_FLIP`.

This is the path to macOS IOSurface / Metal compositing.

---

## Phase Summary

| Phase | Key Deliverable | Visible? | Risk |
|-------|----------------|---------|------|
| 1 | Fix SDE build | No | None |
| 2 | `sys_mapfb()` syscall | Pixels from userspace | Low |
| 3 | `lib/libfb/` | Drawing primitives | None |
| 4 | `bin/display` + hardcoded taskbar | **Taskbar on screen** | Low |
| 5 | MPI display protocol | No | None |
| 6 | `vmm_share_region()` | Apps can draw windows | Medium |
| 7 | `bin/taskbar` as separate process | Cleaner architecture | Low |
| 8 | `bin/terminal` | Windowed terminal | Medium |
| 9 | Compositor upgrade | Layering, alpha, drag, close | Low |
| 10 | C++ refactor + libfb merge | Clean two-layer stack | Low |
| 11 | GPU surface backing | HW acceleration | Future |

**Taskbar is on screen after Phase 4.**
**Full window system is working after Phase 8.**
**macOS upgrade path stays open through Phase 11.**

---

## Key Design Rules (Do Not Break)

1. **MPI carries signals, not drawing commands.** If you find yourself putting
   pixel coordinates in an MPI message, stop — that's the BeOS trap.
2. **`views` is the only process that calls `sys_mapfb()`.** Everyone else
   gets a shared region.
3. **`vmm_share_region` is the only shared memory primitive.** No ad-hoc
   physical address passing between processes.
4. **The MPI protocol is versioned from day one.** First field in every
   message is `uint32_t msg_type`. Easy to extend without breaking old clients.
5. **Apps render with objGFX. views owns the framebuffer.** No app ever calls
   libfb or writes to the physical framebuffer directly.

---

## Status

| Phase | Name | Status |
|-------|------|--------|
| 1 | Fix SDE build | Done |
| 2 | `sys_mapfb()` syscall | Done |
| 3 | `lib/libfb/` | Done |
| 4 | `bin/display` + taskbar | Done |
| 5 | MPI display protocol | Done |
| 6 | `vmm_share_region()` | Done |
| 7 | `bin/taskbar` separate process | Done |
| 8 | `bin/terminal` | Done |
| 9 | Compositor upgrade (SSD, z-order, drag, close) | Done |
| 10a | views → C++ (Framebuffer/Window/WindowManager) | Done |
| 10b | Absorb libfb into views | Done |
| 10c | taskbar → C++ + ogSurface | Done |
| 10d | Remove libfb as public library | Done |
| 10e | Clean up objGFX | Done |
| 11 | GPU surface backing | Future |
