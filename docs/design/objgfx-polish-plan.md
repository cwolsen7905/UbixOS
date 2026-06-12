# uBixOS objGFX Modernization Plan

> `objGFX` (`lib/objgfx/`, headers `include/objgfx/`, reference
> `docs/apps/objgfx-reference.md`) is uBixOS's **app-side rendering library**:
> the immediate-mode 2-D drawing API every userland app uses to paint into its
> shared-memory window buffer, which `views` then composites to the screen. It
> is the uBixOS analogue of **Core Graphics (Quartz 2D)** on macOS and
> **Direct2D / Win2D** on Windows — the layer *below* the widgets, *above* the
> framebuffer.
>
> Today objGFX has the **minimum viable primitive set**: surfaces, packed-pixel
> get/set, solid fills, outlines, parametric curves, integer blits, a single
> alpha value, TrueType text (Latin-1) via stb_truetype, BMP/PNG load. It draws
> a clean, *flat, aliased, sRGB-naive* picture — roughly a 2005-era 2-D stack.
> The seeds of something better are already in the tree: `ogEdgeTable` (a
> scanline polygon edge table with gouraud interpolation), `ogDropShadow`,
> `ogFillRoundRect`, `ogBlendColor`, and `ogPixCon` (format conversion). This
> plan is the **ongoing, multi-phase** effort to grow those seeds into a
> rendering library that produces output people expect in **2026**, borrowing
> the best-proven ideas from **Windows 11 (Fluent / WinUI / Mica & Acrylic)**
> and **macOS (Quartz, San Francisco text, vibrancy)** — proportionate to a
> console-first hobby OS.
>
> **Layer boundary (read first).** objGFX provides *primitives*; it is not the
> compositor and not a widget toolkit. Backdrop blur, vibrancy, window
> elevation and animation *policy* live in `views` (see
> `views-polish-plan.md`); objGFX's job is to give `views` and apps the fast,
> correct building blocks (premultiplied compositing, gradients, separable blur,
> rounded-rect clipping, gamma-correct text) those effects are built from. Every
> phase below states what objGFX exposes vs. what the consumer assembles.
>
> **Hard constraints (non-negotiable, every phase honors them):**
> - **Software-only.** No GPU. QEMU `virt` has no 2-D accelerator we use; the
>   Pi/Orange Pi GPU is a far-future maybe, not a dependency. Everything is
>   CPU spans.
> - **Scalar is the floor; SIMD is an *optional accelerated backend*, not the
>   default.** Every primitive has a scalar, cache-friendly span implementation
>   that always works on any CPU and is the correctness oracle. SIMD (SSE2 on
>   i386, NEON on aarch64) is selected at runtime *when available* — see
>   **"Backend & capability model"** below. The current `-mno-sse` /
>   `-mgeneral-regs-only` build is a consequence of the kernel not yet saving
>   the vector register file across context switches (i386 never sets
>   `CR4.OSFXSR`, so XMM faults `#UD`; aarch64 enables EL0 FP via `CPACR_EL1`
>   but `context.S` saves only `x19–x30`, not `v0–v31`/`FPSR`). Lifting that is
>   a one-time **kernel** change per arch, and is the real enable switch for the
>   SIMD backend.
> - **Core links `-lc` only.** `lib/objgfx` uses **no STL / no libstdc++**
>   (the font core says so explicitly). New code uses flat arrays and manual
>   memory, not `std::vector`/`std::map`.
> - **Pure userland, dual-arch by construction.** Nothing here touches
>   `sys/arch/`. Both i386 and aarch64 stay green throughout.
> - **Immediate mode stays.** No retained scene graph, no display list. Apps
>   draw every frame; `views` owns damage/dirty-rect.

## North Star

A 2026-grade 2-D library is defined by five things objGFX lacks, in order of how
much a person *notices* them:

1. **Real alpha compositing.** The alpha byte is honored, not ignored.
   Source-over (Porter-Duff) blending on **premultiplied** 32bpp, done in
   **linear light** (gamma-correct), so translucency, soft edges and shadows
   look right instead of muddy.
