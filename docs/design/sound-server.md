# UbixOS Sound Server Design Plan — `aural`

## Goal

Build a userland sound server, **`aural`**, that owns the audio device and
mixes every application's audio into one output stream — the audio counterpart
of the `views` compositor. It is named to parallel `views`: `views` is what you
see, `aural` is what you hear.

The core design principle is identical to the display system: **shared-memory
buffers, MPI as signaling only.** An app writes PCM samples into its own shared
ring; `aural` mixes all client rings and writes the blend to the codec. MPI
carries control signals (claim / start / stop / set-volume / release) — never
sample data. This keeps the door open to richer mixing, effects, and resampling
later without a protocol change.

This solves a real bug in today's audio: the AC97 ring at `/dev/audio` is
**single-stream** — whoever opens it owns it, so `aplay` and vDoom currently
fight over the device. With `aural`, every app mixes cleanly, and master volume
becomes a property of the server instead of a one-shot applier.

---

## How Other Systems Did It

| System | Who owns the codec | App produces audio how | Mixer |
|--------|--------------------|------------------------|-------|
| macOS / Core Audio | `coreaudiod` (userspace) | Render into ring, signal via Mach port | `coreaudiod` |
| Linux / PulseAudio | `pulseaudio` (userspace) | Write to shared memfd, signal over socket | `pulseaudio` |
| Linux / PipeWire | `pipewire` (userspace) | Graph nodes over shared buffers | `pipewire` |
| OpenBSD / sndiod | `sndiod` (userspace) | Write to socket / shared buffer | `sndiod` |
| BeOS / media_server | `media_server` (userspace) | Buffer hand-off over IPC | `media_server` |
| **UbixOS** | **`aural`** (userspace) | Write PCM into a shared `vmm_share_region` ring, signal via MPI | **`aural`** |

UbixOS `aural` is closest to macOS `coreaudiod` / OpenBSD `sndiod` in concept,
but reuses exactly the machinery `views` already uses: `vmm_share_region` for
the sample buffers and MPI for control — no sockets, no new kernel subsystem.

The symmetry with the display stack is the whole point:

| Display (`views`) | Audio (`aural`) |
|-------------------|-----------------|
| Owns the framebuffer (`sys_mapfb`) | Owns `/dev/audio` (the AC97 DMA ring) |
| Client gets a `vmm_share_region` window buffer | Client gets a `vmm_share_region` PCM ring |
| Composites all windows → screen | Mixes all client streams → codec |
| MPI: `DISPLAY_CLAIM` / `FLIP` / `RELEASE` | MPI: `AURAL_CLAIM` / `START` / `STOP` / `RELEASE` |
| Per-window Z-order, focus | Per-stream gain, master volume |
| `views` mailbox | `aural` mailbox |

---

## Current State

- **Kernel AC97 driver** (`sys/pci/ac97.c`) works: 16-bit stereo PCM at 48 kHz
  out of a single kernel ring buffer exposed as `/dev/audio` (char device).
  Master/PCM mixer volume + mute are controllable via the `AUDIO_SET_VOLUME` /
  `AUDIO_SET_MUTE` ioctls (added alongside this plan).
- **`libaudio`** (`lib/libaudio/`) is the client API: `audio_open` →
  `open("/dev/audio")`, `audio_write`, `audio_set_rate`, plus the new
  volume/mute helpers. `aplay`, `mp3play`, and `doom` write directly to the
  device through it.
- **No mixing.** The device is single-open; concurrent players conflict.
- **Master volume persistence** is a stop-gap: the Settings Sound pane writes
  `/aural/volume` + `/aural/mute` to ubistry, and `bin/sndcfg` (an `init.d`
  one-shot) applies them to the codec at boot. When `aural` exists, it
  subsumes both — the server owns master volume and applies it on startup, and
  `sndcfg` is retired.
- MPI + `vmm_share_region` are proven by `views`.

---

## Protocol (MPI control + shared ring)

A client never writes to `/dev/audio`. Instead:

1. Client allocates a PCM ring (`aligned_alloc` + `_sys_shareregion` to the
   `aural` pid), exactly as a `views` client allocates its window buffer.
2. Client sends `AURAL_CLAIM` to the `aural` mailbox with `{shm_vaddr, bytes,
   rate, channels, format}` and its reply mailbox. `aural` maps the ring and
   replies `AURAL_ACK` with a stream id.
3. Client fills the ring and advances a shared write index; `aural`'s mix loop
   drains all rings by their read/write indices (same lock-free single-producer
   / single-consumer discipline the AC97 ISR already uses).
4. Control messages adjust a stream without touching the data path:
   `AURAL_START`, `AURAL_STOP`, `AURAL_SET_GAIN` (per-stream), `AURAL_RELEASE`.
5. Master volume / mute are server-wide: `AURAL_SET_MASTER` (also written to
   `/aural/volume` for persistence). The Settings Sound pane talks to `aural`
   instead of ioctl'ing `/dev/audio` directly.

Sample data lives only in the shared rings; MPI messages stay tiny and fixed —
mirroring "MPI carries only signals, never drawing commands" from the display
plan.

---

## Mixing

`aural`'s output loop, woken by the AC97 buffer-completion path (or a periodic
tick), for each output DMA buffer:

1. Zero a 32-bit accumulator buffer (`AC97_BUF_FRAMES` stereo frames).
2. For each active stream: read available frames from its ring, apply the
   stream gain, accumulate into the mix (with resampling/format conversion if
   the stream rate ≠ 48 kHz — initially require 48 kHz/stereo/s16 and reject
   others).
3. Apply master volume, clamp to s16, and `audio_write()` the mix to
   `/dev/audio`.

Clamping (not wrapping) on sum overflow is essential — the display plan's
equivalent is alpha-clamped compositing.

---

## Phases

### Phase 1 — Passthrough server
**Result:** `aural` runs, owns `/dev/audio`, accepts a single client and copies
its ring straight through. `aplay` migrated to the client API still plays.
**Risk:** Low — no mixing yet; proves the claim/ring/MPI plumbing.

### Phase 2 — N-client mixing
**Result:** Two clients (e.g. `mp3play` + a game) play simultaneously, summed
and clamped. This is the headline feature.
**Risk:** Medium — the mix loop timing vs. the AC97 buffer cadence is the
delicate part; reuse the existing ISR ping-pong budget.

### Phase 3 — Volume model
**Result:** Per-stream gain + master volume in the server; master persists via
`/aural/volume`. `sndcfg` retired; the Settings Sound pane points at `aural`.
**Risk:** Low.

### Phase 4 — Client migration + polish
**Result:** `aplay`, `mp3play`, `doom`/`vdoom` all use the `libaudio` client
path. Optional: rate/format conversion so non-48 kHz sources work; a per-app
volume list in the Sound pane.
**Risk:** Medium — touches each audio app; do one at a time behind the API.

---

## Relationship to the current stop-gap

Until Phase 3 lands, persistence is handled by `bin/sndcfg` + the `/aural/*`
ubistry keys + the AC97 volume ioctls. These are deliberately named and scoped
so that `aural` can adopt them wholesale: the keys stay, the ioctls become the
server's private channel to the codec, and `sndcfg` is deleted once the server
applies master volume itself.

---

## Out of scope (future)

Recording/capture (`/dev/audio` is output-only today); per-app volume UI beyond
a flat list; audio effects/EQ graph; network audio; hot device switching.
