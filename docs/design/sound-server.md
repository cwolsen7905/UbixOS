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

## Status Matrix

Legend: ✅ done & verified · 🟡 partial · ⬜ not started

| Phase | Item | Status | Notes |
|-------|------|--------|-------|
| — | Kernel AC97 driver `/dev/audio` (s16 48 kHz stereo + vol/mute ioctls) | ✅ | i386 `sys/pci/ac97.c`; single-stream |
| — | virtio-sound `/dev/audio` (same `dev_char_write`/`ioctl` contract, major 20) | ✅ | aarch64 `sys/arch/aarch64/dev/virtio_sound.c` — so `aural` is **dual-arch** |
| — | `libaudio` client API (`audio_open/write/set_rate` + vol/mute) | ✅ | `lib/libaudio/`; used by aplay/mp3play/doom |
| — | Stop-gap master-volume persistence (`sndcfg` + `/aural/*` ubistry) | ✅ | retired when Phase 3 lands |
| 1 | `aural_proto.h` — protocol DTOs + shared SPSC ring + helpers | ✅ | `include/audio/aural_proto.h` |
| 1 | `bin/aural` server (C++: AudioSink / StreamRegistry / Mixer / AuralServer) | ✅ | **works on aarch64** — boots in init.d, mixes live (vDoom + Tessera together). Nap-paced loop (real `nanosleep`), per-stream priming + variable-length feed |
| 1 | Dead-client reaper (`kill(pid,0)` → free ring + stream) | ✅ | `StreamRegistry::reap()` |
| 1 | `aural` reads master vol/mute from ubistry; owns it (decision #6) | ✅ | `AudioSink::open_device` → `apply_saved_settings` |
| 1 | `libaudio` veneer (`audio_open` → `AURAL_CLAIM`, no app changes) | ✅ | `lib/libaudio/audio.c`; legacy `/dev/audio` fallback when aural absent. **Key fix:** must set `msg.header` before `mpi_postMessage` (MPI delivers the field, not the type arg) — see BUG-AUDIO. Added `ubix_api.a` to aplay/mp3play/tessera/settings link |
| 1 | Wire into `bin/Makefile` SUBDIRS (both arches) + `etc/init.d/17-aural` | ✅ | both arches build; aarch64 boots desktop + audio with it |
| 1 | Remove `sndcfg` (bin/sndcfg, 17-sndcfg, SUBDIRS, makereg.c, settings comment) | ✅ | done; stale build artifact purged |
| 1 | Real aarch64 `nanosleep` (was yield-once) — daemon pacing primitive | ✅ | `sys/arch/aarch64/kern/syscall.c` via `sched_wait_event_timeout`; i386 generic nanosleep still busy-yields (TODO) |
| — | **Choppy/underrun audio under load (esp. intermittent streams)** | 🟡 | mitigated (priming + variable feed); residual = likely OS perf, not aural — tracked as **BUG-AUDIO-01** in BUGS.md |
| 1 | **Blocking-write pacing** in both drivers (`ac97.c` + `virtio_sound.c`) | ⬜ | superseded by nanosleep pacing for now; an IRQ-driven completion would tighten it (see BUG-AUDIO-01) |
| 2 | N-client mixing (sum + clamp two streams) | ✅ | **verified** — vDoom + Tessera mix simultaneously on aarch64 |
| 3 | Volume model (per-stream gain + server master; retire `sndcfg`) | ⬜ | Settings Sound pane points at `aural` |
| 4 | Client migration + polish (aplay/mp3play/vdoom; rate/format convert) | ⬜ | one app at a time behind the API |

The device + client-API half is built; the **mixer server itself (Phases 1–4)
is not started** — concurrent players still fight over the single-open device.

---

## Decisions (locked 2026-06-10)

These are settled for this version of uBixOS; the architecture doc
(`docs/architecture/audio.md`) is written against them.

1. **Blocking-write pacing.** `aural`'s mix loop is clocked by a **blocking
   `write()` to `/dev/audio`** that returns when a DMA period drains. This
   blocking/poll path is **Phase 1 kernel work** and must land in **both**
   drivers (`sys/pci/ac97.c` i386, `sys/arch/aarch64/dev/virtio_sound.c`
   aarch64). No busy-polling, no guess-timer.
2. **`aural` is hardware-generic → dual-arch from day one.** It knows only the
   generic `/dev/audio` contract (write PCM, `AUDIO_*` ioctls, blocking pace,
   master-volume ioctl), never AC97/virtio internals. Both drivers already
   present that identical contract (same `dev_char_write`/`dev_char_ioctl`,
   major 20), so the same `aural` binary runs on i386 and aarch64.
3. **`/dev/audio` stays the raw hardware node, `aural`-private** (mirrors "only
   `views` opens `sys_mapfb`"). Normal apps never open it.
4. **Compatibility = the `libaudio` veneer only.** `audio_open`/`audio_write`/…
   are reimplemented on top of the `aural` protocol (shared ring + MPI), so
   every current app (`aplay`/`mp3play`/vDoom, all of which use `libaudio`)
   becomes a mixed client **with no source change**. **No** kernel
   `/dev/audio`→server forwarding for raw `open()+write()` apps — none exist.
5. **Control plane / data plane split.** MPI carries only control
   (`CLAIM`/`START`/`STOP`/`SET_GAIN`/`SET_MASTER`/`RELEASE`); PCM lives only in
   the shared SPSC ring. Empty ring at mix time = silence (one slow client never
   stalls the master).
6. **Master volume lives in ubistry; `sndcfg` is removed (not retired later).**
   `aural` reads `/aural/volume` + `/aural/mute` from ubistry at startup
   (`ubistry_get_int`), applies them to the codec, and writes them back on
   `AURAL_SET_MASTER` — so `aural` owns master volume from day one. The one-shot
   `bin/sndcfg` + `etc/init.d/17-sndcfg` are deleted; the Settings Sound pane
   keeps writing the same ubistry keys. This also removes the boot-ordering
   concern (no `sndcfg` for `aural` to sequence against).

Out of scope this version: capture/recording, effects/EQ graph, network audio,
hot device-switch, per-app volume UI beyond a flat list.

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

- **Kernel `/dev/audio` driver on both arches** — i386 AC97 (`sys/pci/ac97.c`)
  and aarch64 virtio-sound (`sys/arch/aarch64/dev/virtio_sound.c`) both expose
  the **same** char-device contract (`dev_char_write` / `dev_char_ioctl`, major
  20): 16-bit stereo PCM at 48 kHz out of a single ring, with `AUDIO_SET_VOLUME`
  / `AUDIO_SET_MUTE` master controls. Because the contract is identical, `aural`
  is hardware-generic and runs on both architectures unchanged. *(What is **not**
  there yet on either driver: a blocking `write()` that returns when the DMA
  period drains — decision #1, Phase 1 kernel work, is to add it.)*
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
its ring straight through. `aplay` (via the `libaudio` veneer) still plays.
Includes the foundational pieces all later phases depend on:
- **Blocking-write pacing** in both drivers (decision #1) — `aural`'s loop is
  clocked by `write()` returning on DMA-period drain. *Touches `sys/pci/ac97.c`
  and `sys/arch/aarch64/dev/virtio_sound.c`; verify both arches still play a
  tone.*
- **`libaudio` veneer** (decision #4) — `audio_open` → `AURAL_CLAIM` + shared
  ring; existing apps unchanged.
- **Dead-client reaper** (the views lesson) — `aural` reclaims a stream whose
  client died without `AURAL_RELEASE`.

**Risk:** Low–medium — no mixing yet, but the blocking-write change touches the
kernel drivers; proves the claim/ring/MPI plumbing + pacing on both arches.

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
