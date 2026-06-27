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
- **M0.5 — EL drop + DTB + memory map. ✅ CONFIRMED ON REAL HARDWARE (2026-06-27).**
  Standalone, extends M0: gates to core 0, drops **EL2→EL1** (verified on HW: entry
  EL `0x2` → now EL `0x1`), parses the firmware DTB (passed in x0 @ `0x2eff7400`,
  valid, ~35 KB) and prints `/memory` — **real Pi 3 RAM: base `0x0`, size
  `0x3b400000` (992 MB = 1 GB − 32 MB GPU split)**. Nails M1's three unknowns (EL
  drop, DTB/FDT parse, the RAM-at-`0x0` layout) in isolation before the full kernel.
- **M0.75 — Timer interrupt via the BCM controller. ✅ CONFIRMED ON REAL HARDWARE
  (2026-06-27).** Standalone, extends M0.5: an EL1 vector table (`vectors.S`,
  VBAR_EL1, 2 KB-aligned), the **BCM2837 "ARM local" interrupt controller**
  (`0x40000000`) routing the EL1 physical-timer IRQ (CNTPNSIRQ) to core 0, and the
  architected timer re-arming in the handler → ticks roll on hardware. `start.S`
  also sets `CNTHCTL_EL2` so EL1 can use the physical timer. Retires M1's
  highest-risk unknown — the interrupt controller (`gic.c` does NOT transfer) — and
  it's the engine the scheduler runs on.
- **M1 — Full kernel to serial.** EL2→EL1 drop, the BCM interrupt controller, and
  the architected timer are all ✅ **proven standalone** (M0.5/M0.75). The remaining
  work is **integrating the real kernel**: the RAM-at-`0x0` link/physmap (build on
  the in-progress higher-half migration — parameterize the physmap base) + a Pi
  board target (port the proven M0.75 device code: PL011 console, BCM IRQ
  controller, timer) + PL011 as the kernel console. **Gated on the higher-half
  work landing.**
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

## M1 implementation plan (board abstraction)

From mapping the kernel's aarch64 MD layer (2026-06-27): the **higher-half migration
is already in the tree** — the kernel runs at a high VA and reaches physical memory
through a TTBR1 physmap (`PHYSMAP_BASE + phys`), with devices at `PHYSMAP_BASE +
<hardcoded base>`. That's the foundation. The kernel is otherwise **monolithic /
QEMU-hardcoded**; the Pi needs a board abstraction.

### Blockers (all currently hardcoded for QEMU `virt`)
- `sys/compile/ldscript.aarch64`: `KERNEL_PHYS = 0x40200000` — past the Pi's 1 GB
  top; the Pi kernel must link low (firmware loads at `0x80000`).
- `sys/arch/aarch64/vmm/vmm_machdep.c`: `AARCH64_RAM_BASE = 0x40000000` — Pi RAM
  base is `0x0`; `g_ram_top` derives from it.
- `sys/arch/aarch64/vmm/mmu.c`: physmap maps block 0 (0–1 GB) as **Device**. The Pi
  has **RAM `0x0–0x3F000000` *and* the peripheral window `0x3F000000`+ in that same
  1 GB block** → block 0 needs 2 MB granularity (RAM Normal-cacheable, peripherals
  Device).
- Device bases hardcoded: PL011 `0x09000000` (`uart.c`), GIC `0x08000000/0x08010000`
  (`gic.c`), timer = CNTV via GIC INTID 27 (`timer.c`), virtio `0x0A000000`.
- Kernel entry (`kern/start.S`) assumes **EL1 entry**; the Pi firmware enters at EL2
  (M0.5 already has the drop to port).

### The abstraction — `struct aarch64_board`
Selected early in `boot.c` by DTB `/compatible` (`raspberrypi,3-model-b` /
`brcm,bcm2837` vs QEMU `linux,dummy-virt` vs the H618 string). Holds:
- **identity:** name, compatible[].
- **memory:** `ram_base`, `kernel_phys` (LMA), the peripheral window (base/size, for
  the 2 MB Device mapping).
- **console:** `uart_base` + `init/putc/getc` (port M0.75's self-init PL011).
- **intc:** `init / enable_intid / dispatch (ack→route→eoi) / send_resched` — port
  M0.75's BCM "ARM local" controller as `bcm_intc.c`; QEMU keeps `gic.c`.
- **timer:** `init` + kind/routing — Pi = CNTP via the local controller (M0.75);
  QEMU = CNTV via the GIC (INTID 27).

Two instances now (`board_qemu_virt`, `board_rpi3`), H618 a third later.

### Sequencing (keep QEMU **and** x86_64 green at every step)
1. **Introduce `struct aarch64_board`; refactor the QEMU path behind it** — pure
   no-behavior-change refactor; verify QEMU still boots (the silent-breakage risk).
2. **Per-board `KERNEL_PHYS`/`RAM_BASE`** — parameterize the linker script +
   `vmm_machdep` (a board/build var; Pi → `0x80000`, RAM `0x0`).
3. **Pi physmap** — 2 MB-granular block 0 (RAM Normal up to the peripheral window,
   Device above), board-driven.
4. **EL2→EL1 drop in the kernel entry** (port M0.5), conditional on the entry EL.
5. **`board_rpi3`** — PL011 console + `bcm_intc` + CNTP-via-local-ctrl timer (all
   ported from the confirmed M0.75 code) → **kernel boots to the serial console.**
6. Then **M2** (DTB-driven), **M4** (sdhci + a ubpool partition for `/` — extend
   `make-rpi3-sd.sh` to mirror `mkimage.sh`'s FAT-boot + ubpool layout), **M5**
   (init → shell on serial).

### ⚠️ Coordination gate
Steps 1–4 edit the **same** aarch64 `vmm/kern` files the **higher-half lane has
uncommitted** right now. Per AGENTS-COORD Rule 3, those can't be refactored while
they're another lane's live WIP. **M1 execution is gated on the higher-half work
being committed.** Until then, the new Pi board drivers (`bcm_intc.c`, the PL011
console, the timer) can be written as *new* files without conflict, but the wiring
(steps 1–4) waits. The confirmed M0.75 standalone code is the reference for all of
the device pieces.
