# uBixOS look and feel: notes

## Files

- `tokens.css`: every colour, radius, spacing value, font size and shadow. Pages and `ui.css` only use these variables. The one hard-coded value is the `#07080d` browser backdrop outside the framebuffer.
- `ui.css`: shared components (`.taskbar`, `.titlebar-active`, `.winbtn`, `.field`, `.list-row`…). State modifiers `.is-hover`, `.is-pressed`, `.is-focused`, `.is-disabled`, `.is-selected` match the states in `states.html` one to one.
- Screens, all 1280×800 and absolutely positioned: `login.html`, `desktop.html`, `launcher-menu.html`, `terminal.html`, `files.html`, `window-chrome.html`, `states.html`. Open `index.html` to see them all.
- `assets/`: `wallpaper-1280x800.png` and `wallpaper-800x480.png`. Icons are in `icons/16`, `icons/24` and `icons/48` as plain SVG. They use stroke paths with baked-in colours, so you can rasterise them to PNG once at build time.

## Direction

- **Dark, quiet, crisp.** There's one blue-grey ground (`--c-ground`) and a single blurple accent (`--c-accent`). The accent is used for **lines and marks, never floods**: outlines on primary buttons, focus rings, the running-window mark, and the 1px hairline on the focused window. Contrast comes from neutral steps, not saturation. The only saturated area is the wallpaper.
- **Console-first.** The terminal has the darkest surface in the system (`--c-console`), tabs in its title bar, and a full 16-colour ANSI palette in the tokens. The login screen offers a **Desktop / Console** session choice. The tray has a console button (Ctrl+Alt+F1).
- **Identity cues that are uBixOS's own:**
  - A floating, rounded taskbar 8px off the screen edge.
  - A labelled `uBix` launcher pill with a ring-and-dot mark.
  - Window controls in a capsule (keep-on-top | minimize · maximize · close) with round buttons.
  - A short accent mark on top of running task items.
  - A one-line clock (`Fri 9 Oct  21:41`).
  - Left-aligned login.
- **Type.** DejaVu Sans Book 12px is the default, with Bold for emphasis only (the clock time, the launcher label, avatar initials). Hierarchy comes from size and colour. DejaVu Sans Mono 13/18 is used in the terminal.

## Renderer constraints honoured

- Fills, rounded rects, circles and lines only. Translucency is plain alpha (`--c-taskbar`, `--c-panel`, `--c-scrim-login`). There's no blur.
- Every element has at most one shadow, written as `x y blur colour` (`--shadow-*`). Outlines are 1px rectangle strokes.
- The only gradient in the UI is the horizontal hairline on active title bars and the login card: accent, fading to transparent over 48px at each end. The app icon discs use a vertical gradient, but those are pre-rendered assets.
- The wallpaper is a pre-rendered PNG, so its glow and grid cost nothing at runtime.
- No transforms, filters, masks or animation.

**One exception to flag:** the CSS uses `outline` with `outline-offset` for the focus ring. Implement it as a 2px rounded-rect stroke drawn 2px outside the control, with radius = control radius + 2.

## Key measurements

- Taskbar: x 8, y 752, 1264×40, radius 12, 1px edge, `--shadow-taskbar`.
  - Launcher pill: 32px high.
  - Task items: 180×32. The running mark is 16×2 at the top centre: grey when running, accent when focused.
- Title bar: 32px.
  - Controls capsule: 28px high.
  - Buttons: 22px circles with 16px glyphs.
  - Close hover uses `--c-danger-hover`. Every other button uses `--c-neutral-800` for hover and `--c-accent-800` for pressed.
- Maximized windows fill 0,0 → 1280×744, with radius 0 and no shadow.
- Controls: 32px high, radius 6.
- List rows: 28px.
- Scrollbar: 8px overlay.

## Smaller screens (800×480)

- Use `wallpaper-800x480.png`, which has the same composition and horizon at 60%.
- Taskbar:
  - Keep it at 40px high, but set the inset to 0 and the radius to 0, so it runs edge to edge at y 440. That gives back 8px per edge.
  - Task items drop their label below 120px of free width and become 40×32, icon only, still with the running mark.
  - The tray drops the date and the volume percentage, and keeps the time.
- Windows open maximized by default (0,0 → 800×440). The title bar stays 32px.
- Launcher:
  - Becomes 480×400.
  - The category rail collapses to a 48px icon column.
  - The app grid becomes 4 × 64px tiles with 32px icons.
  - The Continue list is hidden.
- Login: the card is centred horizontally at 340px wide and the clock drops to 32px. The Desktop/Console choice stays.
- Files: the sidebar collapses to 48px icons. Grid tiles are 88×84 with 32px icons.
- Terminal: drop the status bar. 13px DejaVu Mono still gives about 100×24 cells.
- All metrics are on a 4px grid, so you can derive a 0.75× scale by rounding to whole pixels if you ever need one.

## Interactions & animation (describe-only)

- **Launcher.** Opens on a launcher click or the Super key, and the search field is focused immediately. Typing filters apps. If the text doesn't match an app, Enter runs it as a shell command in a new Terminal tab. Esc or a click outside closes it.
  - Animation (optional): 120ms fade from 0→100% alpha, plus a slide up of 8px. Skip it on Pi-class hardware if it costs frames.
- **Task items.**
  - Click a task item to focus that window. Clicking the focused item minimizes it. Clicking a minimized item restores it.
  - Middle-click closes the window.
- **Windows.**
  - Drag the title bar to move. Double-click it to maximize or restore.
  - Dragging to the top edge maximizes. Dragging a maximized window un-maximizes it under the cursor.
  - Resize from the 6px edges or 14px corners.
  - Keep-on-top toggles `.is-on`.
  - Minimize/restore (optional): 150ms alpha fade, no scaling.
- **Focus.** Only one window is active at a time. Clicking the desktop deactivates all windows.
- **Tray.** Volume opens the popover in `desktop.html`, and the scroll wheel over the volume icon changes volume in 5% steps. The clock opens a month calendar using the panel style. The console button switches to tty1.
- **Login.** Enter submits. A wrong password shakes the field horizontally ±6px three times over 240ms. If that's too costly, flash the field outline in `--c-danger` instead. Caps Lock shows a warning line under the field.
- **Keyboard.** Every control in `states.html` has a focused state. Tab order follows reading order.

## Open items

- Your current "Do You Ubix?" login wallpaper and the existing screenshots didn't come through with the brief. The login screen uses the synthwave wallpaper under a 62% scrim for now. To swap it, replace the `<img>` in `login.html`.
- There's only one style direction so far. For alternatives, copy the folder and edit its `tokens.css`. Every screen reads from it.
