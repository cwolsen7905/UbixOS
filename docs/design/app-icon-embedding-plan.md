# App Icon System — Plan

How a uBixOS application declares its icon, so the desktop (taskbar Start grid,
window tabs, title bars, Files) can show a real per-app icon instead of the
taskbar hand-drawing a glyph for every known app.

## Status

- **Shipped (interim): sidecar PNG at `/usr/share/icons/`.** The taskbar Start
  grid loads `/usr/share/icons/<exec-basename>.png` per app via objGFX `ogImage`,
  cached once per menu-open, nearest-neighbour scaled into the tile. Fallback
  chain: **PNG → hand-drawn glyph (`draw_app_icon`) → monogram letter.** Icons
  are staged by `tools/mkimage.sh` from `share/icons/*.png` (dir may be empty;
  glyphs cover apps without a PNG). This is enough for now.
- **Planned (this doc): embed the icon *inside the app binary* (ELF)** so the
  icon travels with the executable — the Windows model. Not yet built; the ideas
  below are to be refined when we return to it.

## Why embed in the binary at all

The sidecar approach works, but the icon is a separate file that can be lost,
forgotten on install, or drift from the binary. Embedding makes the app
*self-contained*: copy the binary, you have its icon. It also means an app
"owns" its icon without a registry entry or naming convention.

How the platforms do it, for reference:
- **Windows** — icon compiled into the `.exe` as a PE **resource** (`.rsrc`);
  the shell extracts it (`ExtractIconEx`). Self-contained.
- **macOS** — `Icon.icns` is a **sidecar** in the `.app` bundle (not in the
  Mach-O), referenced by `Info.plist`.
- **Linux/freedesktop** — a `.desktop` file names an icon resolved from an icon
  theme dir. Sidecar + naming convention (this is essentially our interim).

## Mechanism options (ELF embed)

Both are proven techniques; the kernel already binary-embeds whole programs via
`objcopy` (`_binary_*_start` symbols in `sys/arch/aarch64/kern/boot.c`).

1. **`objcopy --add-section`** (preferred). Post-link, stamp a named section:
   ```sh
   objcopy --add-section .ubixicon=app.icon \
           --set-section-flags .ubixicon=noload,readonly  <app> <app>
   ```
   The icon lives in a `.ubixicon` section inside the binary. Nothing else in
   the app references it; it's pure metadata a reader extracts.

2. **`.incbin` / `_binary_*` symbols.** Compile a stub (`.section .ubixicon` +
   `.incbin "app.icon"`) into the app, exposing start/end symbols the app itself
   can read (to hand its icon to the compositor at runtime — see consumption #2).

## Consumption models

1. **Extract from the binary file (Windows-shell style) — for the Start menu.**
   The taskbar already has each app's exec path (from ubistry). To draw a tile it
   `open()`s the binary, reads the ELF header (`e_shoff`/`e_shnum`/`e_shstrndx`),
   walks section headers to find `.ubixicon`, `pread`s just that section, and
   blits it. No running process, no app cooperation, `pread`s only the header +
   the icon section (not the whole binary). ~50-line ELF section reader.

2. **App self-declares at runtime — for window tabs / title bars.** A running app
   reads its own embedded icon (via the `_binary_*` symbols) and sends it to the
   compositor in `DISPLAY_CLAIM` (an icon path, or the raw bytes inline). The
   compositor/taskbar caches it and shows it on the window's tab + title bar. This
   is the `DISPLAY_SETICON` idea in `views-polish-plan.md`.

## Icon payload format (to decide)

- **Raw fixed-size BGRA (recommended first):** e.g. 48×48×4 = 9216 bytes. The
  reader blits verbatim — **no decoder needed**, dead simple and fast. App
  authors a PNG; a build step converts PNG→raw BGRA and `--add-section`s it.
- **PNG in the section:** smaller on disk, but the reader needs
  **PNG-decode-from-memory** — objGFX `ogImage::Load` currently takes a *path*,
  not a buffer, so this needs a small `ogImage` addition (decode from memory).
- **Multiple sizes** (like `.ico`/`.icns`): a tiny header + N raw mips (16/32/48).
  Overkill until we have HiDPI; note it and defer.

## Open questions / to refine

- **Format:** raw BGRA vs PNG-in-section vs multi-size. (Lean raw BGRA to start.)
- **Build ergonomics:** a shared make rule + a host PNG→BGRA converter, invoked
  by each GUI app's Makefile (author `app.png`, get `.ubixicon` embedded). Where
  does the converter live (`tools/`)? Reuse host libpng (already a NetSurf dep).
- **`ogImage` from memory:** worth adding regardless (lets PNG-in-section and
  future in-memory decode work); small change to `lib/objgfx/ogImage.cpp`.
- **`DISPLAY_CLAIM` icon field:** path vs inline bytes in `display_proto.h`;
  size cap if inline.
- **Precedence:** embedded `.ubixicon` vs sidecar `/usr/share/icons/*.png` vs
  glyph vs monogram — settle the fallback order (proposed: embedded → sidecar →
  glyph → monogram).
- **Caching:** the taskbar should cache extracted icons keyed by binary path +
  mtime so it doesn't re-parse every menu-open.
- **objGFX P6 vector icons:** a tiny vector-icon format (crisp at any DPI) is a
  longer-term alternative to raster embed; see `objgfx-polish-plan.md` P6.

## Companion plans

- `views-polish-plan.md` — window-tab / title-bar icons (consumption #2), the
  `DISPLAY_SETICON` protocol item.
- `objgfx-polish-plan.md` — P6 image pipeline / vector icons; `ogImage`
  from-memory decode.
