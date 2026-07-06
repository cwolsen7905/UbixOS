#!/bin/sh
# Fetch the Raspberry Pi 4 / Pi 400 (BCM2711) VideoCore GPU firmware (proprietary
# Broadcom blobs — fetched at use time, never vendored) + the board DTB, and write
# config.txt into the SD boot/ directory.  Unlike the Pi 3, the Pi 4 has NO
# bootcode.bin on the SD (its first-stage bootloader lives in the SPI EEPROM); it
# loads start4.elf / fixup4.dat directly.  See docs/design/raspberry-pi-4-400-bringup.md.
#
# Usage:  sh tools/build-rpi4-kernel.sh   # build kernel8.img first
#         sh tools/fetch-rpi4-firmware.sh
set -e

BOOT=build/aarch64/boards/rpi4/boot # ARCH + BOARD layout
RAW=https://github.com/raspberrypi/firmware/raw/master/boot
mkdir -p "$BOOT"

for f in start4.elf fixup4.dat bcm2711-rpi-4-b.dtb bcm2711-rpi-400.dtb; do
	echo "fetching $f ..."
	curl -fsSL "$RAW/$f" -o "$BOOT/$f"
done

# disable-bt overlay: REQUIRED for dtoverlay=disable-bt to route the PL011 (uart0,
# our driver) onto GPIO14/15.  Without it the mini-UART owns the header pins and our
# PL011 output goes to the (absent) Bluetooth — nothing on serial.
mkdir -p "$BOOT/overlays"
echo "fetching overlays/disable-bt.dtbo ..."
curl -fsSL "$RAW/overlays/disable-bt.dtbo" -o "$BOOT/overlays/disable-bt.dtbo"

cat > "$BOOT/config.txt" <<EOF
# uBixOS Raspberry Pi 4 / Pi 400 boot config (see raspberry-pi-4-400-bringup.md)
arm_64bit=1
enable_uart=1
init_uart_clock=48000000   # UARTCLK so IBRD/FBRD (26/3) = 115200 on the PL011
enable_gic=1               # boot with the GIC-400 enabled (g_gicv2_intc)
dtoverlay=disable-bt       # route PL011 to GPIO14/15 (header pins 8/10)
hdmi_force_hotplug=1       # drive HDMI even if the monitor isn't detected at boot
kernel=kernel8.img
EOF

[ -f "$BOOT/kernel8.img" ] || echo "NOTE: no kernel8.img in $BOOT yet - run tools/build-rpi4-kernel.sh."

echo "Pi 4 firmware fetched into $BOOT (kernel8.img comes from the kernel build):"
ls -la "$BOOT"
