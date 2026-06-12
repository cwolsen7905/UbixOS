# Tech Spec: VESA Framebuffer Console

**Status: SUPERSEDED on i386 / DEFERRED — never implemented.**

> This spec was never built (`sys/isa/fbcon.c` / `font8x16.h` do not exist). On
> i386 it has been superseded: the kernel console stayed VGA text mode + COM1
> serial (which works fine for diagnostics), and the VESA framebuffer is owned
> by the userland `views` compositor via `sys_mapfb` — a kernel framebuffer
> console would now *conflict* with that ownership.
>
> **Why it is kept, not deleted:** the idea returns for the **arm64 port**.
> AArch64 (QEMU `virt`, Raspberry Pi) has *no* VGA text mode (0xB8000 is
> x86-PC-only), so the kernel's on-screen console there must be either the
> PL011 UART (serial — the primary early console) or a framebuffer console
> exactly like this one. When arm64 reaches the framebuffer milestone
> (`cross-arch-plan.md` Phase 15), revisit this design — generalised over a
> generic linear-framebuffer descriptor rather than the VESA/multiboot
> specifics below. Until then this is a reference, not active work.

## Status Matrix

Legend: ✅ done & verified · ⬜ not started · ⛔ superseded (won't build as specced)

| Target | Status | Notes |
|--------|--------|-------|
| i386 / VESA LFB (this spec) | ⛔ | never built; `views` owns the VESA LFB via `sys_mapfb`, so the kernel console stays VGA text + COM1 — a kernel fb console would conflict |
| aarch64 / virtio-gpu (output) | ✅ | shipped as a `kconsole` sink — `sys/arch/aarch64/dev/fbcon.c`, convergence-plan **Phase 3.5** (`6db8e3dc1`); 8×8 glyphs, suspended when `views` claims the screen. Boot log / panic / base-profile login text on screen |
| aarch64 — on-screen keyboard input (screen-only, no serial) | ⬜ | fbcon is an output sink today; an interactive base console on a serial-less SBC still needs a keyboard input path (convergence-plan Phase 4 follow-up) |

The on-screen kernel console the product identity wants was delivered on
aarch64 (where there is no VGA text mode); this i386/VESA spec below is kept as
the reference design that work generalised from.

---

**Original goal (i386/VESA):** Replace the VGA text-mode console with a single
800×600 VESA linear framebuffer console, rendering all kernel and userspace
output as pixel-drawn text.  Individual graphics card drivers can be added
later; this spec uses the VESA Linear Framebuffer (LFB) provided by
GRUB/multiboot as the universal foundation.

---

## 1. Current State

| Concern | Current implementation |
|---|---|
| Screen output | VGA text mode (80×25), written directly via `0xB8000` |
| kprintf path | Writes to VGA text buffer + COM1 serial |
| Userspace write() | `sys_write` → TTY layer → VGA text buffer |
| Boot handoff | GRUB multiboot, VESA mode set via VM86 BIOS call in `vesa_init()` |
| Graphics subsystem | Userland compositor (`bin/views`) owns VESA framebuffer via `sys_mapfb`; kernel console still uses VGA text mode |

---

## 2. Goals

1. Request an 800×600×32bpp VESA LFB from GRUB via the multiboot header.
2. Implement a kernel framebuffer console driver (`sys/isa/fbcon.c`) that
   renders 8×16 bitmap glyphs into the LFB.
3. Replace VGA text-mode writes in `kprintf` with framebuffer console writes.
4. Redirect the TTY userspace write path to the same framebuffer console.
5. Keep COM1 serial output active at all times as a diagnostic fallback.
6. Keep VGA text mode active until `fbcon_init()` succeeds, so early boot
   messages are never lost.

**Non-goals (this spec):**
- Hardware-specific drivers (Intel, AMD, virtio-gpu).
- Multiple virtual consoles or window management.
- Accelerated scrolling / hardware blitting.
- Mouse cursor.
- Colour themes or font selection at runtime.

---

## 3. Multiboot Header Changes

**File:** `sys/init/start.S`

Add the multiboot video mode flags to the existing multiboot header so GRUB
calls the VBE mode-set before handing control to the kernel.

```
MULTIBOOT_VIDEO_MODE  equ  (1 << 2)   ; add to existing flags

; Multiboot header (already present — extend it):
multiboot_header:
    dd  MULTIBOOT_MAGIC
    dd  MULTIBOOT_FLAGS | MULTIBOOT_VIDEO_MODE
    dd  -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS | MULTIBOOT_VIDEO_MODE)
    dd  0, 0, 0, 0, 0         ; load address fields (unused with ELF)
    dd  0                     ; graphics mode: 0 = linear, 1 = text
    dd  800                   ; preferred width
    dd  600                   ; preferred height
    dd  32                    ; preferred depth (bits per pixel)
```

GRUB treats width/height/depth as hints. The kernel must **not** assume
800×600×32 was honoured; it must read back the actual values from the
multiboot info struct (see §4).

---

## 4. Multiboot Info Parsing

**File:** `sys/init/main.c` (kmain) and/or a new `sys/init/multiboot.c`

The multiboot info struct fields relevant to the framebuffer:

| Field | Type | Description |
|---|---|---|
| `flags` | uint32 | Bit 12 set → framebuffer fields valid |
| `framebuffer_addr` | uint64 | Physical base address of the LFB |
| `framebuffer_pitch` | uint32 | Bytes per scanline (may be > width × bpp/8) |
| `framebuffer_width` | uint32 | Actual width in pixels |
| `framebuffer_height` | uint32 | Actual height in pixels |
| `framebuffer_bpp` | uint8 | Bits per pixel (expect 32 or 24) |
| `framebuffer_type` | uint8 | 1 = RGB, 2 = EGA text — must be 1 |

These values are saved into a global `struct fbcon_info` early in `kmain`,
before any other subsystem initialises.  If bit 12 of `flags` is not set, or
`framebuffer_type` is not 1, the kernel falls back to VGA text mode and logs
a warning on serial.

---

## 5. New Files

```
sys/isa/fbcon.c        — framebuffer console driver (init, putchar, scroll)
sys/isa/fbcon.h        — public API
sys/isa/font8x16.h     — embedded 8×16 bitmap font (256 glyphs × 16 bytes)
```

No new Makefile entries are needed beyond adding `fbcon.o` to
`sys/isa/Makefile`'s OBJS.

---

## 6. Font

Embed the standard 8×16 PC VGA bitmap font as a `static const uint8_t
font8x16[256][16]` array in `sys/isa/font8x16.h`.  This font is in the public
domain and is 4 096 bytes — negligible kernel size cost.

Each glyph is 16 bytes, one byte per row, MSB = leftmost pixel.

---

## 7. fbcon API

```c
/* sys/isa/fbcon.h */

struct fbcon_info {
    uint32_t *base;    /* mapped LFB base (physical == virtual pre-paging) */
    uint32_t  pitch;   /* bytes per scanline                                */
    uint32_t  width;   /* pixels                                            */
    uint32_t  height;  /* pixels                                            */
    uint8_t   bpp;     /* bits per pixel                                    */
};

/* Call once from kmain after multiboot info is parsed. */
int  fbcon_init(struct fbcon_info *info);

/* Drop-in for the VGA text putchar used by kprintf. */
void fbcon_putchar(char c);

/* Clear screen. */
void fbcon_clear(void);

/* Called by the TTY layer for userspace write(). */
void fbcon_write(const char *buf, size_t len);
```

---

## 8. fbcon Implementation Notes

### 8.1 Internal state

```c
static struct fbcon_info  fb;
static int  cursor_col  = 0;   /* character column, 0-based */
static int  cursor_row  = 0;   /* character row, 0-based    */
static int  cols;              /* fb.width  / 8             */
static int  rows;              /* fb.height / 16            */
static uint32_t fg = 0xFFFFFF; /* foreground colour (white) */
static uint32_t bg = 0x000000; /* background colour (black) */
```

### 8.2 Glyph rendering

```c
static void draw_glyph(int col, int row, char c) {
    const uint8_t *glyph = font8x16[(uint8_t)c];
    uint32_t *dst = fb.base + row * 16 * (fb.pitch / 4) + col * 8;
    for (int y = 0; y < 16; y++) {
        uint8_t bits = glyph[y];
        for (int x = 0; x < 8; x++)
            dst[x] = (bits & (0x80 >> x)) ? fg : bg;
        dst += fb.pitch / 4;
    }
}
```

### 8.3 putchar logic

```
\n  → cursor_col = 0; cursor_row++; if (cursor_row >= rows) scroll();
\r  → cursor_col = 0
\b  → if (cursor_col > 0) cursor_col--
else → draw_glyph(cursor_col, cursor_row, c); cursor_col++;
       if (cursor_col >= cols) { cursor_col = 0; cursor_row++; }
       if (cursor_row >= rows) scroll();
```

### 8.4 Scrolling

Move the framebuffer contents up by one text row (16 pixel rows):

```c
static void scroll(void) {
    size_t row_bytes = 16 * fb.pitch;
    size_t total     = rows * row_bytes;
    memmove(fb.base, (uint8_t *)fb.base + row_bytes, total - row_bytes);
    /* Clear bottom row */
    uint32_t *last = (uint32_t *)((uint8_t *)fb.base + total - row_bytes);
    for (size_t i = 0; i < row_bytes / 4; i++) last[i] = bg;
    cursor_row = rows - 1;
}
```

At 800×600×32 the `memmove` moves ~1.84 MB per scroll event.  This is
acceptable for a text console.  Hardware-accelerated scrolling (e.g. adjusting
a display start offset register) is a future optimisation and out of scope here.

---

## 9. kprintf Integration

**File:** `sys/lib/kprintf.c`

`kprintf` currently calls a `putchar`-equivalent that writes to the VGA text
buffer and to serial.  Change it to:

```c
void kputchar(char c) {
    serial_putchar(c);   /* COM1 — always on */
    if (fbcon_ready)
        fbcon_putchar(c);
    else
        vga_putchar(c);  /* fallback until fbcon_init() succeeds */
}
```

`fbcon_ready` is a `static int` in `fbcon.c`, set to 1 by `fbcon_init()`.
VGA text mode is retained as a fallback so that early boot panics (before
`fbcon_init()` runs) still appear on screen.

---

## 10. TTY / Userspace Integration

**File:** `sys/kernel/tty.c` (or wherever the TTY write path lands)

The TTY layer currently writes characters to the VGA text buffer.  Replace
those writes with calls to `fbcon_putchar()` / `fbcon_write()`.

The keyboard input path is unaffected — it feeds characters into the TTY
read buffer regardless of the output driver.

---

## 11. Memory Mapping

The LFB physical address comes from the multiboot info struct.  In the early
kernel, before the VMM is fully initialised, physical == virtual (identity
mapping covers the low 4 MB and the framebuffer is usually above that).

**Action required:** ensure the VMM maps the LFB region as write-combining or
uncached.  The LFB for QEMU's VBE adapter typically appears in the 0xE0000000
range (PCI BAR).  The exact address must be identity-mapped (or mapped to a
fixed virtual address) before `fbcon_init()` is called.

This is the most platform-specific step.  For QEMU it will just work because
the region is already accessible.  On real hardware the address could be
anywhere in PCI space and may need a dedicated VMM mapping call.

---

## 12. Implementation Order

1. **Multiboot header** — add video mode request to `start.S`, verify GRUB
   passes a valid framebuffer info struct (check on serial).
2. **Info parsing** — save framebuffer fields into a global in `kmain`,
   print them on serial to confirm.
3. **Font** — embed `font8x16.h`.
4. **fbcon.c skeleton** — `fbcon_init`, `fbcon_putchar`, `fbcon_clear`;
   test by calling `fbcon_putchar` directly from `kmain` before the scheduler
   starts.
5. **kprintf wired** — swap VGA calls in `kprintf` for `fbcon_putchar`; keep
   VGA fallback.  Full boot output should now appear on the framebuffer.
6. **TTY wired** — redirect TTY write path; userspace `printf` output appears
   on framebuffer.
7. **Cleanup** — remove VGA text-mode write paths (keep VGA init for fallback
   detection only).

---

## 13. Risks and Mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| GRUB ignores the video mode request | Low | Read back actual mode from multiboot info; fall back to VGA text if framebuffer_type ≠ 1 |
| LFB address not accessible before VMM init | Medium | Call `fbcon_init` after the identity-map is established; verify address on serial first |
| 24bpp vs 32bpp pixel packing difference | Low | Check `bpp` field; implement both `write_pixel_32` and `write_pixel_24` |
| Scroll performance noticeable | Medium | Acceptable for now; document as a known TODO |
| Early panic before fbcon_init | Low | VGA text fallback retained until step 7 |
| Real hardware LFB address outside identity map | Medium | Out of scope for QEMU target; noted as follow-up work |

---

## 14. Future Work (out of scope for this spec)

- Hardware-specific drivers (Intel GMA, AMD, virtio-gpu) to replace or
  supplement the VBE LFB.
- Multiple virtual consoles (one per TTY).
- Hardware scroll offset register (avoid `memmove` on scroll).
- Coloured output in `kprintf` (e.g. red for panics).
- Runtime font selection.
