# Raspberry Pi 4 / Pi 400 (BCM2711) bring-up — sketch

**Status: PLANNED (not started).** The Pi 3B port (`BOARD_RPI3`) is the reference;
this reuses the same arch (aarch64) + board abstraction (`struct aarch64_board`,
`struct aarch64_intc`) but is a **new board** (`BOARD_RPI4`) — the BCM2711 differs
from the BCM2837 in the peripheral map and in two whole subsystems (USB, Ethernet).

## What's the same (reuse from BOARD_RPI3)
- aarch64 kernel, higher-half physmap, MMU, the `struct aarch64_board`/`aarch64_intc`
  abstraction, VideoCore mailbox + framebuffer, EMMC/SDHCI (just different base),
  the ARCH+BOARD build layout (`build/aarch64/boards/rpi4/`).
- Boot shape: firmware loads `kernel8.img` raw at `0x80000`, core 0 at EL2, `x0`=DTB
  (`bcm2711-rpi-4-b.dtb`). Same EL2→EL1 drop.

## What's different (the work)
| Subsystem | BCM2837 (Pi 3) | BCM2711 (Pi 4/400) | Effort |
|---|---|---|---|
| **Peripheral base** | `0x3F000000` | **`0xFE000000`** | trivial — new `aarch64_board` bases (UART `0xFE201000`, EMMC `0xFE340000`, GPIO `0xFE200000`, mailbox `0xFE00B880`) |
| **Interrupt ctrl** | BCM local (no GIC) | **GIC-400 (GICv2)** @ `0xFF841000/0xFF842000` | *easier* — reuse `g_gicv2_intc`; wire the Pi 4 GIC bases |
| **Per-core timer/IPI** | BCM local `0x40000000` | GIC SGIs + generic timer | closer to QEMU; SMP via GIC + spin-table/PSCI |
| **USB** | DWC2 (done) | **xHCI via VL805 (PCIe)** + DWC2 on USB-C | **NEW: PCIe bring-up + xHCI host driver** — the big one |
| **Ethernet** | USB LAN9514 (done) | **GENET** (mmapped GbE MAC + MDIO PHY) | **NEW: GENET driver** (memory-mapped, not USB) |
| **Boot device** | SD firmware | SPI EEPROM (SD / **USB** / TFTP) | boots off USB with no SD (see below) |

## Milestones
- **R4.0 — Serial hello.** `BOARD_RPI4` board struct (peri base `0xFE000000`, PL011
  `0xFE201000`), reuse `uart.c`. First banner over serial. (Config: `arm_64bit=1`,
  `enable_uart=1`, `bcm2711` DTB.)
- **R4.1 — GIC + timer.** Wire `g_gicv2_intc` to the Pi 4 GIC-400 bases; timer IRQ
  through the GIC (much like QEMU). IRQs + preemption.
- **R4.2 — DTB RAM + physmap.** BCM2711 memory map (up to 4/8 GB; high peripheral
  window). Reuse the DTB `/memory` parse; adjust the 2 MB physmap block scheme for
  the `0xFE000000` peripheral window.
- **R4.3 — EMMC + pool + world.** SDHCI at `0xFE340000` (the Pi 4 has EMMC2); mount
  the UbixFS pool → full world off SD. (Mirrors Pi 3 M4.)
- **R4.4 — Framebuffer.** VideoCore mailbox FB (same as Pi 3 M6, different mailbox
  base) → views desktop.
- **R4.5 — GENET Ethernet.** The memory-mapped gigabit MAC + MDIO PHY → the lwIP
  netif bridge (same `send()/poll_rx()/mac/ready` interface as SMSC). No USB needed.
- **R4.6 — PCIe + xHCI (USB).** Bring up the BCM2711 PCIe root complex, enumerate the
  VL805, then an **xHCI** host driver → HID (built-in keyboard) + mass storage. This
  is the largest piece — a new host-controller stack (rings, TRBs, contexts), reusing
  our USB enumeration/HID class logic on top.
- **R4.7 — SMP.** 4× Cortex-A72 via GIC SGIs + spin-table/PSCI.

## Boot from USB (no SD)
The Pi 4/400 EEPROM bootloader supports **USB mass-storage boot out of the box** (no
OTP fuse dance like the Pi 3). Put the FAT boot partition (firmware + `kernel8.img`) +
the UbixFS pool on a USB drive; set the EEPROM `BOOT_ORDER` to include USB. **Caveat:**
the *firmware* boots off USB fine, but *our OS* using USB devices (the keyboard, a USB
NIC) still needs the **xHCI driver (R4.6)**. For a headless GENET-networked server
(R4.5), no USB driver is needed at all.

## Priority note
`BOARD_RPI4` is a real port, gated behind the two new drivers (xHCI, GENET). Recommend
finishing the Pi 3B (HID + dropbear) first; the Pi 4 GIC makes interrupts easier but
xHCI + GENET are the cost of entry.
