# UbixOS Audio System Architecture — `aural`

> **Status (2026-06-10): target design — partially built.**
> The kernel `/dev/audio` device and the `libaudio` client library exist and
> work **single-stream** today (AC97 on i386, virtio-sound on aarch64). The
> **`aural` mixer server** that lets multiple apps play at once is **not built
> yet** — Phases 1–4 in `docs/design/sound-server.md`. This document describes
> the model the implementation builds toward and the rules app authors should
> follow; sections describing `aural` itself are the spec until those phases
> land. The `/dev/audio` contract and `libaudio` API below are real today.

## Design principle

**Shared-memory ring buffers; MPI as signaling only.**

This is the exact twin of the display system. Sound hardware is a **single
stream**: one DMA ring playing one interleaved PCM stream at a fixed rate. So
exactly one process — the **`aural`** mixer server — owns the hardware device
and mixes every app's audio into that one stream. Apps write PCM into a
shared-memory ring `aural` reads; MPI carries only control signals
(claim / start / stop / set-gain / release), **never sample data**. This keeps
the door open to resampling, effects, and routing later without a protocol
change.

> `aural` is to sound what `views` is to the screen. If you know the display
> stack, you already know this one.

| Display (`views`) | Audio (`aural`) |
|---|---|
| Owns the framebuffer (`sys_mapfb`) | Owns `/dev/audio` (the codec DMA ring) |
| Client gets a `vmm_share_region` **window buffer** | Client gets a `vmm_share_region` **PCM ring** |
| Composites all windows → screen | Mixes all client streams → codec |
| MPI: `DISPLAY_CLAIM`/`FLIP`/`RELEASE` | MPI: `AURAL_CLAIM`/`START`/`STOP`/`RELEASE` |
| Per-window Z-order, focus | Per-stream gain, master volume |

### Two governing rules (memorize these)

