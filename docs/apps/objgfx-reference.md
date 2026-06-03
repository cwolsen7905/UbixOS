# objGFX Reference

`libobjgfx.so` is the userland drawing library for UbixOS apps.  It
provides surfaces (the buffer you draw into), text (via TrueType), and
image loading.  This document is the reference; the tutorial is
[`writing-a-views-app.md`](writing-a-views-app.md).

Headers are under `include/objgfx/`; include with
`<objgfx/objgfx.h>` etc.  Add `-I.../include` to your CFLAGS — never
`-I.../lib/objgfx`.  The library is linked dynamically (no static
archive is shipped).

---

## Coordinate system, colors, units

- Origin `(0,0)` is **top-left**.  X grows right, Y grows down.
- All coordinates are **inclusive of both endpoints** for shape calls
  (e.g. `ogFillRect(0, 0, 9, 9, c)` fills a 10×10 block, not 9×9).
- The native pixel format used by `views` is 32 bpp, packed as
  `0x00RRGGBB` (alpha byte ignored by the compositor).  Use the
  `RGB(r,g,b)` macro idiom: `((r<<16)|(g<<8)|b)`.
- Coordinates outside the surface are clipped (not faulted) for the
  high-level shape calls; raw `ogSetPixel` does **not** clip — bounds-check
  yourself.

---

## `ogSurface`

The surface is the central abstraction: a 2-D buffer of pixels you can
draw into.  In a views app you call `ogAttach` to wrap the compositor's
shared buffer; for an off-screen scratch surface use `ogCreate`.

### Construction

```c++
ogSurface s;                                  // empty, must Attach/Create
s.ogAttach(buf, w, h, OG_PIXFMT_32BPP);       // wrap caller-owned buffer
s.ogCreate(w, h, OG_PIXFMT_32BPP);            // allocate a fresh buffer
```

Pixel format constants (all in `<objgfx/ogPixelFmt.h>`):
`OG_PIXFMT_8BPP`, `OG_PIXFMT_15BPP`, `OG_PIXFMT_16BPP`, `OG_PIXFMT_24BPP`,
`OG_PIXFMT_32BPP`.  For views apps, **always use `OG_PIXFMT_32BPP`** —
the compositor expects it.

### Drawing — solid fills

| Call | What it draws |
|------|---------------|
| `ogClear(c)` | Fill the entire surface with colour `c`. |
| `ogFillRect(x1, y1, x2, y2, c)` | Axis-aligned filled rectangle. |
| `ogFillCircle(cx, cy, r, c)` | Filled circle. |
| `ogFillTriangle(x1,y1, x2,y2, x3,y3, c)` | Filled triangle. |
| `ogFillPolygon(n, points, c)` | Convex filled polygon. |

### Drawing — outlines and curves

| Call | What it draws |
|------|---------------|
| `ogLine(x1,y1, x2,y2, c)` | Line.  Set anti-aliasing first if wanted. |
| `ogHLine(x1, x2, y, c)` / `ogVLine(x, y1, y2, c)` | Axis-aligned line — faster than `ogLine`. |
| `ogRect(x1,y1, x2,y2, c)` | Rectangle outline. |
| `ogCircle(cx, cy, r, c)` | Circle outline. |
| `ogArc(cx, cy, r, start, end, c)` | Arc (angles in implementation units). |
| `ogCurve` / `ogBSpline` / `ogCubicBezierCurve` | Parametric curves. |

### Anti-aliasing and blending

```c++
s.ogSetAntiAliasing(true);     // affects lines/curves
s.ogSetBlending(true);         // alpha-blend ogSetPixel writes
s.ogSetAlpha(128);             // global alpha for blended writes
```

Anti-aliasing only applies to the curve/line primitives — fills are
always solid.  Blending only kicks in if `ogSetBlending(true)` was set.

### Raw access (when you need it)

```c++
s.ogSetPixel(x, y, color);              // one pixel, no clipping
uint32_t c = s.ogGetPixel(x, y);
void *row = s.ogGetPtr(x, y);           // direct pointer into the buffer
```

For tight inner loops, use `ogGetPtr` once per row and write `uint32_t`
words directly — that's how `bin/muffin` does its wallpaper blit.

### Blits (surface-to-surface)

```c++
ogSurface scratch;
scratch.ogCreate(64, 64, OG_PIXFMT_32BPP);
/* ...draw into scratch... */

dest.ogCopyBuf(destX, destY, scratch, 0, 0, 63, 63);              // 1:1 copy
dest.ogScaleBuf(dx,dy, dx+127,dy+127, scratch, 0,0, 63,63);       // stretch
```

`ogCopy` does a full-surface copy; `ogClone` makes a duplicate surface.

### Useful inspectors

