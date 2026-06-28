#!/bin/sh
# Build the full Raspberry Pi 3 (BCM2837) kernel: BOARD_RPI3, linked at 0x80000,
# flattened to a raw kernel8.img the VideoCore firmware loads directly.  Builds into
# build/rpi3b/ (separate from the QEMU build/aarch64 so neither clobbers the other's
# objects), reusing the already-built aarch64 world (libc + musl crt + busybox) for
# the embedded init/login/sh boot triad.  See docs/design/raspberry-pi-3b-bringup.md.
#
# Prereq:  bmake world TARGET=aarch64     (builds build/aarch64/lib/libc.a + musl)
# Usage:   sh tools/build-rpi3-kernel.sh
# Output:  build/rpi3b/boot/kernel8.img   (then: sh tools/make-rpi3-sd.sh)
set -e

SRC=$PWD
OBJ=$SRC/build/rpi3b
WORLD=$SRC/build/aarch64
OBJCOPY=${OBJCOPY:-aarch64-elf-objcopy}

[ -f "$WORLD/lib/libc.a" ] || {
	echo "build the aarch64 world first: bmake world TARGET=aarch64"
	exit 1
}

# Stage the world embed inputs so the recipe embeds the real static init/login/sh
# (the Pi has no disk yet, so it boots that triad from a ramfs root).
mkdir -p "$OBJ/lib" "$OBJ/bin" "$OBJ/obj"
cp -f "$WORLD/lib/libc.a" "$OBJ/lib/"
cp -Rf "$WORLD/obj/musl" "$OBJ/obj/" 2>/dev/null || true
[ -f "$WORLD/bin/cat" ] && cp -f "$WORLD/bin/cat" "$OBJ/bin/" || true
[ -f "$WORLD/ld-musl-aarch64.so.1" ] && cp -f "$WORLD/ld-musl-aarch64.so.1" "$OBJ/" || true

# Build the kernel with the Pi compile/link flags into build/rpi3b.
bmake kernel-aarch64 TARGET=aarch64 OBJ_DIR="$OBJ" \
	AARCH64_KCFLAGS_EXTRA=-DBOARD_RPI3 \
	AARCH64_LD_EXTRA="--defsym KERNEL_PHYS=0x80000"

# Flatten the ELF to a raw image (loaded at 0x80000 by the firmware — no Image header).
"$OBJCOPY" -O binary "$OBJ/boot/kernel" "$OBJ/boot/kernel8.img"
echo "Pi kernel: $OBJ/boot/kernel8.img ($(wc -c < "$OBJ/boot/kernel8.img") bytes)"
"$OBJCOPY" --version >/dev/null 2>&1 && aarch64-elf-objdump -f "$OBJ/boot/kernel" 2>/dev/null | grep -i "start address" || true
