# Views / Taskbar UI Modernization — Plan & Status

Modernizing the uBixOS desktop (compositor + taskbar + start menu) from a
2000s-era look to a contemporary flat design. Branch: `wip/netsurf-fonts`,
June 2026.

## Status matrix

At-a-glance view of every tracked item. Detail for done items is in the **Done**
table; detail for open items is in the numbered **Outstanding** sections.

| Item | Status | Ref |
|------|--------|-----|
| Window chrome — flat title bars, hand-rasterised glyphs | ✅ Done | Done table |
| Window depth — drop shadows + rounded corners | ✅ Done | Done table |
| Roomier dimensions + calm slate accent | ✅ Done | Done table |
| Taskbar restyle, brand, clock, start menu | ✅ Done | Done table |
| Hover highlighting (start/tabs/menus) | ✅ Done | Done table |
| Volume system tray | ✅ Done | Done table |
| Logout→relogin reboot fix | ✅ Done | Done table |
| Shared-region refcount (removes the ~3 MB/logout leak) | ✅ Done | Done table |
| Modern login screen (vlogin) | ✅ Done | Done table |
| objGFX reusable primitives (rounded-rect/shadow/blend) | ✅ Done | Done table |
| Active-window highlight (`DISPLAY_FOCUS` sender) | ✅ Done | Done table |
| **Network tray indicator** | ⬜ To do | §1 |
| **Login affordances** (caret blink, clock, power, field hover) | ⬜ To do | §2 |
| **Hover-to-open submenus** | ⬜ To do | §3 |
| **Volume tray scroll-to-adjust / click-to-mute** | ⬜ To do | §4 |
| **Taskbar translucency** | ⬜ To do | §5 |
| **Per-app icons on window tabs** | ⬜ To do | §6 |

Related (tracked elsewhere): the residual ~51-page/cycle **orphan-zombie reaping**
leak lives in `docs/design/session-plan.md` (post-rfork follow-up).

## Done (shipped & committed)

| Area | What | Commits |
|------|------|---------|
| Window chrome | Windows 11-flat title bars: flat accent fill + hairline, hand-rasterised min/max/close glyphs (no font-character buttons), left title, focus-dimmed glyphs | `e1bf6eb88` |
| Window depth | Soft drop shadows (quadratic falloff, no sqrt) + anti-aliased rounded corners, blended against the cached desktop; `flush()` damage inflated by the shadow reach so moves leave no trails | `dbd40eaac` |
| Dimensions | `DECOR_H` 18 → 28 (roomier bars); default focused accent → calm desaturated slate `0x333C4C` (was saturated `0x284870`) | `dbd40eaac` |
| Glyph polish | Brighter (`0xFF6E6E`) + larger close/min/max glyphs for the taller bar | `c38bbc20e` |
| Taskbar | Flat slate restyle (cohesive palette), flat window tabs w/ accent underline, boxless clock, flat pop-over start menu | `e64d58b83` |
| Taskbar | Hamburger start icon + "uBixOS" brand, date+time clock, start-menu footer (logged-in user + power/logout button) | `681dc37f6` |
| Hover | Hover highlighting on start button, window tabs, menu items, and sticky-parent (parent row stays lit while its submenu is open). Needed an additive `input_router` change: forward motion to the topmost `wants_motion` window under the cursor + a negative-coord "exit" event (tracked by id) | `823183605` |
| System tray | Volume speaker glyph (red+slashed when muted, level ticks) left of the clock, mirrored from ubistry `/aural/volume`+`/aural/mute`, click opens Settings | `823183605` |
| Taskbar | Icon-only window buttons: app glyph tile (shared scalable `draw_app_glyph`, keyed off the title) replaces the truncated name | current |
| Hover preview | Windows-11 live thumbnail: taskbar hover → `DISPLAY_PREVIEW` (id + anchor-x) → compositor floats a scaled window buffer above the button. MPI carries only the signal; the compositor owns the pixels + scaling. Redrawn under cursor motion; hidden on unhover/click | current |
| Stability | **Logout→relogin reboot fixed** — `vmm_share_region` physical-page use-after-free (see below) | `0e695e4d3` |
| Stability | objGFX glyph-blit hardened against corrupted cache entries (defensive) | `e44ee148c` |
| Diagnostics | Kernel segfault report names the VMA/backing file holding `eip` (offline `addr2line`) | `d5c645d7e` |
| VMM / leak | Shared-region refcount: `cowCounter` +1 per shared page; freed exactly once after **both** owner and recipient unmap — removes the ~3 MB/logout leak the reboot fix left behind | `596b66839` |
| Login | Modern rounded login card: calm slate palette, soft drop shadow, boxed `Username`/`Password` fields with accent-underline + caret on focus, centred "uBixOS"/"Sign in" header | `2b675e041` |
| objGFX | Reusable `ogFillRoundRect` / `ogRoundRect` / `ogDropShadow` / `ogBlendColor` primitives; the compositor + chrome's local `decor_blend` folded into `ogBlendColor` (single implementation, ABI-safe additions) | `80facc813`, `7940fab39` |
| Active window | Compositor sends `DISPLAY_FOCUS` so the taskbar highlights the active tab; all focus changes route through one `WindowManager::set_focus()`, deduped and sent after the claim ACK to dodge the handshake race | `54f9cbfb7` |