1. **`aural` is the only process that opens `/dev/audio`.** Everyone else gets a
   `vmm_share_region` ring. (The audio mirror of "only `views` calls
   `sys_mapfb()`".)
2. **MPI carries only control signals, never PCM.** Samples live solely in the
   shared ring. Control plane and data plane are separate — that separation is
   what makes effects/routing addable later without breaking the protocol.

---

## Component map

```
  ┌───────────────────────────────────────────────────────────────┐
  │  Kernel — the /dev/audio contract (same on both arches)        │
  │                                                               │
  │  i386:     sys/pci/ac97.c          → /dev/audio (major 20)    │
  │  aarch64:  sys/arch/aarch64/dev/virtio_sound.c → /dev/audio   │
  │                                                               │
  │    dev_char_write  – queue one period of PCM for playback     │
  │    dev_char_ioctl  – AUDIO_* control (rate, master vol, mute) │
  │    blocking write  – returns when the DMA period drains       │
  │                      (this is what paces aural's mix loop)    │
  └───────────────────────────────┬───────────────────────────────┘
                                  │ open + blocking write + ioctl
                                  │ (aural ONLY)
  ┌───────────────────────────────▼───────────────────────────────┐
  │  bin/aural  (mixer server)                                    │
  │                                                               │
  │  • Owns /dev/audio (opens it; nobody else does)               │
  │  • Per stream: shared PCM ring, gain, rate/format, state      │
  │  • Mix loop (paced by the blocking write):                    │
  │      zero accumulator → sum each active ring × gain →         │
  │      apply master volume → clamp to s16 → write /dev/audio    │
  │  • Empty ring at mix time = silence (never stalls the master) │
  │  • Reaps dead clients (kill(pid,0)) → frees ring + stream     │
  │  • Owns master volume; persists to /aural/volume (ubistry)    │
  └──────┬──────────────────────────────────┬─────────────────────┘
         │ MPI control                      │ MPI control
         │ + shared PCM ring                │ + shared PCM ring
  ┌──────▼───────────────────┐    ┌─────────▼─────────────────────┐
  │  POSIX / ported app      │    │  Native uBixOS app            │
  │  (aplay, mp3play, vDoom) │    │                               │
  │                          │    │  Uses the aural protocol      │
  │  Uses libaudio's device  │    │  directly (the layer libaudio │
  │  API UNCHANGED:          │    │  sits on): named streams,     │
  │    audio_open()          │    │  per-stream gain, future      │
  │    audio_write()         │    │  effects/sync.                │
  │  → libaudio veneer turns │    │                               │
  │    these into an aural   │    │  AURAL_CLAIM / START / STOP / │
  │    stream transparently  │    │  SET_GAIN / RELEASE           │
  └──────────────────────────┘    └───────────────────────────────┘
```

---

## The `/dev/audio` contract (kernel, both arches — exists today)

`aural` depends only on this generic device contract, never on AC97- or
virtio-specific details, so the same server binary runs on both architectures:

| Operation | Meaning |
|---|---|
| `open("/dev/audio")` | Acquire the hardware stream (single-open; `aural` is the owner) |
| `write(fd, pcm, len)` | Queue one period of PCM. **Blocks until a DMA period drains** — this is the mixer's clock (decision: blocking-write pacing lands in both `ac97.c` and `virtio_sound.c`). |
| `ioctl(AUDIO_SET_RATE)` | Set sample rate (initially 48 kHz) |
| `ioctl(AUDIO_SET_VOLUME / AUDIO_SET_MUTE)` | Master codec volume / mute — becomes `aural`'s private channel to the hardware |

Format baseline: **48 kHz, 16-bit signed, stereo, interleaved.** Other
rates/formats are rejected at `CLAIM` until resampling lands (Phase 4).

> **Naming:** `/dev/audio` is the **raw hardware node** and, once `aural` exists,
> is **`aural`-private**. Normal apps never open it.

---

## `aural` MPI protocol  (`include/audio/aural_proto.h` — to be created)

Typed, fixed-size, versioned DTOs (same discipline as `display_proto.h`;
validated at the boundary). Final struct layouts are defined when Phase 1 lands;
the contract is:

| Message | Direction | Purpose |
|---|---|---|
| `AURAL_CLAIM` | client → aural | Register a stream: `{ver, shm_vaddr, bytes, rate, channels, format, reply_mbox}` |
| `AURAL_ACK` | aural → client | Stream granted: `{stream_id}` |
| `AURAL_NAK` | aural → client | Refused: `{reason, supported_format}` (client converts + retries) |
| `AURAL_START` / `AURAL_STOP` | client → aural | Begin / pause draining this stream |
| `AURAL_SET_GAIN` | client → aural | Per-stream gain `{stream_id, gain}` |
| `AURAL_SET_MASTER` | client → aural | Server-wide master volume (also persisted) |
| `AURAL_RELEASE` | client → aural | Tear the stream down (ring unmapped, slot freed) |

All messages fit within `MESSAGE_LENGTH` (248 bytes). MPI never carries PCM.

---

## Shared PCM ring (the data plane)

`vmm_share_region` (syscall 45) — the same mechanism `views` uses for window
buffers — puts the PCM ring into both address spaces:

1. The client allocates the ring (`aligned_alloc`) and shares it to `aural`'s
   pid (`_sys_shareregion`), exactly as a `views` client shares its window
   buffer.
2. `AURAL_CLAIM` hands `aural` the ring's address, size, and format.
3. The ring is a **lock-free single-producer / single-consumer** queue: the
   client advances a `write_idx`, `aural` advances a `read_idx`, both stored in
   the ring header. Size is a power of two for cheap index masking. This is the
   same SPSC discipline the AC97 ISR ping-pong already uses.
4. `aural` drains available frames each mix period. An empty ring contributes
   **silence** — a slow client never stalls the master mix.

---

## What POSIX / ported apps do (the `libaudio` veneer)

Ported software (`aplay`, `mp3play`, vDoom) keeps using `libaudio`'s
device-style API **unchanged** — the veneer turns those calls into an `aural`
stream under the hood, so every existing audio app gets mixed for free with no
source change:

```c
#include <audio/audio.h>

int h = audio_open();              /* internally: AURAL_CLAIM + shared ring   */
audio_set_rate(h, 48000);
audio_write(h, pcm, nbytes);       /* internally: copy into ring, advance idx */
audio_set_volume(h, 80);           /* internally: AURAL_SET_GAIN (per stream) */
audio_close(h);                    /* internally: AURAL_RELEASE               */
```

No raw `open("/dev/audio")+write()` path is forwarded by the kernel — every
audio app goes through `libaudio`, so the veneer covers 100% of them.

## What native uBixOS apps do (the `aural` protocol)

Native apps that want per-stream control, named streams, or (later) effects and
A/V sync talk the `aural` protocol directly — the lower layer `libaudio` sits
on, the same way a native GUI app uses `objGFX`/the display protocol instead of
poking the framebuffer:

```c
// 1. Allocate a PCM ring and share it to aural
void *ring = aligned_alloc(PAGE_SIZE, RING_BYTES);
_sys_shareregion(ring, RING_BYTES, aural_pid);

// 2. AURAL_CLAIM {ring, RING_BYTES, 48000, 2, FMT_S16}, await AURAL_ACK → stream_id
// 3. Fill the ring, advance write_idx; AURAL_START
// 4. AURAL_SET_GAIN as needed; AURAL_RELEASE on exit
```

**Rule of thumb:** ported app → `libaudio` (device API, unchanged).
Native app → `aural` protocol (richer control). Both are mixed identically by
the server.

---

## Decisions (locked 2026-06-10)

1. **Blocking-write pacing** in the kernel drivers (both `ac97.c` and
   `virtio_sound.c`) clocks `aural`'s mix loop — Phase 1 kernel work.
2. **`aural` is hardware-generic.** It knows only the `/dev/audio` contract;
   AC97 and virtio-sound both implement it → `aural` is **dual-arch from day
   one** (both drivers already exist and present the same interface).
3. **`/dev/audio` stays the raw hardware node, `aural`-private; compatibility is
   the `libaudio` veneer only** — no kernel `/dev/audio`→server forwarding (no
   current app needs it).
4. **Master volume lives in ubistry; `aural` owns it; `sndcfg` is removed.**
   `aural` reads `/aural/volume` + `/aural/mute` at startup
   (`ubistry_get_int`) and applies them to the codec; `AURAL_SET_MASTER` updates
   both the codec and the ubistry keys. The old one-shot `bin/sndcfg` /
   `etc/init.d/17-sndcfg` are deleted. The Settings Sound pane keeps writing the
   same ubistry keys.

---

## Adding an audio app

- **Porting POSIX software:** link `libaudio`, use `audio_open`/`audio_write`/
  `audio_close`. Nothing aural-specific to learn — it becomes a mixed client
  automatically.
- **Writing a native app:** create an MPI mailbox, allocate + share a PCM ring,
  `AURAL_CLAIM`, fill the ring + `AURAL_START`, `AURAL_RELEASE` on exit. See
  `docs/apps/writing-an-aural-app.md` (to be written alongside Phase 1).

```sh
bmake world && bmake image && bmake run        # i386
bmake world image-arm TARGET=aarch64 && bmake run-aarch64 TARGET=aarch64
```

---

## Out of scope (this version)

Recording/capture (`/dev/audio` is output-only), an effects/EQ graph, network
audio, hot device switching, and per-app volume UI beyond a flat list. The
control/data-plane split keeps all of these additive.
