#!/bin/sh
# Fetch the Raspberry Pi 3 VideoCore GPU firmware (proprietary Broadcom blobs -
# not redistributable, so fetched at use time, never vendored) + the board DTB,
# and assemble a ready-to-copy SD boot/ directory: firmware + config.txt + our
# kernel8.img.  See docs/design/raspberry-pi-3b-bringup.md.
#
# Usage:  sh tools/build-rpi3-hello.sh   # build kernel8.img first
#         sh tools/fetch-rpi3-firmware.sh
# Then copy the contents of build/rpi3b/boot/ onto the SD's FAT32 partition.
set -e

BOOT=build/aarch64/boards/rpi3/boot # ARCH + BOARD layout (see raspberry-pi-3b-bringup.md)
RAW=https://github.com/raspberrypi/firmware/raw/master/boot
mkdir -p "$BOOT"

for f in bootcode.bin start.elf fixup.dat bcm2710-rpi-3-b.dtb; do
	echo "fetching $f ..."
	curl -fsSL "$RAW/$f" -o "$BOOT/$f"
done

cat > "$BOOT/config.txt" <<EOF
# uBixOS Raspberry Pi 3B v1.2 boot config (see raspberry-pi-3b-bringup.md)
arm_64bit=1
enable_uart=1
init_uart_clock=48000000   # fix UARTCLK so M0's IBRD/FBRD (26/3) = 115200
dtoverlay=disable-bt       # route PL011 to GPIO14/15 (header pins 8/10)
hdmi_force_hotplug=1       # drive HDMI even if the monitor isn't detected at boot (M6 fb)
kernel=kernel8.img
EOF

# The kernel8.img is produced by the kernel build (tools/build-rpi3-kernel.sh, or
# the M0.75 standalone tools/build-rpi3-hello.sh) directly into $BOOT — this script
# only fetches the (proprietary, un-vendored) firmware + writes config.txt.
[ -f "$BOOT/kernel8.img" ] || echo "NOTE: no kernel8.img in $BOOT yet - run tools/build-rpi3-kernel.sh."

echo "firmware fetched into $BOOT (kernel8.img comes from the kernel build):"
ls -la "$BOOT"
