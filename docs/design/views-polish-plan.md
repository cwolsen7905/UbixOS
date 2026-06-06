# Views / Taskbar UI Modernization — Plan & Status

Modernizing the uBixOS desktop (compositor + taskbar + start menu) from a
2000s-era look to a contemporary flat design. Branch: `wip/netsurf-fonts`,
June 2026.

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
| Stability | **Logout→relogin reboot fixed** — `vmm_share_region` physical-page use-after-free (see below) | `0e695e4d3` |
| Stability | objGFX glyph-blit hardened against corrupted cache entries (defensive) | `e44ee148c` |
| Diagnostics | Kernel segfault report names the VMA/backing file holding `eip` (offline `addr2line`) | `d5c645d7e` |

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
`PAGE_SHARED` too, so the owner's free skips `free_page` for shared frames.

## Outstanding

### 1. Refcount shared regions (remove the logout leak) — **priority**
The fix above is a *bounded leak*: shared frames are now freed only when the
**owning** process exits, so each logout cycle leaks vlogin's ~3 MB buffer
(~80 logouts → OOM on a 256 MB VM). Proper fix:
- `vmm_share_region`: `adjust_cow_counter(phys[i], +1)` per shared page (instead
  of, or in addition to, the `PAGE_SHARED` source mark).
- Both teardown sites (`vmm_unmap_page.c`, `vmm_paging.c` clean-virtual-space):
  for a `PAGE_SHARED` page **not** in the file-page cache (i.e. a share_region
  page), call `free_page` (which decrements `cowCounter` and frees at 0) instead
  of the current no-op. Must distinguish file-cache `PAGE_SHARED` pages (managed
  by `vm_filecache` refcount) from share_region ones — use the
  `vm_filecache_unref_phys` "was-in-cache" result.
- Net: page freed exactly once after **both** owner and recipient unmap, in
  either order. Verify with the logout cycle + `/proc/meminfo` free-page count
  staying flat across many cycles.

### 2. Active-window highlight — ✅ DONE
Shipped: the compositor now sends `DISPLAY_FOCUS` (the taskbar receiver already
existed), so the active window's taskbar tab is highlighted.
- All focus changes route through `WindowManager::set_focus()` — claim, release,
  reap, close, minimize, raise, and `input_router`'s click-to-focus (via a new
  focus callback, the path the first attempt missed).
- Posts the focused window id (0 for no-decor/none), deduped so redundant updates
  don't spam the taskbar mailbox.
- Avoids the earlier handshake race: in `handle_claim` the focus is sent **after**
  the client ACK + tab NOTIFY, and the dedupe makes the taskbar's own no-decor
  claim a no-op, so `DISPLAY_FOCUS` never lands mid-claim.

### 3. Network tray indicator
Volume tray is done; a network status glyph (link up/down, maybe IP) needs a
data source the taskbar can read. None exists yet — add a procfs entry
(e.g. `/proc/net` or a `net` status file) or a small syscall, then a glyph in
the tray next to the volume speaker.

### 4. Modernize the login screen (vlogin) — ✅ DONE
Shipped: the login UI was rebuilt to match the modern chrome.
- **Palette** — calm slate (slate card `#272E3A`, border `#394456`, modern-blue
  accent `#5B8DEF`); the old saturated navy is gone.
- **Centered card** — rounded-corner card with a soft drop shadow, drawn with the
  new shared objGFX primitives (`ogFillRoundRect`/`ogRoundRect`/`ogDropShadow`),
  sat 45px below screen centre.
- **Real input fields** — boxed `Username`/`Password` fields; the focused field
  lights up with a 2px accent underline + caret (replaces the block-cursor row).
- **Branding** — centred "uBixOS" wordmark + "Sign in" subtitle and divider.
- **Reusable primitives** — the rounded-rect/shadow/blend helpers were promoted
  into objGFX (and the compositor's `decor_blend` folded into `ogBlendColor`), so
  this look is now available to every app, not bespoke to vlogin.

Deferred (nice-to-have, not blocking): blinking-caret animation, a clock/date and
power/restart control on the login screen, and field hover states (the window is
already a `wants_motion` surface, so hover can be added later).

### 5. Nice-to-haves (unscheduled)
- Hover-to-open submenus (currently click-to-open; hover only highlights).
- Volume tray: scroll-to-adjust / click-to-mute instead of opening Settings.
- Taskbar translucency (compositor blend) once shadows/corners are proven.
- Per-app icons on window tabs (needs an icon protocol/asset path).