2. **Everything is anti-aliased.** Rounded rects, circles, polygons, arcs —
   not just lines. Coverage-based, via one shared scanline pipeline.
3. **A unified vector path + transform model.** `moveTo/lineTo/curveTo/close`,
   fill rules, stroking with caps/joins/width, an affine CTM, and a clip stack
   (including rounded-rect clip — the single most-used shape in both OSes).
4. **Gradients and materials.** Multi-stop linear/radial gradients; a separable
   blur primitive that `views` turns into Acrylic/Mica/vibrancy.
5. **Crisp, scalable text and layout.** Gamma-correct AA, kerning, subpixel
   positioning, UTF-8 beyond Latin-1, font fallback, and a points→pixels
   high-DPI model so the same UI is sharp at 1× and 2×.

The convenience API (`ogFillRect`, `ogCircle`, …) never breaks — those calls
stay, but are re-expressed on top of the new path/coverage pipeline so they
inherit AA and correctness for free.

## Non-goals

- **GPU acceleration *inside objGFX*.** objGFX rasterizes into a shared-memory
  buffer; it does not own the framebuffer or talk to a GPU. Hardware
  compositing/blit acceleration is a `views` + kernel-driver concern (see the
  GPU/display-acceleration companion plan) — objGFX's contract is to keep
  producing portable CPU surfaces that *either* a software *or* a GPU
  compositor can consume. (CPU **SIMD** of the software rasterizer is in scope
  — that's a different axis; see "Backend & capability model".)
- **A retained-mode/scene-graph API.** Immediate mode is the contract.
- **A widget toolkit inside objGFX.** Buttons/fields/sliders are a `views`-side
  concern; objGFX supplies their paint primitives only. (`vWidget`/`vWindow`
  are dead legacy and are *not* revived — see the reference doc.)
- **Color management beyond sRGB.** Linear-light sRGB blending is the target;
  ICC profiles / wide-gamut / HDR are not.
- **Breaking the `-lc`/no-STL constraint, or making SIMD/GPU a *requirement*.**
  Every primitive must still run, correctly, scalar-and-software on a CPU with
  no SIMD and no GPU. Acceleration is always additive.

## Backend & capability model

Acceleration is layered in as **swappable backends behind a stable interface**,
the way Cairo (image/GL) and Skia (raster/Ganesh) do it — never as a fork of
the drawing code. Two independent axes, in two different layers:

**Axis 1 — CPU SIMD (objGFX-internal).** The hot span ops — `blend_span`,
`blur_pass`, `blit`, glyph coverage — are reached through a function-pointer
table chosen **once at surface/library init**, not branched per pixel. objGFX
*already* leans this way: every surface carries `getPixel`/`setPixel` function
pointers (`objgfx.h`). The selection rule:

1. **Scalar** is always present and is the reference oracle.
2. At init, probe CPU features — **CPUID** (i386: SSE2 is a safe baseline for any
   Atom/P4-class or newer target; the probe only guards genuine museum pieces)
   and **`ID_AA64PFR0_EL1`** (aarch64: NEON is mandatory in ARMv8-A, so always
   present) — *and* a **kernel capability flag** saying vector state is saved
   across context switches.
3. Only if **both** the CPU reports the feature **and** the kernel saves vector
   state, install the SIMD variant; otherwise stay scalar.

   The CPU almost always has SIMD; the kernel flag is the true gate. Until the
   kernel work below lands, the flag is false and the scalar floor is what runs
   — which is exactly today's behavior, with no code change required to fall
   back. The **gallery test app** diffs SIMD output against scalar to keep the
   accelerated path honest.

   *Kernel prerequisite (tracked separately, not part of this plan's phases):*
   - *i386*: set `CR4.OSFXSR`/`OSXMMEXCPT`; FXSAVE/FXRSTOR the 512-byte XMM area
     in the lazy-FPU path (`mathStateRestore`, the `CR0.TS`/`#NM` mechanism in
     `sys/arch/i386/kern/context_switch.c`); export the capability flag.
   - *aarch64*: save/restore `v0–v31` + `FPSR`/`FPCR` in `aarch64_ctx_switch`
     (`sys/arch/aarch64/kern/context.S`) and in the trapframe (needed before any
     preemptive kernel); export the capability flag.

**Axis 2 — GPU / display hardware (NOT in objGFX).** Today both arches present a
**dumb linear framebuffer**: i386 via VESA (`sys/arch/i386/dev/vesa.c`, a dead
end for accel — needs a native card driver), aarch64 via virtio-gpu used
scanout-only (`virtio_gpu_fb` + `virtio_gpu_flush`, *not* using virtio-gpu's 2D
or virgl command paths). Because **`views` owns the framebuffer** (only it calls
`sys_mapfb()`; apps draw into shm and `views` composites), GPU acceleration
belongs to **`views` + a kernel display driver**, where the realistic early win
is the final composite/blit — not in objGFX. aarch64 is the better first target:
virtio-gpu (and a real Pi/Orange Pi GPU) is a *command* device with an
acceleration path to grow into, whereas VESA is a flat memory window. This work
is its own companion plan; objGFX is unaffected because it keeps emitting CPU
surfaces a GPU compositor can consume unchanged.

## Status matrix

| Phase | Theme | State |
|------:|-------|-------|
| P0 | Compositing & color correctness (premultiplied source-over, gamma-correct blend, AA fills) + the backend-dispatch seam for the span ops | **Not started** |
| P1 | Unified vector path + stroking (reuse `ogEdgeTable`) | Not started |
| P2 | Clip stack + affine transform (CTM), rounded-rect clip | Not started |
| P3 | Gradients & materials (linear/radial, separable blur, generalized shadows) | Not started |
| P4 | High-DPI / points→pixels scale model | Not started |
| P5 | Text excellence (gamma AA, kerning, subpixel, UTF-8, fallback) | Not started |
| P6 | Image pipeline (quality scaling, nine-slice, more decoders, vector icons) | Not started |
| P7 | Design tokens & theming (semantic color roles, light/dark, metrics) | Not started |
| P8 | Motion math (standard easing curves; driver lives in `views`) | Not started |
| — | Ongoing: perf, gallery test app, reference-doc upkeep, both-arch green | Continuous |

Design-only until the user approves a phase. Phases are **roughly** ordered by
dependency, but P0 is a hard prerequisite for everything; P4/P5/P7 can interleave.

---

## P0 — Compositing & color correctness *(the foundation)*

The highest-leverage work, because it makes every later phase look right and is
invisible to the convenience API.

- **Premultiplied source-over.** Define the canonical surface as 32bpp
  premultiplied `0xAARRGGBB`. Add one well-tested `blend_over(dst, src)` span
  routine; route `ogSetBlending`, text, shadows and (later) gradients through it
  instead of the ad-hoc per-site math. `ogBlendColor` becomes a thin wrapper.
- **Gamma-correct blending.** Blend in linear light: a 256-entry sRGB→linear
  LUT and a linear→sRGB step (table-driven; no FP in the inner loop). This alone
  fixes the "grey fringe" on AA text and the dinginess of translucent fills —
  it is the difference both macOS and Win11 made years ago.
- **AA every fill.** Re-implement `ogFillCircle`, `ogFillRoundRect`,
  `ogFillPolygon`, `ogFillTriangle` as coverage fills over `ogEdgeTable`'s
  scanline output (it already exists and already supports gouraud). Add an
  `ogSetAntiAliasing`-respecting coverage path so shapes match the quality of
  lines.
- **Honor the alpha byte.** Stop discarding the top byte on shared buffers;
  define the `views` contract for what premultiplied alpha means at FLIP.
  (Coordinated with `views`/`display_proto`.)
- **Backend-dispatch seam.** Funnel the hot span ops (`blend_over`/`blend_span`,
  glyph-coverage blit, and later `blur_pass`/gradient fill) through a
  per-surface function-pointer table populated at init from a global capability
  struct — extending the `getPixel`/`setPixel` pattern objGFX already uses. P0
  ships **only the scalar implementations** (the oracle); the seam is what lets
  the SIMD backend slot in later with zero changes to callers. No SIMD code in
  P0 itself.

Exit: a translucent, soft-cornered card drawn over a photo looks correct
(no dark halo, no jaggies) on both arches; all blending flows through the one
dispatched span path.

## P1 — Unified vector path + stroking *(the Quartz/Direct2D core)*

- **`ogPath`** builder (flat-array backed, no STL): `moveTo`, `lineTo`,
  `quadTo`, `cubicTo`, `close`; flatten beziers to line segments at a tolerance.
- **Fill** any path via `ogEdgeTable` with nonzero/even-odd fill rule and P0
  coverage AA.
- **Stroke**: width, caps (butt/round/square), joins (miter/round/bevel),
  by expanding the path to a fill outline (no GPU). Hairline fast-path snaps to
  device pixels for crisp 1px borders.
- **Re-express convenience primitives** (`ogRect`, `ogCircle`, `ogRoundRect`,
  `ogArc`, `ogLine`) on top of `ogPath` so there is *one* rasterizer to make
  correct and fast. Public signatures unchanged.

## P2 — Clipping & affine transform *(the CTM + the card-corner essential)*

- **Clip stack**: `ogPushClipRect` / `ogPopClip`, then arbitrary **path clip**.
  Rounded-rect clip is the priority — every Win11 card and macOS sheet clips
  its content to a rounded rect; today apps fake it.
- **Affine transform** (2×3 CTM): `translate`/`scale`/`rotate`/`skew` applied
  to path geometry, with integer fast-paths preserved for the axis-aligned
  common case. Enables P4 (scale) and rotated content.

## P3 — Gradients & materials *(the visual language of 2026)*

- **Gradients**: multi-stop **linear** and **radial** (conic optional), filled
  through the P0 gamma-correct pipeline. Both OSes lean on subtle gradients for
  depth; flat-only reads as dated.
- **Separable box/stack blur** (`ogBlur`): fast two-pass blur — the primitive
  `views` composites into **Acrylic/Mica** (Win11) and **sidebar/menu
  vibrancy** (macOS): blur(backdrop) + tint + optional noise. objGFX provides
  blur+tint; `views` owns the material policy and what gets blurred.
- **Generalized shadows**: fold `ogDropShadow` into a soft inner/outer shadow
  built on `ogBlur` (elevation levels as a token in P7).

## P4 — High-DPI / scale model *(Retina + Win11 display scaling)*

- **Points→pixels**: a per-surface scale factor; a logical coordinate layer so
  apps lay out in points and objGFX rasterizes at device resolution.
  Device-pixel snapping for hairlines and text.
- **@2x asset selection** hook for images/icons.
- Threaded through `views` (window geometry in points) and font sizing; couples
  to the runtime resolution / display-settings work.

## P5 — Text excellence *(what people actually read)*

- **Gamma-correct grayscale AA** (inherited from P0) — biggest text-quality win.
- **Kerning** (stb_truetype kern table) and **subpixel positioning** so runs
  don't visibly snap to integer pens.
- **Beyond Latin-1**: UTF-8 decode + an on-demand glyph cache keyed by codepoint
  (the current fixed `32..255` flat array becomes a small hash/bucket — still
  no STL), enabling real Unicode text.
- **Font fallback chain** (UI → mono → symbol) so missing glyphs don't tofu.
- **System font roles** (UI / mono / display, weights) as named handles, fed by
  P7 tokens. Optional Win11-style RGB **subpixel** AA toggle vs. macOS-style
  grayscale, as a system setting.

## P6 — Image pipeline & resizable art

- **Quality scaling**: bilinear default (bicubic option) replacing nearest in
  `ogScaleBuf` — scaled wallpaper/icons stop looking blocky.
- **Nine-slice / nine-patch** for resizable buttons, panels and chrome from a
  single source bitmap (how both OSes ship stretchable art).
- **More decoders**: JPEG (stb_image already supports it), alpha PNG
  compositing through P0; a small **vector icon** path (icons are vector in both
  OSes — a tiny path format or SVG-lite, not full SVG).

## P7 — Design tokens & theming *(semantic color, light/dark)*

- **Semantic color roles** mirroring `NSColor` system colors / WinUI brushes:
  `accent`, `label`, `secondaryLabel`, `fill`, `separator`, layered
  `background`s — resolved for **light/dark** and an **accent tint**.
- **Metric tokens**: corner-radius scale, spacing scale, elevation/shadow
  levels — so apps stop hardcoding `radius = 8`.
- Sourced at runtime from **ubistry** (Settings drives theme live); couples to
  `ubistry-plan.md`. objGFX ships the token *contract* + sane defaults; the
  values are a settings layer.

## P8 — Motion math *(curves here; the loop lives in views)*

- A small **easing library**: the standard cubic-bezier presets both OSes
  ship (`easeOut`, `easeInOut`, spring approximations) as pure math.
- objGFX provides the curve evaluation; the **animation driver/timeline** is a
  `views` concern (it owns the frame clock and damage). Keeps the draw library
  stateless re: time.

---

## Ongoing (every phase, continuous)

- **A gallery/test app** (`bin/oggallery`, or extend `bin/hello`) that paints
  one panel per primitive — the eyeball regression harness on both arches.
  Stand this up early; it is how P0..P8 are visually verified.
- **Performance — scalar first.** Algorithmic span optimization (run-length
  solid spans, row-pointer blits, avoiding per-pixel function-pointer calls in
  hot loops) is the baseline that runs everywhere and lands *before* any SIMD.
  For a software rasterizer these wins are larger than SIMD. Measure on aarch64
  QEMU (the slow target).
- **SIMD backend — planned, gated on the kernel flag.** The SSE2/NEON
  implementations of the hot span ops (P0 `blend_span`, P3 `blur_pass`, blits,
  glyph coverage) are written against the backend seam from "Backend &
  capability model" and installed at runtime once the kernel saves vector state
  and the CPU reports the feature. It is *additive* — never a dependency for any
  phase, and the scalar path remains the correctness oracle. The kernel
  vector-context-switch work (i386 FXSAVE/FXRSTOR; aarch64 `v0–v31`+`FPSR`) is
  the prerequisite and is tracked as its own kernel task, not a phase here.
- **Reference doc upkeep**: `docs/apps/objgfx-reference.md` and the
  `writing-a-views-app.md` tutorial are updated **in the same change** as any
  API addition (standing convention for the apps docs).
- **Both arches green**: `bmake kernel world TARGET=i386` and `TARGET=aarch64`
  after every phase; scalar-software baseline intact, no STL, no `sys/arch/`
  churn from objGFX itself.

## Companion plans

- `views-polish-plan.md` — the compositor/taskbar layer *above* objGFX. The
  material (Acrylic/Mica/vibrancy), window elevation and animation policy land
  there and **consume** P3/P8 primitives. Translucency/blur in views is blocked
  on P0+P3.
- `ubistry-plan.md` — supplies the live theme/token values for P7 (light/dark,
  accent) via Settings.
- `display-settings` / runtime-resolution work — supplies the scale factor and
  device resolution P4 builds on.
- **GPU / display-acceleration plan (to be written, owned by views + drivers).**
  Axis 2 from "Backend & capability model": grow the kernel display driver past
  dumb-framebuffer scanout and add an accelerated compositing/blit backend to
  `views`. aarch64/virtio-gpu (then real Pi/Orange Pi GPU) is the first target
  because it is a command device; i386/VESA stays software-composited unless a
  native card driver is ever written. objGFX is unaffected — it keeps emitting
  CPU surfaces that either compositor consumes. Couples to `views-polish-plan.md`.
- **Kernel FP/vector context-switch (prerequisite for the SIMD backend).** Save
  the vector register file across context switches per arch (i386 FXSAVE/FXRSTOR
  + `CR4.OSFXSR`; aarch64 `v0–v31`+`FPSR/FPCR` in `context.S`/trapframe) and
  export a capability flag. Tracked with the SMP/preemption work, not here.
