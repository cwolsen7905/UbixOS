# Raspberry Pi 3 Model B v1.2 (BCM2837) Bring-Up Plan

**Status:** drafted 2026-06-27. **Primary first real-hardware aarch64 target** —
the board is in hand and is the best-documented bare-metal arm64 platform there
is. Console-first, milestone-driven; each milestone is flashed to a microSD and
observed over a USB-UART serial console. (Sibling of
`orange-pi-zero2w-bringup.md`; the OPi/H618 stays a secondary target.)

## The board

- **SoC:** Broadcom **BCM2837** — 4× Cortex-A53 (aarch64), 1.2 GHz. **1 GB
  LPDDR2, base physical `0x00000000`** (top `0x40000000`).
- **Boot:** the **VideoCore GPU firmware** boots, reads the FAT boot partition
  (`bootcode.bin` → `start.elf`), and loads **`kernel8.img` as a raw flat binary
  at `0x80000`**, entering **core 0 at EL2** with **x0 = DTB physical address**
  (x1–x3 = 0); cores 1–3 are held in a firmware spin-table. **No U-Boot, no arm64
  Image header** — just a flat `objcopy` binary. Much simpler than the OPi's
  `booti` path.
- **Console:** **PL011 (UART0) @ `0x3F201000`** — *the same PL011 uBixOS already
  drives on QEMU virt*. By default it's wired to Bluetooth; `dtoverlay=disable-bt`
  in `config.txt` frees it to GPIO14/15 (header **pins 8/10**). (The mini-UART
  (UART1) @ `0x3F215040` is the other option but its baud is tied to the core
  clock — PL011 is cleaner *and* reuses our driver.)

## Verified specs (BCM2837, peripheral base `0x3F000000`)

| Item | Value |
|------|-------|
| Peripheral base | `0x3F000000` (Pi 2/3; Pi 1 was `0x20000000`, Pi 4 `0xFE000000`) |
| **PL011 UART0** | `0x3F201000` — **reuses `uart.c`** (just a new base) |
| mini-UART (AUX) | `0x3F215040` (alt console; baud tied to core clock) |
| Legacy IRQ controller | `0x3F00B200` (ARMC — peripheral IRQ enable/pending) |
| Per-core local IRQ/timer | `0x40000000` (BCM2836 "ARM local" — routes the generic-timer IRQ + mailboxes/IPIs) |
| System timer (BCM) | `0x3F003000` (free-running 1 MHz; alt to the ARM generic timer) |
| GPU mailbox | `0x3F00B880` (property interface → framebuffer, board info) |
| EMMC / SD (sdhci) | `0x3F300000` — microSD, for M4 |
| RAM | base **`0x00000000`**, 1 GB |
| Boot hand-off | firmware → `kernel8.img` flat @ `0x80000`, **EL2**, **x0 = DTB**, cores 1–3 in spin-table |

## What reuses vs. what's new (honest)

