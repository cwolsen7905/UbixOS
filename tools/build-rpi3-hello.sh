#!/bin/sh
# Build the Raspberry Pi 3B v1.2 (BCM2837) M0 serial-hello kernel - a tiny,
# standalone bare-metal arm64 binary that prints a banner over PL011.  The
# VideoCore firmware loads a raw flat binary at 0x80000, so the output is a plain
# `kernel8.img` (no arm64 Image header).  See docs/design/raspberry-pi-3b-bringup.md.
#
# Usage:  sh tools/build-rpi3-hello.sh
# Output: build/rpi3b/kernel8.img  (copy to the SD FAT partition alongside the
#         Pi firmware + config.txt + DTB; power on; watch serial @ 115200)
set -e

CROSS=${CROSS:-aarch64-elf-}
SRC=sys/arch/aarch64/board/rpi3b
OUT=build/rpi3b
mkdir -p "$OUT"

CFLAGS="-ffreestanding -nostdlib -mgeneral-regs-only -fno-pic -fno-stack-protector -Wall -Wextra"

"${CROSS}gcc" $CFLAGS -c "$SRC/start.S" -o "$OUT/start.o"
"${CROSS}gcc" $CFLAGS -O2 -c "$SRC/hello.c" -o "$OUT/hello.o"
"${CROSS}ld" -T "$SRC/m0.ld" "$OUT/start.o" "$OUT/hello.o" -o "$OUT/rpi3b-m0.elf"
"${CROSS}objcopy" -O binary "$OUT/rpi3b-m0.elf" "$OUT/kernel8.img"

echo "built $OUT/kernel8.img ($(wc -c < "$OUT/kernel8.img") bytes)"
echo "entry/load address:"
"${CROSS}objdump" -f "$OUT/rpi3b-m0.elf" | grep -i "start address" || true
