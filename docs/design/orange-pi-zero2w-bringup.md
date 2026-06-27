# Orange Pi Zero 2W (Allwinner H618) Bring-Up Plan

**Status:** drafted 2026-06-26. First real-hardware aarch64 target (the QEMU `virt`
port is the reference). Console-first, milestone-driven; each milestone is
flashed to a microSD and observed over a USB-UART serial console.

## The board

- **SoC:** Allwinner **H618** — 4× Cortex-A53, GIC-400 (**GICv2**), ARM generic
  timer, **PSCI** via ATF/BL31. 1 GB LPDDR4 (this unit).
- **Boot:** BootROM → SPL (DRAM init, on the eMMC/SD) → **U-Boot** → loads our
  kernel + DTB, enters at EL1/EL2 with **x0 = DTB pointer**. We do **not** init
  DRAM (the SPL does). PSCI is already up (ATF).
- **Console:** UART0 — a **dw-apb (16550-compatible)** UART, *not* PL011. U-Boot
  leaves it muxed + clocked, so M0 can write it without touching CCU/PIO.

## Verified H618 specs (2026-06-27, deep-research, primary-sourced)

From mainline `sun50i-h616.dtsi` (the H618 board DTS includes it), U-Boot
`README.sunxi64`, and the arm64 boot protocol — all high-confidence:

| Item | Value |
|------|-------|
| UART0 | `0x05000000`, dw-apb/16550, 32-bit regs (reg-shift=2): **THR @ 0x00, LSR @ 0x14, TX-ready = LSR bit 5 (0x20)** |
| GIC-400 (GICv2) | GICD `0x03021000`, GICC `0x03022000` (GICH `0x03024000`, GICV `0x03026000`) |
| Generic timer | EL1 virtual timer PPI/INTID **27**, CNTFRQ ~24 MHz (same as QEMU virt) |
| RAM | base `0x40000000`, 1 GB (**same base as QEMU virt** → kernel `0x40200000` link works) |
| MMC0 (microSD) | ~`0x04020000` (sunxi-mmc) — for M4 |
| CCU / PIO | CCU `0x03001000`, PIO `0x0300B000` — for M4 clock/pinmux |
| U-Boot hand-off | **EL2**, flat arm64 `Image` via **`booti`**, **DTB phys in x0**, 2 MB-aligned load |

Net: GIC/timer/RAM transfer (new addresses only); the genuinely-new bits are the
dw-apb UART and the Image/`booti` boot format.

## What transfers from the QEMU `virt` port (big reuse)

| Piece | Reuse |
|---|---|
| All MI kernel + userland (sched, VFS, FAT/UbixFS, lwIP, MPI, views, musl world) | as-is |
| **GICv2** driver (`gic.c`) | reuse — only base addresses change (GIC-400) |
| **ARM generic timer** (`timer.c`, per-CPU `CNTV`) | reuse — `CNTFRQ` read at runtime |
| **PSCI `CPU_ON` SMP** (`apsmp.c`) | reuse — ATF provides PSCI on HW |
| **DTB `/memory` parse** | reuse — already runtime |
| COW/pmap/exceptions | reuse |

## New machine-dependent drivers (the actual work)

1. **dw-apb 16550 UART** — serial console (`kputc`/`kgetc`). *M0 — the lifeline.*
2. **sunxi SD/MMC** (`sunxi-mmc`) — microSD root storage. *The big one.*
3. **CCU** (clock control unit) — gate/configure peripheral clocks (MMC, later DE/USB).
4. **PIO** (pin controller) — pin-mux MMC/USB/etc. (UART already done by U-Boot).
5. *Later:* **DE2 + HDMI/TCON** display (complex — console-first defers it),
   **EHCI/OHCI USB**, WiFi.

## Architecture decision: DTB-driven, one tree

Rather than a compile-time "board target" with hardcoded addresses, **read
peripheral base addresses from the DTB** (the kernel already parses `/memory`).
Match `compatible` strings → bind the right MD driver → at the node's `reg`
address. One kernel boots QEMU `virt` *and* the H618; matches the "one tree,
profile-driven" identity and avoids hardcoding (possibly-wrong) addresses.
Pragmatic first pass: read the few addresses we need from the DTB, bind drivers
explicitly; grow to full `compatible`-matching later.

