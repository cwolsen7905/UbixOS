# uBixOS File Manager ("Files") — Design Plan

> A graphical file manager for the uBixOS desktop — the single biggest "feels
> complete" gap for a desktop OS today (there is no GUI way to browse the disk,
> only the shell). **Explorer-leaning, best-of-all-worlds**: the spatial model and
> chrome of Windows 11 File Explorer (the user's stated preference), the *power*
> ideas from macOS Finder (column view, Quick Look, tags), the *restraint* of
> GNOME Files, and the *speed* of orthodox dual-pane managers (Total Commander /
> Dolphin) — all adapted to uBixOS's `views` + `objGFX` stack and VFS.
>
> **Identity fit.** uBixOS is *console-first, graphical-optional* — the shell and
> `ls`/`cp`/`mv`/`rm` are the foundation; this is a userland *convenience layer*
> over the same VFS, not a new foundation. It is a standard `views`-hosted app
> (one process, a shared-memory window buffer, MPI for window control), built the
> same way as `bin/activity` and `bin/diskutil`. Pure userland, **dual-arch by
> construction** — nothing here touches `sys/arch/`.
>
> **What the kernel already gives us (verified):** browse via `opendir`/`readdir`/
> `stat`; mutate via `mkdir`(`sys_mkdir`), `rmdir`, `unlink`, `rename`(POSIX 128);
> capacity via `statfs`/`fstatfs` (396/397); open-with via `execve` of a path the
> **ubistry** registry maps from a file's type — exactly how the start menu
> launches apps today (`ubistry_get_str(".../exec")` → `execve`). So no new
> syscalls are needed for the MVP.

## North Star

Open it and the disk is *right there* — a familiar Explorer window: a **navigation
sidebar** on the left (pinned places + the folder tree), an **address bar with
clickable breadcrumbs** across the top, a **details list** in the middle, and a
**status bar** below. Double-click a folder to dive in, a file to open it in the
right app. Back/forward/up like a browser. Right-click anything for the obvious
actions. It should feel *immediately* familiar to a Windows user, reward a Finder
user with column view + Quick Look, and let a power user flip on a second pane and
drive the whole thing from the keyboard — without any of that clutter being in the
way of a newcomer.

## Best-of-all-worlds — what we borrow, and from where

| Source | What we take | Why |
|--------|--------------|-----|
| **Windows 11 Explorer** (the base) | Sidebar (Quick Access + tree), breadcrumb **address bar**, back/forward/up toolbar, Details/List/Icons views, status bar, **tabs that restore on reopen**, right-click context menu, drag-and-drop (incl. *onto breadcrumb segments*) | The user's preferred model; "navigation is seamless from the moment you open it" |
| **macOS Finder** | **Column view** (Miller columns), **Quick Look** (spacebar preview, no app launch), **tags/labels**, sidebar favorites, bottom **path bar** | The genuinely-better ideas Explorer lacks; Quick Look + column view are why Finder users keep it |
| **GNOME Files** | Minimal default, progressive disclosure, **type-ahead find** (start typing to jump) | Keep it clean for newcomers; power features present but not cluttering |
| **Orthodox (Total Commander / Dolphin)** | Optional **dual-pane / split view**, **keyboard-first** operation (arrows, type-ahead, F5 copy / F6 move / F7 mkdir / F8 delete), built-in viewer | Fast bulk copy/move between two locations; control over aesthetics |

Design rule from the recon: Explorer's strength is *frictionless defaults*; Finder's
is *preview + column navigation*; the orthodox managers' is *keyboard speed*. We
make Explorer the **default face**, fold in Finder's preview/column ideas as
first-class options, and put the dual-pane/keyboard power behind a toggle so it
never clutters the newcomer path.

## Constraints (every phase honors them)

- **`views` + `objGFX` app, MPI control only** — `DISPLAY_CLAIM`/`ACK` → `ogAttach`
  the shm buffer → draw → `DISPLAY_FLIP`; the event loop is the proven
  `bin/activity`/`bin/diskutil` shape (block on `mpi_waitMessage`, no busy-poll).
- **VFS-backed, no new syscalls for the MVP** — `opendir`/`readdir`/`stat` to
  browse; `mkdir`/`rmdir`/`unlink`/`rename` to mutate; `statfs` for free space.
- **No system clipboard yet** — the app carries its *own* cut/copy buffer for
  intra-app paste (Phase 2). A real cross-app clipboard is a companion feature.
- **The pooled root has no symlinks** — no "create shortcut/symlink"; show targets
  if a mounted FS exposes them.
- **Permissions are not enforced yet** — Properties shows mode/owner **read-only**
  until the multi-user-security plan lands; `chmod`-in-GUI waits on that.
- **Open-with via ubistry** — a `/files/assoc/<ext>` → `/bin/<app>` map in the
  registry, read exactly like the start-menu `exec` keys. No hard-coded launch.
- **Looks good for free** — uses the now-AA, gamma-correct objGFX (rounded cards,
  smooth selection, crisp text), so it matches the rest of the 2026 desktop.

## Phases

Each phase ends shippable and is independently useful. Roughly Explorer-MVP →
navigation polish → operations → Finder/orthodox power → niceties.

### P0 — MVP single-pane browser *(the milestone)*
The familiar Explorer window, browse + open + the safe basics:
- Window chrome: **toolbar** (back / forward / up, New Folder, view toggle),
  **address bar** showing the path as breadcrumbs, **details list**, **status bar**.
- **Details view**: Name · Size · Type · Modified, from `stat`; click a header to
  sort (reuse the `bin/activity` column-sort pattern).
- **Folder vs file icons** (objGFX-drawn glyphs first; a small icon set later).
- Navigation: double-click a folder to enter, **up**, **back/forward** history,
  type-ahead to jump to a name.
- **Open a file** → look up `/files/assoc/<ext>` in ubistry → `execve` the app
  (falls back to a "no handler" toast/dialog).
- Safe mutations: **New Folder**, **Rename** (inline), **Delete** (with confirm).
- Launchable from Start → Utilities and from a desktop icon.

### P1 — Navigation polish *(Explorer essentials)*
- **Editable address bar** — click it to type a path; breadcrumb **segment
  dropdowns** to hop to sibling folders (the Explorer touch).
- **Left sidebar** — *Quick Access* (pinned places, persisted in ubistry) above a
  live **folder tree**; drag a folder to pin it.
- **View modes** — List, Details, **Large Icons** (grid).
- **Search/filter** — a box that filters the current directory; recursive search
  is a later refinement.
- **Status bar** — item count, selected count + total size, and **free space**
  (`statfs`).

### P2 — File operations + an (internal) clipboard *(best-of)*
- **Cut / Copy / Paste** (Ctrl-X/C/V) via an in-app buffer; integrate the system
  clipboard once it exists.
- **Multi-select** — Shift/Ctrl-click + marquee (rubber-band) select.
- **Drag-and-drop** — move/copy within the window and **onto breadcrumb segments**
  (Win11's nicest recent add) and sidebar pins.
- **Copy progress + errors** — surfaced via the notifications companion (long
  copies show progress; failures show a dialog, not a silent stop).
- **Properties dialog** — name, type, size, mod time, mode/owner (read-only).

### P3 — Power features *(Finder + orthodox)*
- **Column view** (Miller columns) — Finder's deep-navigation win.
- **Quick Look** — **spacebar** previews the selection without launching an app:
  images via `ogImage` (PNG/BMP already decode), text/source via the font, dir
  summary otherwise. Cheap here, and a standout feature.
- **Tabs** — browser-style, **restored on reopen** (Win11 "smart tabs"), persisted
  in ubistry.
- **Dual-pane / split view** — a toggle for a second pane; **F5 copy / F6 move /
  F7 mkdir / F8 delete** between panes; fully keyboard-drivable. Off by default so
  the newcomer view stays clean.

### P4 — Niceties
- **Trash** — deletes stage to `~/.trash` with restore, instead of hard `unlink`.
- **Tags / color labels** (Finder) — stored in ubistry or an xattr sidecar.
- **Themeing** — light/dark + accent from the ubistry tokens (ties to objGFX P7).
- **Remote/mounted browsing** — surfaces VFS mountpoints (network FS is far future).

## Companion features this unlocks / needs

These are small, independently-useful efforts the file manager leans on — several
double as the other "feels complete" desktop gaps:
- **Default-app registry** (`/files/assoc/<ext>` in ubistry) + an **image viewer**
  and **text viewer** app — the open-with targets, *and* Quick Look's renderers.
- **System clipboard** — a `views`-owned clipboard so cut/copy/paste works across
  apps (the file manager's P2 internal buffer is the bridge until then).
- **Notifications / toasts** — copy progress, "no handler for .xyz", errors.
- **Trash spec** — shared with the shell (`rm` could honor it later).

## Non-goals
- Not an orthodox-only manager — single-pane Explorer is the default; dual-pane is
  an opt-in mode, not the identity.
- No network/cloud/FTP client in scope (VFS mountpoints only, much later).
- Not a terminal replacement (a "open terminal here" action is fine; an embedded
  shell pane is not in scope).
- No file *editing* — the manager *launches* editors/viewers; it doesn't embed them.

## Sequencing & effort
P0 (the MVP window) is roughly the size of `bin/activity` — a focused `views`+objGFX
app over `readdir`/`stat`, ~1–2 sessions. P1–P2 are incremental polish on top. P3
(column view, Quick Look, dual-pane, tabs) is where it becomes genuinely
best-of-breed and is the larger lift. Build P0 first, ship it in Start → Utilities,
then layer up — same cadence as the Activity Monitor and objGFX plans.

## References (recon)
- Windows 11 File Explorer — tabs/smart-tabs, breadcrumb address bar, drag-and-drop
  into breadcrumbs, streamlined context menu.
- Finder vs Explorer comparisons — Quick Look, tags, column view as Finder's
  borrow-worthy strengths; Explorer's "seamless from open" navigation.
- Orthodox / dual-pane (Total Commander, Double Commander, Dolphin, OneCommander) —
  dual-pane, keyboard-first F-keys, built-in viewer.
