#!/bin/sh
# Build the Raspberry Pi 4 / Pi 400 (BCM2711) kernel under the ARCH + BOARD layout:
#
#   build/aarch64/               = the ARCH — the shared world + default kernel.
#   build/aarch64/boards/rpi4/   = the BOARD — the board-specific kernel (BOARD_RPI4,
#                                  linked at 0x80000) + its SD image.
#
# Same shape as tools/build-rpi3-kernel.sh: reuse the shared aarch64 world in place
# via symlinks (no copy), compile the kernel with -DBOARD_RPI4, link at 0x80000, and
# flatten to a raw kernel8.img the VideoCore firmware loads directly (no Image header
# — start.S omits _head for BCM Pis).  See docs/design/raspberry-pi-4-400-bringup.md.
#
# Prereq: bmake world TARGET=aarch64      (builds build/aarch64/lib/libc.a + musl)
# Usage:  sh tools/build-rpi4-kernel.sh   (or: bmake kernel-rpi4)
# Output: build/aarch64/boards/rpi4/boot/kernel8.img   (then: bmake image-rpi4)
set -e

SRC=$PWD
ARCH_DIR=$SRC/build/aarch64
OBJ=$ARCH_DIR/boards/rpi4
OBJCOPY=${OBJCOPY:-aarch64-elf-objcopy}

[ -f "$ARCH_DIR/lib/libc.a" ] || {
	echo "build the aarch64 world first: bmake world TARGET=aarch64"
	exit 1
}

mkdir -p "$OBJ/obj/sys" "$OBJ/boot"
rm -rf "$OBJ/lib" "$OBJ/bin" "$OBJ/obj/musl"
ln -snf ../../lib "$OBJ/lib"
ln -snf ../../bin "$OBJ/bin"
ln -snf ../../../obj/musl "$OBJ/obj/musl"

bmake kernel-aarch64 TARGET=aarch64 OBJ_DIR="$OBJ" \
	AARCH64_KCFLAGS_EXTRA=-DBOARD_RPI4 \
	AARCH64_LD_EXTRA="--defsym KERNEL_PHYS=0x80000"

"$OBJCOPY" -O binary "$OBJ/boot/kernel" "$OBJ/boot/kernel8.img"
echo "Pi 4 kernel: $OBJ/boot/kernel8.img ($(wc -c < "$OBJ/boot/kernel8.img") bytes)"
aarch64-elf-objdump -f "$OBJ/boot/kernel" 2>/dev/null | grep -i "start address" || true
