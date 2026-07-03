# uBixOS Vellum — Native GUI Text Editor Plan

> **Vellum** is uBixOS's **native graphical text editor**: a windowed `objGFX`
> app on the `views` desktop, the analogue of **TextEdit** (macOS) or
> **Notepad++** (Windows) — a friendly, mouse-and-keyboard editor for the
> graphical profile, distinct from the console editors `vi` and `ed` that ship
> in `bin/`/`usr.bin/`.
>
> **Why "Vellum".** The OS names its apps with one evocative word (`tessera`,
> `cubitaire`, `aural`, `ubistry`) rather than utilitarian abbreviations. A
> *vellum* is a fine writing surface (prepared parchment); the whole uBixOS
> display stack is built on `objGFX` **surfaces**, and an editor is literally a
> surface you write on. Binary: `/usr/bin/vellum`. Window title: "Vellum".
>
> **Product-identity fit (read `console-and-arch-convergence-plan.md`).** uBixOS
> is *console-first, graphical-optional*. Vellum is a **userland layer app**, not
> a base-system dependency: the console keeps `vi`/`ed`; Vellum is the desktop
> citizen you reach for under the graphical profile. Nothing in the kernel or
> base system depends on it.
>
> **Hard constraints (inherited from the platform):**
> - **objGFX only.** Vellum draws into its shared-memory window buffer with
>   `objGFX` (`ogSurface` + `ogScalableFont`) and lets `views` composite. It
>   never touches the framebuffer directly (only `views` calls `sys_mapfb`).
> - **Software, scalar, no float in hot paths where avoidable.** Same world as
>   every other app; the monospace glyph cache is `ogScalableFont`'s Latin-1
>   cache.
> - **Both 64-bit arches green.** Build under `TARGET=aarch64` and
>   `TARGET=x86_64`; no arch-specific code (it's a pure userland app).

## Status

- **Not started.** This document is the plan. The old `usr.bin/edit/` (a 2018
  console `cat`-a-file stub, never wired into any build) is **superseded** and
  will be removed when Vellum lands.
- Companion docs: `views-polish-plan.md` (compositor + display protocol),
  `objgfx-polish-plan.md` (rendering primitives), `file-manager-plan.md`
  (`files` — the app that will "Open with Vellum"), `app-icon-embedding-plan.md`
  (the icon at `/usr/share/icons/vellum.png`).

## Goals

1. Open, edit, and save UTF-8-ish (Latin-1 today, matching `ogScalableFont`)
   plain-text files through the VFS.
2. A real editing experience: multi-line buffer, blinking caret, cursor keys,
   Home/End/PgUp/PgDn, word-wrap-off horizontal scroll, line numbers, a status
   bar (path, dirty flag, `Ln`/`Col`).
3. Mouse: click-to-position, wheel scroll, drag to select.
4. Selection + clipboard (cut/copy/paste), undo/redo.
5. Feel native to the desktop: resizable decorated window, monospace font,
   Start-menu entry + icon, and "Open with Vellum" from the `files` manager.

## Non-goals (at least initially)

- No syntax highlighting in M1 (added in a later milestone).
- No project/tree view, no tabs-for-multiple-files in M1 (single buffer first).
- No LSP, no plugins, no terminal integration, no VCS — this is TextEdit, not
  an IDE. (VS Code is explicitly out of scope for uBixOS: it is Electron =
  Chromium + Node/V8, which the OS has no path to host.)
- No RTL/complex-script shaping (bounded by `ogScalableFont`'s Latin-1 cache).

## Architecture

Vellum is a single userland C++ app in **`usr.bin/vellum/`** (`main.cc` +
`Makefile`), modelled directly on `usr.bin/term/term.cc` — the closest existing
pattern (a text app that claims a resizable decorated window, runs an
`ubix::Mailbox` event loop, and renders a monospace grid with `ogScalableFont`).

```
usr.bin/vellum/
  Makefile        # include ../../Makefile.incl, ../Makefile.incl;
                  # BINARY=vellum; OBJS=main.o;
                  # .include "${UBIX_MK}/ubix.musl.cxx.ubix.prog.mk"
  main.cc         # everything (Editor class + main() event loop)
```
Register in `usr.bin/Makefile` `SUBDIRS`. Stage the icon via
`share/icons/vellum.png` (see `mkimage.sh` icon staging) and a Start-menu seed
in `tools/makereg.c`.

### Window & display protocol

- Claim a **decorated, resizable** window from `views` (`DISPLAY_CLAIM`,
  `no_decor=0`), default ~720×520, `min` ~360×240, `max` bounded to the screen.
- `wants_motion=1` so drag-select and hover work.
- Handle: `DISPLAY_KEY` (editing), `DISPLAY_MOUSE` (click/drag/wheel),
  `DISPLAY_CLOSE` (prompt-if-dirty then release), `DISPLAY_WINRESIZE`
  (re-attach the new buffer, recompute visible rows/cols, full redraw).
- `DISPLAY_SETTITLE` to reflect the filename + a leading `*` when modified
  (e.g. `*notes.txt — Vellum`).
- Flip only the damaged rect where practical (see Rendering).

### Text model

- **M1: `std::vector<std::string> lines_`** — one string per line, newline
  implied between entries. Dead simple, correct, good enough for the file sizes
  a hobby OS edits. Cursor is `(cy_ line, cx_ column-in-chars)`.
- **Later:** if large-file editing or heavy edit churn shows `vector<string>`
  cost, migrate to a **gap buffer** or **piece table** behind the same cursor
  API. Not needed for M1; note and defer.
- Line endings: read LF and CRLF, normalise to LF in the buffer, remember the
  original on load, write it back on save (default LF for new files).

### Layout & rendering

```
+------------------------------------------------+  <- server title bar (views)
| 12 | int main(void) {                          |
| 13 |     printf("hi\n");                        |   gutter | text area
| 14 |     return 0;|                             |         (caret after 0)
| .. |                                            |
+------------------------------------------------+
|  notes.txt          UTF-8   LF   Ln 14, Col 14  |  <- status bar (Vellum-drawn)
+------------------------------------------------+
```

- **Font:** `ogScalableFont` loaded from `/var/fonts/DejaVuSansMono.ttf` at ~16px.
  Cell metrics: `fh_ = LineHeight()`, `fw_ = Advance('M')` (monospace ⇒ uniform);
  verify with `TextWidth`.
- **Gutter:** line numbers, right-aligned, muted; width sized to the digit count.
- **Text area:** visible rows = `(sh_ - status_h) / fh_`; `top_` = first visible
  line, `left_` = first visible column (horizontal scroll; no wrap in M1).
- **Caret:** a 2px vertical bar at the cursor cell; **blinks** via the same
  mailbox-timeout trick `vlogin` uses (`mbox.wait(reply, N_ticks)` toggles the
  caret and repaints just the caret cell) so an idle editor produces no CPU spin.
- **Status bar:** flat strip at the bottom — path, encoding, line-ending, and
  `Ln`/`Col`; dirty shown as a leading `*`.
- **Repaint strategy:** **M1 = full redraw + full-window flip on every change**
  (an editor is not an animation; a 720×520 repaint is cheap and trivially
  correct). **M2+:** damage-diff like `term` (track a dirty rect: caret move =
  two cells; single-char insert = rest-of-line; line insert/delete = below the
  cursor to the bottom) and flip only that rect.
- Modern touches borrowed from the desktop work: current-line subtle highlight,
  optional 1px accent on the caret line's gutter, selection drawn as an
  accent-tinted fill (see `objgfx-polish-plan` for gamma-correct blends).

### Input model

Compositor keys arrive as `DISPLAY_KEY {keycode, pressed}`. Control chars are
pre-folded to `keycode < 0x100` (`Ctrl-S = 0x13`), specials are `KEY_*`
(`<sys/kbd.h>`). Proposed bindings (macOS/Notepad++-ish, all interceptable):

| Key | Action |
|-----|--------|
| Printable (`0x20`–`0xFF`) | Insert char at cursor |
| `Enter` (`\r`/`\n`) | Split line at cursor |
| `Backspace` (`0x08`) | Delete left / join with previous line |
| `KEY_DEL` | Delete right / join with next line |
| `Tab` (`\t`) | Insert spaces to the next tab stop (default 4; configurable) |
| `KEY_LEFT/RIGHT/UP/DOWN` | Move cursor (Shift+ extends selection — M2) |
| `KEY_HOME/END` | Start / end of line |
| `KEY_PGUP/PGDN` | Scroll one page, move cursor with it |
| `Ctrl-S` (`0x13`) | Save |
| `Ctrl-O` (`0x0F`) | Open (path prompt — M2) |
| `Ctrl-N` (`0x0E`) | New buffer |
| `Ctrl-Q` (`0x11`) | Quit (prompt if dirty) |
| `Ctrl-Z`/`Ctrl-Y` (`0x1A`/`0x19`) | Undo / redo (M2) |
| `Ctrl-X/C/V` (`0x18`/`0x03`/`0x16`) | Cut / copy / paste (M2) |
| `Ctrl-F` (`0x06`) | Find (M3) |

> Note `Ctrl-C` is `0x03` — inside Vellum it means **copy**, not SIGINT (Vellum
> is a GUI app, not a tty job). Keep the mapping local to the editor.

**Mouse** (`DISPLAY_MOUSE`, window-relative coords): click → position caret at
the nearest cell (clamped to line length); drag → extend selection (M2); wheel
→ scroll `wheel` lines. A click in the gutter selects the whole line (M2).

### File I/O

- Load: `argv[1]` (absolute or cwd-relative) via `fopen`/`fread` through the
  VFS; missing file ⇒ empty buffer titled "untitled". Read fully into `lines_`.
- Save: write `lines_` joined by the file's newline to a temp path, then rename
  over the target (atomic-ish) — or direct `fopen("w")` for M1 simplicity.
- Dirty tracking: any mutation sets `modified_`; save clears it; title/status
  reflect it; close/quit prompts when dirty (a simple in-window confirm bar).

## Milestones

- **M1 — MVP (usable editor).** Window claim + resize, `vector<string>` buffer,
  load/save (`argv[1]`, `Ctrl-S`), full-redraw render with gutter + status bar +
  blinking caret, all cursor/edit keys in the table above, click-to-position,
  wheel scroll. Registered in the build, Start menu, icon. **Ships first.**
- **M2 — Selection, clipboard, undo.** Shift+arrows / drag selection, an
  accent-tinted selection fill, cut/copy/paste (shared clipboard via `ubistry`
  or a small MPI clipboard service — decide in the open questions), undo/redo
  ring, damage-diff rendering.
- **M3 — Find/replace + go-to-line.** Incremental find bar, replace, `Ctrl-G`.
- **M4 — Syntax highlighting.** A minimal, table-driven tokenizer (C/C++, sh,
  Makefile, Markdown) colouring keywords/strings/comments; per-line cached
  token runs. Pluggable so more languages are data, not code.
- **M5 — Multiple buffers / tabs + "Open with".** Tab strip for several open
  files; `files` manager "Open with Vellum" association; open-recent.
- **Later:** soft-wrap toggle, auto-indent, bracket match, config (tab width,
  font size) via `ubistry`, large-file model (gap buffer/piece table).

## Desktop integration

- **Start menu:** seed an entry in `tools/makereg.c` (category "Utilities" or
  similar) pointing at `/usr/bin/vellum`.
- **Icon:** `share/icons/vellum.png` (a sheet-of-paper / pen-nib glyph), staged
  to `/usr/share/icons/vellum.png` by `mkimage.sh`; the taskbar Start grid and
  window buttons pick it up automatically (see `app-icon-embedding-plan.md`;
  fallback is the hand-drawn glyph — add a "vellum"/"edit" case to
  `draw_app_glyph` in `taskbar.cc`).
- **Files manager:** add "Open with Vellum" (spawn `/usr/bin/vellum <path>`);
  eventually a default association for `text/*`.
- **Taskbar:** decorated window ⇒ it already gets a tab, hover preview, and
  Alt-Tab entry for free.

## Open questions / to decide

- **Clipboard architecture.** No system clipboard exists yet. Options: (a) a
  tiny MPI "clipboard" service (like a mini `ubistry` key), (b) a well-known
  `ubistry` path holding the clipboard text, (c) app-local only for M1. Leaning
  (b) for M2 so cut/copy/paste works across Vellum *and* `term`/`files` later.
- **Open/Save-As UI.** No file-chooser widget exists. M1: path via `argv` +
  `Ctrl-S` to the same path. M2: a simple in-window path prompt, or reuse the
  `files` manager as a chooser via a new MPI request.
- **Config surface.** Tab width, font size, line-number toggle → `ubistry`
  (`vellum/*`) with a Settings pane later, mirroring how `views` reads
  `views/*`.
- **Encoding.** Latin-1 today (font cache limit). Note UTF-8 decode + a wider
  glyph cache as a cross-cutting objGFX item, not a Vellum-only fix.
- **Big files.** `vector<string>` is fine to ~MBs; revisit the model only if a
  real workload hurts.
- **Name lock-in.** `vellum` unless you prefer `scribe` / `quill` / `vedit`
  (GUI-convention `v`-prefix). Trivial to rename before M1 lands.