| Piece | Pi 3 |
|---|---|
| All MI kernel + userland (sched, VFS, FAT/UbixFS, lwIP, MPI, views, musl world) | reuse |
| **PL011 UART** (`uart.c`) | **reuse** — new base `0x3F201000` (a real win vs the OPi's new dw-apb) |
| ARM generic timer **registers** (`timer.c`) | reuse — but the **IRQ is delivered by the BCM local controller**, not a GIC |
| **Interrupt controller** | **NEW** — BCM2837 legacy peripheral IC (`0x3F00B200`) + per-core local IC (`0x40000000`). **No GIC**, so `gic.c` does *not* transfer. The biggest M1 task. |
| **SMP secondary start** | **NEW** — firmware **spin-table** (write the core's entry to its release address + `sev`), not PSCI `CPU_ON`. (Simpler than PSCI, but different from `apsmp.c`.) |
| **RAM at `0x0`** | the kernel currently links at `0x40200000` (RAM-at-1GB). On the Pi 3 that address is *past* the 1 GB top → the kernel must link low **or** ride the in-progress **higher-half migration** (kernel at a high TTBR1 VA + a physmap whose base = the board's RAM base; `0x0` for Pi vs `0x40000000` for H618/QEMU). Higher-half makes this a config, not a fork. |
| Boot format | GPU firmware loads a raw flat `kernel8.img` (no U-Boot, no Image header) — **simplest of all targets** |

Net: simplest boot + PL011 reuse + best docs + in-hand. Costs: a from-scratch BCM
interrupt controller, spin-table SMP, and the RAM-at-`0x0` link/physmap question.

## SD boot setup (the FAT boot partition)

A microSD with a FAT32 partition holding the Raspberry Pi firmware + our kernel:
- `bootcode.bin`, `start.elf`, `fixup.dat` (from raspberrypi/firmware),
- `bcm2837-rpi-3-b.dtb` (the board DTB, from the firmware/linux tree),
- `config.txt`:
  ```
  arm_64bit=1
  enable_uart=1
  dtoverlay=disable-bt      # frees PL011 to GPIO14/15 (pins 8/10)
  kernel=kernel8.img
  ```
- `kernel8.img` (our flat kernel). Power on → it runs. (No U-Boot step.)

## Serial console wiring

PL011 on the 40-pin GPIO header (USB ports are USB/data, not serial):
- **pin 8 = TXD (GPIO14)**, **pin 10 = RXD (GPIO15)**, **pin 6 = GND**.
- **3.3 V** USB-TTL dongle, crossed: dongle RX→pin 8, dongle TX→pin 10, GND→pin 6,
  **115200 8N1**. (5 V adapters can damage the SoC.)

## Milestones (each = flash SD, watch serial)

- **M0 — Serial hello. ✅ CONFIRMED ON REAL HARDWARE (2026-06-27).** `kernel8.img`
  linked at `0x80000`, MPIDR-gated to core 0, loops a banner over PL011 @
  `0x3F201000` (runs at EL2). **Key gotcha (learned the hard way):** the firmware
  — even with `enable_uart=1` + `dtoverlay=disable-bt` — does **not** reliably
  leave the PL011 configured/routed for a bare-metal kernel, so M0 **initializes
  it itself**: mux GPIO14/15 → ALT0, clear pulls, set 115200 8N1 @ 48 MHz
  (`init_uart_clock=48000000` pins the clock), enable TX/RX. Trusting the firmware
  showed nothing; self-init worked. Build: `tools/build-rpi3-hello.sh` +
  `tools/fetch-rpi3-firmware.sh` + `tools/make-rpi3-sd.sh` → `build/rpi3b/
  rpi3-sd.img`, dd to microSD. Serial on header pins 8(TX)/10(RX)/6(GND), 3.3 V,
  115200. **This is the first uBixOS code to run on real hardware.**
- **M1 — Full kernel to serial.** EL2→EL1 drop; the **BCM2837 interrupt
  controller** (peripheral IC + per-core local IC) + the ARM generic timer (100 Hz
  tick); the RAM-at-`0x0` link/physmap; PL011 as the real console. The substantial
  milestone (new IRQ controller).
- **M2 — DTB.** Parse the firmware-supplied DTB (memory size, the peripheral base
  — so the same kernel can target Pi 3 vs QEMU vs H618 by DTB).
- **M3 — SMP.** Start cores 1–3 via the **spin-table** release addresses; per-core
  timers + IPIs via the local controller; the verified scheduler.
- **M4 — Root storage.** *Shortcut first:* embedded initramfs to a shell fast.
  *Then* the **sdhci/EMMC** driver (`0x3F300000`) — **IRQ-driven** (real SD reads
  take ms; sleep-on-IRQ, don't poll — see the disk-I/O note).
- **M5 — Userland over serial.** `init` → shell on the serial console = a usable
  headless uBixOS on real hardware.
- **M6 — Display.** GPU **mailbox** (`0x3F00B880`) property interface to allocate a
  framebuffer, then `views`. (The Pi's GPU owns the display; the ARM asks for an FB
  over the mailbox — different from virtio-gpu but well-documented.)

## Test workflow (needed before M0)

1. **3.3 V USB-UART dongle** on header pins 8/10/6 → `screen /dev/tty.usbserial* 115200`.
2. A microSD with the firmware files + `config.txt` + DTB (above).
3. A `tools/build-rpi3-hello.sh` that builds `kernel8.img` (M0).

## Why the Pi 3 is the better first target

In-hand, the **best-documented** bare-metal arm64 platform (osdev wiki, bztsrc
raspi3-tutorial, s-matyukevich raspberry-pi-os), the **simplest boot** (firmware
loads a flat binary — no U-Boot, no Image header), and it **reuses our PL011
driver**. The cost (custom BCM interrupt controller, spin-table SMP) is well-trodden
ground with abundant reference code. The OPi/H618 remains the secondary target.
