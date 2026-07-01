#!/bin/sh
# Build the Raspberry Pi 3 (BCM2837) kernel under the ARCH + BOARD build layout:
#
#   build/aarch64/               = the ARCH — the shared world + the default
#                                  (qemu-virt) kernel + image.  Board-independent.
#   build/aarch64/boards/rpi3/   = the BOARD — only the board-specific kernel
#                                  (BOARD_RPI3, linked at 0x80000) + its SD image.
#
# The Pi is not a separate arch — it is aarch64 with a byte-identical world.  So the
# board build REUSES the shared world *in place* via symlinks (lib/, bin/, obj/musl
# → ../../..) instead of copying it: no wasted copy, and no race with a concurrent
# world rebuild.  Only the kernel objects (compiled with -DBOARD_RPI3) and the flat
# kernel8.img are board-specific.  See docs/design/raspberry-pi-3b-bringup.md
# (## Build layout: ARCH + BOARD).
#
# Prereq: bmake world TARGET=aarch64      (builds build/aarch64/lib/libc.a + musl)
# Usage:  sh tools/build-rpi3-kernel.sh   (or: bmake kernel-rpi3)
# Output: build/aarch64/boards/rpi3/boot/kernel8.img   (then: bmake image-rpi3)
set -e

SRC=$PWD
ARCH_DIR=$SRC/build/aarch64
OBJ=$ARCH_DIR/boards/rpi3
OBJCOPY=${OBJCOPY:-aarch64-elf-objcopy}

[ -f "$ARCH_DIR/lib/libc.a" ] || {
	echo "build the aarch64 world first: bmake world TARGET=aarch64"
	exit 1
}

# Board obj tree: real obj/sys + boot (the board-specific kernel output); the world
# artifacts the kernel recipe embeds (lib/, bin/, obj/musl) are symlinked to the
# shared arch build so nothing is copied.
mkdir -p "$OBJ/obj/sys" "$OBJ/boot"
rm -rf "$OBJ/lib" "$OBJ/bin" "$OBJ/obj/musl"
ln -snf ../../lib "$OBJ/lib"           # -> build/aarch64/lib
ln -snf ../../bin "$OBJ/bin"           # -> build/aarch64/bin
ln -snf ../../../obj/musl "$OBJ/obj/musl" # -> build/aarch64/obj/musl

# Build the kernel with the Pi compile/link flags into the board dir, reading the
# shared world through the symlinks above.
bmake kernel-aarch64 TARGET=aarch64 OBJ_DIR="$OBJ" \
	AARCH64_KCFLAGS_EXTRA=-DBOARD_RPI3 \
	AARCH64_LD_EXTRA="--defsym KERNEL_PHYS=0x80000"

# Flatten the ELF to a raw image (loaded at 0x80000 by the firmware — no Image header).
"$OBJCOPY" -O binary "$OBJ/boot/kernel" "$OBJ/boot/kernel8.img"
echo "Pi kernel: $OBJ/boot/kernel8.img ($(wc -c < "$OBJ/boot/kernel8.img") bytes)"
aarch64-elf-objdump -f "$OBJ/boot/kernel" 2>/dev/null | grep -i "start address" || true