| Call | Returns |
|------|---------|
| `ogGetMaxX() / ogGetMaxY()` | Width/height in pixels minus 1. |
| `ogGetBPP() / ogGetBytesPerPix()` | Bits / bytes per pixel. |
| `ogPack(r,g,b)` / `ogPack(r,g,b,a)` | Encode RGB into the surface's native format. |
| `ogUnpack(c, r,g,b)` | Decode a packed pixel. |
| `ogGetLastError()` | Error code from the last failed operation. |

---

## `ogScalableFont`

TrueType-based text rendering.  Header: `<objgfx/ogScalableFont.h>`.
The library bundles a Latin-1 glyph cache (codepoints 32..255) per
size, so the first `PutString` at a new size rasterizes the whole
range up front; subsequent calls are cheap.

### Loading

```c++
ogScalableFont font;
if (!font.Load("/var/fonts/DejaVuSans.ttf", 28)) {
    /* fall back — missing font is non-fatal */
}
```

The size argument is **pixel height** (cap-to-baseline), not points.
Use `IsValid()` to check after `Load`; `Load` may fail silently if the
file is missing or not a `.ttf`.

Standard fonts on the disk image:

| Path | Use |
|------|-----|
| `/var/fonts/DejaVuSans.ttf` | UI text (proportional). |
| `/var/fonts/DejaVuSansMono.ttf` | Terminal, code, tabular data. |

### Drawing

```c++
font.SetFGColor(255, 255, 255);
font.SetBGColor(0, 0, 0, /*alpha=*/0);   // alpha=0 → blend onto whatever's there
font.PutString(surf, x, y, "hello");     // (x,y) is the TOP-LEFT of the text box
font.PutChar(surf, x, y, 'A');
font.CenterTextX(surf, y, "centered");
```

`(x,y)` is the **top-left corner of the text box**, not the baseline.
Internally the baseline is placed at `y + Ascent()`.  This matches the
older `ogBitFont` API so the two are drop-in interchangeable for layout.

If `SetBGColor` was called with alpha > 0, glyph cells are filled by
lerping from `BG` to `FG` — useful for the cleanest possible rendering
on a known solid background.  If alpha is 0 the glyph is alpha-blended
over whatever was already on the surface.

### Metrics

| Call | Returns |
|------|---------|
| `Ascent()` / `Descent()` | Pixels above / below the baseline. |
| `LineHeight()` | Recommended vertical advance between lines. |
| `PixelHeight()` | What you passed to `Load`. |
| `GetWidth()` | Advance of `'M'` — exact for monospace, estimate for proportional. |
| `GetHeight()` | `Ascent + Descent`. |
| `TextWidth(s)` | Total advance of string `s` — use this for centring. |
| `TextHeight(s)` | Cap/line height. |
| `Advance(cp)` | Horizontal advance for one codepoint. |

---

## `ogImage`

Load and save image files into/out of a surface.  Header:
`<objgfx/ogImage.h>`.

```c++
ogSurface bg;
ogImage img;
if (img.Load("/var/background/ubix.png", bg)) {
    surf.ogCopyBuf(0, 0, bg, 0, 0, bg.ogGetMaxX(), bg.ogGetMaxY());
}
```

`Load` autodetects format by extension/magic:

| Format | Loader | Notes |
|--------|--------|-------|
| BMP    | `DecodeBMP` | 24-bit Win3.x style. |
| PNG    | `DecodePNG` | Via vendored `stb_image`; built with `STBI_NO_SIMD` (the kernel forbids SSE in userland too). |

`Save` writes BMPs via `EncodeBMP`; pass `ogImageType` to select format
and optional `ogImageOptions` for tuning.

The surface argument is sized to the image — it's the destination, not
a pre-sized canvas you blit into.

---

## Threading and lifecycle

- An `ogSurface` is **not thread-safe.**  One thread per surface.
  views apps are usually single-threaded against their compositor
  buffer; this is fine.
- The shared-memory buffer behind a views surface is owned by the
  compositor.  After `DISPLAY_WINRESIZE`, the old `shm_base` is invalid —
  call `ogAttach` again with the new pointer before touching the surface.
- `ogCreate`'d surfaces own their buffers and free them in their
  destructor.  `ogAttach`'d surfaces do not — the caller (you, or the
  compositor) owns the underlying memory.

---

## Tips

- **Pre-render to scratch surfaces** when an element doesn't change
  every frame (a widget's static label, the wallpaper).  `ogCopyBuf`
  from the scratch surface is far cheaper than re-rasterising.
- **Always check `ogScalableFont::IsValid()`** before drawing text —
  missing fonts on a fresh image are a common cause of "my app starts
  but nothing appears".
- **The compositor expects 32 bpp.**  Don't change pixel formats on a
  shared buffer; if you need a different format internally, use a
  scratch surface and blit at FLIP time.
- **`ogSetPixel` is unclipped.**  In hot loops it's faster than the
  higher-level primitives, but a bad coordinate corrupts memory
  (probably the next window's buffer).  Clip on your end.
