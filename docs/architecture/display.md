# UbixOS Display System Architecture

## Design principle

**Shared memory buffers; MPI as signaling only.**

Apps render into a framebuffer region the kernel shares into their address space.
The `views` compositor blits those regions onto the physical screen.
MPI messages never carry pixels — only signals ("I claimed a region", "I'm done
drawing", "give me keyboard events").  This keeps the upgrade path to
GPU-accelerated compositing open without changing the protocol.

---

## Component map

```
  ┌──────────────────────────────────────────────────────────┐
  │  Kernel                                                  │
  │  sys/kernel/fb.c                                         │
  │    sys_mapfb      – maps VESA framebuffer into views     │
  │    sys_shareregion– shares a vmm region into a client    │
  │    sys_getmouse   – drains mouse ring buffer (syscall 44)│
  │    sys_getkbd     – drains kbd ring buffer  (syscall 46) │
  │  sys/isa/atkbd.c  – ISR fills kbd_ring[]                 │
  │  sys/isa/mouse.c  – ISR fills mouse ring                 │
  └───────────────────────────┬──────────────────────────────┘
                              │ syscalls / shared memory
  ┌───────────────────────────▼──────────────────────────────┐
  │  bin/views  (compositor — C++)                           │
  │                                                          │
  │  • Owns the physical framebuffer (via sys_mapfb)         │
  │  • Maintains Window list: id, x/y/w/h, shm, mbox         │
  │  • Polls mouse and kbd via inline syscall stubs          │
  │  • Dispatches DISPLAY_MOUSE / DISPLAY_KEY to focused win │
  │  • On DISPLAY_CLAIM: vmm_share_region → client gets shm  │
  │  • On DISPLAY_FLIP: composites client buffer onto screen │
  │  • On DISPLAY_RELEASE: repaints desktop, reblit others   │
  │  • Server-side decorations: draws title bar + close btn  │
  │  • focus-follows-click: left click sets focused window   │
  └──────┬────────────────────────────────┬──────────────────┘
         │ MPI                            │ MPI
  ┌──────▼──────────┐           ┌─────────▼────────────────┐
  │  bin/taskbar    │           │  bin/term  (and others)  │
  │  (C++)          │           │  (C++)                   │
  │                 │           │                          │
  │  DISPLAY_CLAIM  │           │  DISPLAY_CLAIM           │
  │  DISPLAY_FLIP   │           │  DISPLAY_FLIP            │
  │  DISPLAY_RELEASE│           │  DISPLAY_RELEASE         │
  │  DISPLAY_KEY    │           │  DISPLAY_KEY             │
  │  DISPLAY_MOUSE  │           │  DISPLAY_MOUSE           │
  │                 │           │                          │
  │  objgfx:        │           │  objgfx:                 │
  │    ogSurface    │           │    ogSurface             │
  │ ogScalableFont  │           │ ogScalableFont           │
  └─────────────────┘           └──────────────────────────┘
```

---

## MPI protocol  (`include/views/display_proto.h`)

| Message | Direction | Payload struct | Purpose |
|---------|-----------|----------------|---------|
| `DISPLAY_QUERY` | client → views | `display_query` | Ask for screen geometry |
| `DISPLAY_INFO` | views → client | `display_info` | Screen w/h/bpp reply |
| `DISPLAY_CLAIM` | client → views | `display_claim_req` | Request a window region |
| `DISPLAY_ACK` | views → client | `display_ack` | Region granted; carries `shm_base` |
| `DISPLAY_DENIED` | views → client | — | Region refused |
| `DISPLAY_FLIP` | client → views | `display_flip` | Pixel buffer updated |
| `DISPLAY_RELEASE` | client → views | `display_release` | Window closing |
| `DISPLAY_KEY` | views → client | `display_key` | Key event for focused window |
| `DISPLAY_MOUSE` | views → client | `display_mouse_ev` | Mouse event |

All structs fit within `MESSAGE_LENGTH` (248 bytes).  MPI never carries pixels.

### Claiming a window (typical client sequence)

```c
// 1. Create your own MPI mailbox
mpi_createMbox("myapp");

// 2. Send DISPLAY_CLAIM
mpi_message_t msg;
struct display_claim_req *creq = (struct display_claim_req *)msg.data;
msg.header       = DISPLAY_CLAIM;
creq->x          = 100;  creq->y = 100;
creq->w          = 320;  creq->h = 240;
creq->sender_pid = getpid();
strncpy(creq->title, "My Window", sizeof(creq->title) - 1);
strncpy(creq->reply,  "myapp",    sizeof(creq->reply)  - 1);
mpi_postMessage("views", DISPLAY_CLAIM, &msg);

// 3. Wait for ACK
mpi_message_t reply;
while (mpi_fetchMessage("myapp", &reply) != 0)
    sched_yield();

struct display_ack *da = (struct display_ack *)reply.data;
uint32_t win_id   = da->window_id;
void    *shm_base = da->shm_base;   // draw here
```

Set `creq->no_decor = 1` to suppress the server-side title bar (used by
taskbar and flyout panels).

### Rendering and flipping

```cpp
// Attach an ogSurface to the shared buffer and draw
ogSurface surf;
surf.ogAttach(shm_base, w, h, OG_PIXFMT_32BPP);
surf.ogFillRect(0, 0, w-1, h-1, 0x001A1A2E);

// Signal the compositor
struct display_flip *fl = (struct display_flip *)msg.data;
msg.header    = DISPLAY_FLIP;
fl->window_id = win_id;
fl->dirty_x = fl->dirty_y = fl->dirty_w = fl->dirty_h = 0;  // 0 = full
mpi_postMessage("views", DISPLAY_FLIP, &msg);
```

---

## Shared memory path

`vmm_share_region` (syscall 45) is the mechanism that puts the window buffer
into the client's address space:

1. `views` allocates `w * h * 4` bytes with `malloc` (in its own heap).
2. On `DISPLAY_CLAIM`, views calls `vmm_share_region(vaddr, size, client_pid)`.
3. The kernel walks the page table, remaps those physical pages into the client's
   address space at a new virtual address, and returns that address.
4. views sends the client address back in `display_ack.shm_base`.
5. Both processes now read/write the same physical pages — no copy on flip.

---

## Drawing library: objgfx  (`lib/objgfx/`, headers in `include/objgfx/`)

C++, statically linked. Used by all GUI apps (taskbar, term, muffin, etc.).

Attach to a shared memory window buffer:

```cpp
ogSurface surf;
surf.ogAttach(shm_base, w, h, OG_PIXFMT_32BPP);

surf.ogFillRect(0, 0, w-1, h-1, 0x00101010);  // filled rect (inclusive)
surf.ogRect(10, 10, 100, 50, 0x00FFFFFF);      // outline rect
surf.ogLine(0, 0, w-1, h-1, 0x00FF0000);       // line
surf.ogSetPixel(x, y, color);                  // single pixel
```

Load and render text. The current UI uses **scalable, antialiased TrueType
fonts** via `ogScalableFont` (stb_truetype); `.ttf` files live at `/var/fonts/`
(`DejaVuSans.ttf`, `DejaVuSansMono.ttf`). The compositor, taskbar, terminal,
settings, login and tessera all use this:

```cpp
ogScalableFont font;
font.Load("/var/fonts/DejaVuSans.ttf", 16);  // path, pixel height
font.SetFGColor(192, 192, 192, 255);          // R, G, B, A
font.SetBGColor(0, 0, 0, 255);
font.PutString(surf, x, y, "Hello");          // baseline at y + Ascent()
font.PutChar(surf, x, y, 'A');
int w = font.TextWidth("Hello");              // pixel width (exact)
```

> The older fixed-size bitmap font (`ogBitFont`, `.DPF` files) still exists for
> legacy callers and uses the same `PutChar`/`PutString`/`SetFGColor` API, but
> new code should use `ogScalableFont`.

Color helpers when packing `uint32_t` colors from separate R/G/B components:

```cpp
static void font_fg(ogScalableFont &f, uint32_t c) {
    f.SetFGColor((c>>16)&0xFF, (c>>8)&0xFF, c&0xFF, 255);
}
```

---

## Server-side decorations

`views` draws the title bar and close button on behalf of the app. Apps receive
a shared buffer sized to their *content* area (not including the title bar).
`views` composites the decoration above the content buffer when blitting to the
screen.

- Title bar height: `DECOR_H` (18px), defined in `display_proto.h`
- Close button: top-right corner of the title bar; click sends `DISPLAY_RELEASE`
  back to views which tears down the window
- `no_decor = 1` in `display_claim_req` skips decorations (used by taskbar)

---

## Input event flow

```
AT keyboard ISR (atkbd.c)
  └─ kbd_ring_push(keycode, pressed)   ← 64-entry ring buffer
        └─ sys_getkbd (syscall 46)
              └─ inline poll_kbd() in views
                    └─ views event loop
                          └─ DISPLAY_KEY → focused window's MPI mailbox

PS/2 mouse ISR (mouse.c)
  └─ mouse ring buffer
        └─ sys_getmouse (syscall 44)
              └─ inline poll_mouse() in views
                    └─ views event loop
                          └─ cursor move + DISPLAY_MOUSE → focused window
```

Focus is set by left-click on a window (`focused_win` in WindowManager).

### Key constants  (`include/sys/kbd.h`)

| Constant | Value | Key |
|----------|-------|-----|
| `KEY_UP` | 0x100 | ↑ |
| `KEY_DOWN` | 0x101 | ↓ |
| `KEY_LEFT` | 0x102 | ← |
| `KEY_RIGHT` | 0x103 | → |
| `KEY_F1`–`KEY_F10` | 0x104–0x10D | Function keys |
| `KEY_HOME` | 0x10E | Home |
| `KEY_END` | 0x10F | End |
| `KEY_PGUP` | 0x110 | Page Up |
| `KEY_PGDN` | 0x111 | Page Down |
| `KEY_INS` | 0x112 | Insert |
| `KEY_DEL` | 0x115 | Delete |
| `KEY_ESC` | 0x1B | Escape |
| `< 0x80` | ASCII | Printable / control chars |

---

## Native syscall slots (UbixOS ABI — `int $0x81`)

| Slot | Name | Purpose |
|------|------|---------|
| 43 | `sys_mapfb` | Map VESA framebuffer into caller's address space |
| 44 | `sys_getmouse` | Drain one event from mouse ring |
| 45 | `sys_shareregion` | Share a vmm region into another process |
| 46 | `sys_getkbd` | Drain one event from keyboard ring |

---

## Adding a new GUI application

1. Create `bin/myapp/` with a `Makefile` — copy `bin/taskbar/Makefile` as a template.
2. Add `myapp` to `SUBDIRS` in `bin/Makefile`.
3. In your source: create the MPI mailbox, send `DISPLAY_QUERY` to get screen
   dimensions, send `DISPLAY_CLAIM`, draw into `shm_base` with `ogSurface`,
   send `DISPLAY_FLIP` to show it.
4. Handle `DISPLAY_KEY` and `DISPLAY_MOUSE` in your event loop.
5. Send `DISPLAY_RELEASE` on exit.

```sh
bmake world && bmake image && bmake run
```