### The logout reboot bug (root cause, for reference)

`vmm_share_region` maps the owner's physical pages into a recipient and marked
only the **recipient** PTEs `PAGE_SHARED`. The owner's PTEs stayed
`cowCounter==0`, so when the owner (`views`) `free()`d vlogin's full-screen
(~3 MB) window buffer on logout, musl `munmap`'d it and the kernel **freed the
physical frames while vlogin still mapped them** → physical use-after-free → the
freed frames recycled into views' heap, corrupting the long-lived objGFX
stb_truetype glyph cache → wild read/write → compositor crash → triple fault /
reboot. Only logout triggered it because only vlogin's buffer is large enough
that musl `munmap`s on `free()` (small app buffers stay in the heap free-list
and never reach `free_page`). Fixed by marking the **source** pages
`PAGE_SHARED` too (`0e695e4d3`), then properly refcounted in `596b66839`.

## Outstanding

### 1. Network tray indicator
A network status glyph (link up/down, maybe the IP) in the system tray next to
the volume speaker. Blocked on a **data source the taskbar can read** — none
exists yet. Plan:
- Add a procfs entry (e.g. `/proc/net` or a small `net`/`ifstatus` file) or a
  tiny syscall exposing link state + the primary IPv4 address.
- Taskbar: poll it (like it mirrors `/aural/*` for volume), draw a glyph (filled
  when up, slashed when down), optional click → opens the Network settings pane.

### 2. Login affordances (deferred from the login modernization)
The login card shipped (`2b675e041`); these are the polish items left off it:
- **Blinking caret** — the focus caret is currently static; animate it (the
  vlogin loop would need a periodic redraw tick, today it redraws on input only).
- **Clock / date** on the login screen.
- **Power / restart control** on the login screen (mirrors the taskbar footer's
  logout/power button).
- **Field hover states** — the login window is already a `wants_motion` surface,
  so hover highlights on the fields/buttons can be added like the taskbar's.

### 3. Hover-to-open submenus
Start-menu submenus currently **open on click**; hover only highlights the row.
Make hovering a parent row open its submenu (with a small dwell delay so passing
the cursor over rows doesn't flicker submenus open/closed).

### 4. Volume tray: scroll-to-adjust / click-to-mute
The volume glyph currently **opens Settings** on click. Add direct control:
scroll-wheel over the glyph adjusts `/aural/volume`; click toggles `/aural/mute`.
Needs scroll-event delivery to the tray (mouse wheel → `wants_motion`/a new event
field) — check what the input path currently forwards.

### 5. Taskbar translucency
Once shadows/corners are proven, give the taskbar strip a translucent blend over
the desktop (compositor alpha blend of the strip against the cached background),
for a more modern frosted look.

### 6. Per-app icons on window tabs
Show a small per-application icon on each taskbar window tab (and possibly the
title bar). Needs an **icon protocol/asset path**: a way for an app to declare
its icon (e.g. a `DISPLAY_SETICON` message or an icon file path convention) plus
a small icon decoder/cache in the taskbar.