## Milestones (each = flash SD, watch serial)

- **M0 — Serial hello. ✅ BUILT (2026-06-27), awaiting on-hardware test.**
  Standalone bare-metal Image: `sys/arch/aarch64/board/opizero2w/{start.S,hello.c,
  m0.ld}` → `tools/build-opi-hello.sh` → `build/opizero2w/opi-zero2w-m0.bin` (valid
  arm64 Image header verified). Emits the Image header, sets a stack, prints a
  banner over UART0 @ 0x05000000, halts (runs at EL2 — no EL2→EL1 drop yet; that's
  M1). **Flash/test:** boot a microSD to a U-Boot prompt over serial (115200 8N1),
  then `fatload mmc 0:1 0x40200000 opi-zero2w-m0.bin; booti 0x40200000`. Proves:
  boot chain, load address, UART driver. *Smallest, most important step.*
- **M1 — Interrupts + timer.** GIC-400 (GICv2) + generic timer at H618 addresses
  (from DTB); 100 Hz tick; EL0/EL1 split. Reuses `gic.c`/`timer.c`.
- **M2 — DTB peripherals.** Generalise: discover UART/GIC/timer (and later MMC)
  from the DTB so the same kernel runs on both targets.
- **M3 — SMP.** PSCI `CPU_ON` the 3 secondary A53s (reuse `apsmp.c`); per-CPU
  timers; the scheduler we just verified under `-smp 4`.
- **M4 — Root storage.** *Shortcut first:* boot userland from an **embedded
  initramfs/ramfs** (no MMC needed) to reach a shell fast. *Then* write the
  **sunxi-mmc** driver (+ CCU/PIO) for a real microSD root.
  - **Make the MMC driver IRQ-driven from the start** (sleep on the transfer-
    complete IRQ via `sched_wait_event` → `sched_wakeup_chan`, like the net
    driver). Unlike QEMU virtio-blk (instant, where bounded spin-then-yield is
    fine), a real microSD read takes **milliseconds** — polling/yield-spinning
    that long ties up the scheduler, so blocking-on-IRQ is the correct design
    here and is where the "load files without tying up the scheduler" win lands.
- **M5 — Userland over serial.** `init` → shell on the serial console (no display
  yet) — a usable headless uBixOS on real hardware.
- **M6+ — Display / USB / WiFi.** DE2+HDMI for the graphical desktop; USB host;
  WiFi. Console-first means these are additive, not blockers.

## Test workflow (needed before M0)

1. **USB-UART adapter** on the board's debug UART (3.3 V) → `screen`/`minicom` on
   the Mac for serial output. **Without this, bring-up is blind.**
   - **The debug console is on the 40-pin GPIO header, NOT the USB-C** (USB-C is
     power + USB 2.0 OTG only). UART0 = **pin 8 (TX), pin 10 (RX)**, ground on
     **pin 6**. Wire a **3.3 V** USB-TTL dongle crossed: dongle RX→pin 8,
     dongle TX→pin 10, dongle GND→pin 6, **115200 8N1**. (A 5 V adapter can
     damage the 3.3 V SoC pins.) This matches UART0 @ `0x05000000` (M0).
2. **microSD flashing:** U-Boot (mainline or the vendor's) + a boot script
   (`boot.scr`/extlinux) that loads our kernel image + DTB and `booti`s it.
3. A `bmake` target to build the H618 kernel + assemble the SD boot files.

## Open decisions (confirm before M0)

- **Addresses:** DTB-driven (recommended) vs. a hardcoded board target.
- **Storage:** initramfs-first (recommended — defers the MMC driver) vs. MMC-first.
- **U-Boot:** use the board's stock vendor U-Boot, or mainline? (Either provides
  PSCI + loads `booti` images.)
- **Test rig:** confirm you have the USB-UART adapter + can flash a microSD.
