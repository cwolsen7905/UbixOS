# UbixOS AC'97 Sound Support

## Overview

Add PCM audio output to UbixOS via the Intel 82801AA AC'97 controller (PCI
8086:2415), which is QEMU's default audio device.  The design follows the
existing newbus-lite pattern: a PCI driver in `sys/pci/` with `probe`/`attach`,
a kernel ring buffer exposed as `/dev/audio`, and a thin `lib/libaudio/` for
userland.

QEMU invocation:

```sh
-audiodev coreaudio,id=snd0 -device AC97,audiodev=snd0
```

---

## Status

| Phase | Name | Status | Notes |
|-------|------|--------|-------|
| 1 | PCI probe + BAR setup | Done | sys/pci/ac97.c, sys/include/pci/ac97.h |
| 2 | Codec init + sample-rate | Done | NAM register programming |
| 3 | BDL + PCM out DMA | Done | 32-entry descriptor ring, ping-pong |
| 4 | IRQ handler + ring buffer | Done | kernel audio ring, ISR refill |
| 5 | `/dev/audio` VFS node | Done | devfs char write hook in sys/include/sys/bus.h + devfs.c |
| 6 | userland `lib/libaudio/` | Done | audio_open/write/close wrappers, libaudio.a |
| 7 | Test tone app | Done | bin/aplay — 440 Hz square wave, 3 sec; links libaudio.a |

---

## Standing Rules

- Follow newbus-lite: `probe` returns 0 on match, `attach` claims all resources.
- Use `ubx_alloc_ioport` / `ubx_alloc_irq` — no raw `setVector` calls in the driver.
- All DMA memory via `dma_alloc` — never use `kmalloc` for hardware descriptors.
- The driver never touches kernel page tables directly; `ubx_alloc_ioport` is
  sufficient for I/O-space BARs on x86.
- No floating-point in the driver.  Sample-rate maths use integer arithmetic.
- `/dev/audio` accepts raw signed 16-bit stereo little-endian PCM at the
  configured sample rate.  Format negotiation (ioctl) is Phase 6+.

---

## Hardware Background

### PCI Identity

