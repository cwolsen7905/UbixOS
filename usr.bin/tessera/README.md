# Tessera

A Tetris clone for the UbixOS views compositor. The name comes from the Greek/Latin root for "four" + "tile" — the same etymology as the original game.

## Controls

| Key       | Action              |
|-----------|---------------------|
| ← →       | Move piece left/right |
| ↑         | Rotate clockwise    |
| ↓         | Soft drop (1 row)   |
| Space     | Hard drop           |
| P         | Pause / unpause     |
| 1 / 2 / 3 | Switch music track  |
| Q         | Quit                |

Closing the window via the title-bar close button also exits cleanly.

## Scoring

Follows the Nintendo scoring system:

| Lines cleared | Points          |
|---------------|-----------------|
| 1 (Single)    | 40 × level      |
| 2 (Double)    | 100 × level     |
| 3 (Triple)    | 300 × level     |
| 4 (Tetris)    | 1200 × level    |

Soft drop scores 1 point per row; hard drop scores 2 points per row fallen.

Every 10 lines cleared advances the level by 1. Gravity speed increases with each level (800 ms per drop at level 1, decreasing by 70 ms per level, floor of 100 ms).

## Pieces

The standard 7 tetrominoes (I, O, T, S, Z, J, L) in their classic colors. A ghost piece shows where the active piece will land.

## Audio

Requires `/dev/audio` (AC'97 driver). If the device is unavailable the game runs silently.

- **Music:** Three chiptune tracks, selectable at any time with `1` / `2` / `3`:
  - **1** — Korobeiniki (Tetris Theme A)
  - **2** — Minuet in G (Bach/Petzold)
  - **3** — Ode to Joy (Beethoven 9th theme)
- **SFX:** Short tones on move, rotate, lock, and line clear.

All audio is square-wave synthesis at 48 kHz stereo. Switching tracks takes effect immediately and loops from the start of the new piece.

## Building

```sh
bmake -C bin/tessera
```

Or as part of the full world build:

```sh
bmake world
```

The binary is installed to `/usr/bin/tessera` on the disk image.

## Running

Launch from the UbixOS shell after the views compositor is running:

```
tessera
```

If the views compositor is not running, the game prints an error and exits.