| Field | Value |
|-------|-------|
| Vendor | 0x8086 (Intel) |
| Device | 0x2415 (82801AA AC'97 Audio) |
| Class / Subclass | 0x04 / 0x01 (Multimedia Audio) |

### BARs

| BAR | Name | Size | Description |
|-----|------|------|-------------|
| BAR0 | NAM | 256 bytes | Native Audio Mixer — codec registers |
| BAR1 | NABM | 64 bytes | Native Audio Bus Master — DMA control |

Both are I/O-port regions on QEMU (bit 0 of the raw BAR value = 1).  Strip
that bit before using as a port base.

### NAM Registers (offset from BAR0)

| Offset | Name | Notes |
|--------|------|-------|
| 0x00 | Reset | Write any value to cold-reset codec |
| 0x02 | Master Volume | 0x0000 = full volume, 0x8000 = mute |
| 0x04 | Headphone Volume | same encoding as master |
| 0x18 | PCM Out Volume | 0x0000 = full, 0x8000 = mute |
| 0x28 | Audio Status | bit 0 = primary codec ready |
| 0x2A | PCM Front DAC Rate | sample rate in Hz (e.g. 48000) |
| 0x7C | Vendor ID 1 | read-only codec ID |
| 0x7E | Vendor ID 2 | read-only codec ID |

### NABM Registers (offset from BAR1)

Three DMA channels share the same layout at:

| Channel | NABM base offset |
|---------|-----------------|
| PCM In | +0x00 |
| PCM Out | +0x10 |
| Mic In | +0x20 |

Per-channel register layout (add to channel base):

| Offset | Width | Name | Notes |
|--------|-------|------|-------|
| +0x00 | 32-bit | BDL_ADDR | Physical address of BDL array |
| +0x04 | 8-bit | CIV | Current index value (RO) |
| +0x05 | 8-bit | LVI | Last valid index (RW, 0–31) |
| +0x06 | 16-bit | SR | Status (write 1 to clear bits) |
| +0x08 | 16-bit | PICB | Samples remaining in current buffer |
| +0x0A | 8-bit | PIV | Prefetched index (RO) |
| +0x0B | 8-bit | CR | Control (RPBM/IOCE/RR bits) |

**SR bits:**
```
bit 0  CELV  — current entry == last valid
bit 1  LVBCI — last valid buffer completion interrupt
bit 2  BCIS  — buffer completion interrupt status  ← clear in ISR
bit 3  FIFOE — FIFO error
bit 4  DCH   — DMA controller halted
```

**CR bits:**
```
bit 0  RPBM  — run (1) / pause (0) DMA
bit 1  RR    — reset registers (self-clearing)
bit 2  LVBIE — LVBCI interrupt enable
bit 3  FEIE  — FIFO error interrupt enable
bit 4  IOCE  — BCIS interrupt on completion enable
```

Global registers (no channel prefix):

| Offset | Width | Name | Notes |
|--------|-------|------|-------|
| 0x2C | 32-bit | GLOB_CNT | bit 1 = cold-reset release; bit 0 = global IRQ enable |
| 0x30 | 32-bit | GLOB_STA | bit 8 = primary codec ready; bits [2:0] = per-channel interrupt flags |

### Buffer Descriptor List (BDL)

An array of up to 32 entries, physically contiguous, 8 bytes each:

```c
struct ac97_bdle {
    uint32_t  addr;   /* physical address of PCM buffer */
    uint16_t  len;    /* sample count (stereo 16-bit: bytes/4) */
    uint16_t  flags;  /* bit 15=IOC, bit 14=BUP (stop-on-underrun) */
};
```

The hardware advances CIV when it finishes each entry.  LVI marks the last
valid entry; the hardware halts (DCH) if CIV catches up to LVI.  Setting
`IOC` on an entry triggers an interrupt when that entry completes — use this
for ping-pong refill.

---

## Phase 1 — PCI Probe + BAR Setup

**Files:**
- `sys/pci/ac97.c` (new)
- `sys/include/pci/ac97.h` (new)
- `sys/pci/pci.c` — add `&ac97_ubx_driver` to `pci_drv_table[]`
- `sys/pci/Makefile` — add `ac97.c`

**`ac97_probe`:**

```c
static int
ac97_probe(struct ubx_device *dev)
{
    if (dev->dev_vendor == 0x8086 && dev->dev_device_id == 0x2415)
        return 0;
    return -1;
}
```

**`ac97_attach`:**

1. Read raw BAR0 and BAR1 from PCI config space via `pciConfigRead`.
2. Strip the I/O-space flag bit (bit 0): `nam_base = bar0 & ~3;`
   `nabm_base = bar1 & ~3;`
3. Call `ubx_alloc_ioport(dev, nam_base,  256)`.
4. Call `ubx_alloc_ioport(dev, nabm_base, 64)`.
5. Enable PCI bus mastering and I/O space: write PCI command register with
   bits 0 (I/O) and 2 (bus master) set.
6. Read PCI interrupt line (config offset 0x3C) for the IRQ number.
7. Call `ubx_alloc_irq(dev, irq_line, ac97_isr)`.
8. Store `nam_base`, `nabm_base`, softc pointer in `dev->dev_softc`.
9. Call `ac97_codec_init(sc)` (Phase 2).
10. Call `ac97_dma_init(sc)` (Phase 3).
11. Register `/dev/audio` devfs node (Phase 5).
12. `kprintf("ac97: NAM=0x%X NABM=0x%X IRQ=%d\n", nam_base, nabm_base, irq_line);`

**Softc struct (`sys/include/pci/ac97.h`):**

```c
#define AC97_BDL_ENTRIES   32
#define AC97_BUF_SAMPLES   2048          /* samples per buffer (stereo pairs) */
#define AC97_BUF_BYTES     (AC97_BUF_SAMPLES * 4)  /* 16-bit stereo */

struct ac97_bdle {
    uint32_t  addr;
    uint16_t  len;
    uint16_t  flags;
};

struct ac97_softc {
    uint16_t  nam_base;    /* BAR0 I/O port base */
    uint16_t  nabm_base;   /* BAR1 I/O port base */
    uint8_t   irq;

    struct dma_buf  bdl_dma;                 /* BDL descriptor ring */
    struct ac97_bdle *bdl;                   /* virtual alias of bdl_dma */

    struct dma_buf  buf_dma[2];              /* ping-pong PCM buffers */
    uint8_t        *buf[2];                  /* virtual aliases */

    /* Kernel ring buffer — written by sys_write, drained by ISR */
    uint8_t   ring[AC97_BUF_BYTES * 4];     /* 4× buffer worth */
    uint32_t  ring_rd;
    uint32_t  ring_wr;

    uint8_t   next_buf;    /* which ping-pong buffer to fill next */
    uint8_t   lvi;         /* current last-valid-index in hardware */
};
```

---

## Phase 2 — Codec Initialisation

**`ac97_codec_init(struct ac97_softc *sc)`:**

```c
/* 1. Release cold reset */
outl(sc->nabm_base + 0x2C, 0x00000002);   /* GLOB_CNT: cold reset release */
/* QEMU responds quickly; real hardware needs ~50 ms — spin max 1000 iterations */
for (i = 0; i < 1000; i++) {
    if (inl(sc->nabm_base + 0x30) & 0x00000100)  /* primary codec ready */
        break;
    /* sched_yield() if needed */
}

/* 2. Reset codec registers */
outw(sc->nam_base + 0x00, 0xFFFF);

/* 3. Unmute master and PCM out */
outw(sc->nam_base + 0x02, 0x0000);   /* master volume: full */
outw(sc->nam_base + 0x04, 0x0000);   /* headphone: full */
outw(sc->nam_base + 0x18, 0x0000);   /* PCM out: full */

/* 4. Set sample rate — most codecs default to 48000 after reset.
 *    Variable-rate support requires checking the 'VRA' extended cap first.
 *    For QEMU just write 48000 directly. */
outw(sc->nam_base + 0x2A, 48000);
```

---

## Phase 3 — BDL + PCM Out DMA

**`ac97_dma_init(struct ac97_softc *sc)`:**

```c
/* Allocate BDL — must be 8-byte aligned */
dma_alloc(sizeof(struct ac97_bdle) * AC97_BDL_ENTRIES, 8, &sc->bdl_dma);
sc->bdl = (struct ac97_bdle *)sc->bdl_dma.db_vaddr;
memset(sc->bdl, 0, sizeof(struct ac97_bdle) * AC97_BDL_ENTRIES);

/* Allocate two PCM ping-pong buffers */
for (i = 0; i < 2; i++) {
    dma_alloc(AC97_BUF_BYTES, 4, &sc->buf_dma[i]);
    sc->buf[i] = (uint8_t *)sc->buf_dma[i].db_vaddr;
    memset(sc->buf[i], 0, AC97_BUF_BYTES);  /* silence */

    sc->bdl[i].addr  = sc->buf_dma[i].db_paddr;
    sc->bdl[i].len   = AC97_BUF_SAMPLES;    /* samples, not bytes */
    sc->bdl[i].flags = (1 << 15);           /* IOC — interrupt on completion */
}

sc->lvi      = 1;    /* two entries valid: indices 0 and 1 */
sc->next_buf = 0;

/* Point hardware at BDL */
outl(sc->nabm_base + 0x10 + 0x00, sc->bdl_dma.db_paddr);  /* BDL_ADDR */
outb(sc->nabm_base + 0x10 + 0x05, sc->lvi);                 /* LVI */

/* Clear any stale status bits */
outw(sc->nabm_base + 0x10 + 0x06, 0x001E);

/* Enable IOC interrupt and start DMA */
outb(sc->nabm_base + 0x10 + 0x0B, (1 << 4) | (1 << 0));   /* IOCE | RPBM */
```

---

## Phase 4 — IRQ Handler + Ring Buffer

**`ac97_isr(void)`:**

Called when a BDL entry with `IOC` set completes.

```c
static void
ac97_isr(void)
{
    struct ac97_softc *sc = ac97_global_sc;
    uint16_t sr;
    uint8_t  fill_buf;
    uint32_t avail, i;

    sr = inw(sc->nabm_base + 0x10 + 0x06);  /* read SR */
    if (!(sr & (1 << 2))) {
        irqEOI(sc->irq);
        return;   /* not our interrupt (shared IRQ) */
    }
    outw(sc->nabm_base + 0x10 + 0x06, (1 << 2));  /* clear BCIS */

    /* Determine which buffer to fill next */
    fill_buf = sc->next_buf;
    sc->next_buf ^= 1;

    /* Drain ring buffer into the PCM buffer */
    avail = (sc->ring_wr - sc->ring_rd) & (sizeof(sc->ring) - 1);
    if (avail >= AC97_BUF_BYTES) {
        /* Enough data — copy and advance read pointer */
        for (i = 0; i < AC97_BUF_BYTES; i++)
            sc->buf[fill_buf][i] =
                sc->ring[(sc->ring_rd + i) & (sizeof(sc->ring) - 1)];
        sc->ring_rd = (sc->ring_rd + AC97_BUF_BYTES) & (sizeof(sc->ring) - 1);
    } else {
        /* Underrun — fill with silence */
        memset(sc->buf[fill_buf], 0, AC97_BUF_BYTES);
    }

    /* Advance LVI to include the refilled buffer */
    sc->lvi = (sc->lvi + 1) & 31;
    sc->bdl[sc->lvi].addr  = sc->buf_dma[fill_buf].db_paddr;
    sc->bdl[sc->lvi].len   = AC97_BUF_SAMPLES;
    sc->bdl[sc->lvi].flags = (1 << 15);        /* IOC */
    outb(sc->nabm_base + 0x10 + 0x05, sc->lvi); /* update LVI */

    irqEOI(sc->irq);
}
```

**Ring buffer write (called from sys_write path, Phase 5):**

```c
int
ac97_ring_write(const uint8_t *src, uint32_t len)
{
    struct ac97_softc *sc = ac97_global_sc;
    uint32_t free, i;

    free = sizeof(sc->ring) -
           ((sc->ring_wr - sc->ring_rd) & (sizeof(sc->ring) - 1));
    if (len > free)
        len = free;   /* drop excess rather than block for now */

    for (i = 0; i < len; i++)
        sc->ring[(sc->ring_wr + i) & (sizeof(sc->ring) - 1)] = src[i];
    sc->ring_wr = (sc->ring_wr + len) & (sizeof(sc->ring) - 1);
    return (int)len;
}
```

> **Note**: the ring buffer is written from process context and read from IRQ
> context.  The indices are 32-bit; individual 32-bit reads/writes are atomic
> on x86.  No spinlock needed for the pointer update as long as each side only
> advances its own pointer.

---

## Phase 5 — `/dev/audio` VFS Node

**Files:**
- `sys/devfs/audio.c` (new) — devfs char device glue
- `sys/include/devfs/audio.h` (new)
- `sys/fs/vfs/vfs_calls.c` — route `sys_write` for the audio fd

**Approach:**

Register a devfs node `audio` backed by two operations:

```c
int  devfs_audio_write(const char *buf, int len);  /* calls ac97_ring_write */
int  devfs_audio_ioctl(int cmd, void *arg);         /* set rate, format */
```

`devfs_audio_write` copies from userland and forwards to `ac97_ring_write`.
No blocking in Phase 5 — if the ring is full, bytes are dropped.

**ioctl commands (Phase 5 minimum):**

```c
#define AUDIO_SET_RATE   0x4101   /* arg = uint32_t * sample_rate */
#define AUDIO_GET_RATE   0x4102
```

---

## Phase 6 — Userland `lib/libaudio/`

**Files:**
- `lib/libaudio/audio.c`
- `include/audio/audio.h`
- `lib/libaudio/Makefile`

**Header:**

```c
#ifndef _AUDIO_H
#define _AUDIO_H

int  audio_open(const char *dev);               /* open("/dev/audio", O_WRONLY) */
int  audio_set_rate(int fd, uint32_t rate);     /* ioctl AUDIO_SET_RATE */
int  audio_write(int fd, const void *buf, int n); /* write() */
void audio_close(int fd);                       /* close() */

#endif
```

**`audio_open`:**

```c
int
audio_open(const char *dev)
{
    return open(dev, 1 /* O_WRONLY */);
}
```

**`audio_set_rate`:**

```c
int
audio_set_rate(int fd, uint32_t rate)
{
    return ioctl(fd, AUDIO_SET_RATE, &rate);
}
```

---

## Phase 7 — Test Tone App `bin/aplay/`

A minimal app to verify the pipeline with a 440 Hz sine approximated by a
square wave (avoids needing `libm`):

```c
/* bin/aplay/aplay.c */
#include <audio/audio.h>
#include <stdint.h>

#define RATE      48000
#define FREQ      440
#define DURATION  3    /* seconds */

int
main(void)
{
    int16_t buf[RATE / FREQ * 2];  /* one cycle, stereo */
    int     fd, i, s, half;

    fd   = audio_open("sys:/dev/audio");
    if (fd < 0) return 1;
    audio_set_rate(fd, RATE);

    half = RATE / FREQ / 2;
    for (i = 0; i < (int)(sizeof(buf) / 4); i++) {
        int16_t v = (i < half) ? 8000 : -8000;
        buf[i * 2    ] = v;   /* left */
        buf[i * 2 + 1] = v;   /* right */
    }

    for (s = 0; s < RATE * DURATION / (sizeof(buf) / 4); s++)
        audio_write(fd, buf, sizeof(buf));

    audio_close(fd);
    return 0;
}
```

---

## File Map

```
sys/pci/
    ac97.c                  — probe, attach, ac97_isr, ring-buffer writer
sys/include/pci/
    ac97.h                  — ac97_softc, ac97_bdle, ac97_ring_write() proto
sys/devfs/
    audio.c                 — devfs char node for /dev/audio
sys/include/devfs/
    audio.h                 — AUDIO_SET_RATE / AUDIO_GET_RATE ioctls
lib/libaudio/
    audio.c                 — audio_open / audio_write / audio_set_rate / audio_close
    Makefile
include/audio/
    audio.h                 — public API header
bin/aplay/
    aplay.c                 — 440 Hz square-wave test
    Makefile
```

---

## Build Integration

**`sys/pci/pci.c`** — add to driver table:

```c
extern struct ubx_driver ac97_ubx_driver;
...
static struct ubx_driver *const pci_drv_table[] = {
    &e1000_ubx_driver,
    &ide_ubx_driver,
    &lnc_ubx_driver,
    &uhci_ubx_driver,
    &ac97_ubx_driver,   /* ← add */
    NULL,
};
```

**`sys/pci/Makefile`** — add `ac97.c` to `SRCS`.

**`lib/Makefile`** — add `libaudio` to the library build list.

**`bin/Makefile`** — add `aplay` to the binary build list.

---

## QEMU Test Invocation

```sh
bmake run-debug   # serial.log; add AC97 flags in Makefile:
```

In the root `Makefile` `QEMU_FLAGS` or `run` target, append:

```makefile
QEMU_SND = -audiodev coreaudio,id=snd0 -device AC97,audiodev=snd0
```

Then `bmake run` and run `aplay` from the shell prompt.

---

## Known Constraints and Pitfalls

**Sample count vs byte count in BDL.**  The `len` field in `ac97_bdle` is in
*samples*, not bytes.  For stereo 16-bit PCM each sample is 4 bytes.  A
buffer of `AC97_BUF_BYTES = 8192` bytes → `len = 2048`.  Getting this wrong
produces fast or silent playback with no obvious error.

**BDL physical alignment.**  QEMU accepts 4-byte aligned BDL entries.  Real
AC'97 hardware requires the BDL array itself to be 8-byte aligned — use
`dma_alloc(..., 8, ...)` so this is satisfied on both.

**PCM buffer alignment.**  The DMA buffer must be 4-byte aligned;
`dma_alloc(..., 4, ...)` satisfies this.

**LVI wraparound.**  The LVI register is 5 bits wide (0–31).  After entry 31
wrap to 0.  The ring index mask `& 31` handles this.

**Cold reset timing.**  The codec ready flag (GLOB_STA bit 8) may take up to
150 ms on real hardware.  QEMU sets it immediately.  A polled loop with
`sched_yield()` is the right approach — do not busy-spin in a real kernel
tick loop.

**Variable-Rate Audio (VRA).**  The standard AC'97 codec resets to 48 kHz.
To support other rates you must check the Extended Audio ID register
(NAM 0x28) for the VRA capability bit before writing to the DAC Rate register
(NAM 0x2A).  QEMU's AC'97 supports VRA; write the desired rate directly.

**Shared IRQ.**  PCI IRQs are shared on the QEMU i440fx machine.  The ISR
must check the BCIS bit in SR before acting and call `irqEOI` regardless.
`ubx_alloc_irq` uses the shared `irq_register` path which calls all handlers
for that IRQ line.

**Ring buffer and IRQ atomicity.**  The single-producer/single-consumer ring
with separate read/write indices is safe on a single-core x86 without a
spinlock, because the kernel has no SMP and the index writes are naturally
32-bit aligned.  If SMP is ever enabled, add a `spinLock`.
