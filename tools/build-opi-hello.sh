#!/bin/sh
# Build the Orange Pi Zero 2W (Allwinner H618) M0 serial-hello Image - a tiny,
# standalone bare-metal arm64 binary that prints a banner over UART0.  Produces a
# flat `Image` that U-Boot's `booti` accepts.  See docs/design/orange-pi-zero2w-
# bringup.md (M0).
#
# Usage:  sh tools/build-opi-hello.sh
# Output: build/opizero2w/opi-zero2w-m0.bin
set -e

CROSS=${CROSS:-aarch64-elf-}
SRC=sys/arch/aarch64/board/opizero2w
OUT=build/opizero2w
mkdir -p "$OUT"

CFLAGS="-ffreestanding -nostdlib -mgeneral-regs-only -fno-pic -fno-stack-protector -Wall -Wextra"

"${CROSS}gcc" $CFLAGS -c "$SRC/start.S" -o "$OUT/start.o"
"${CROSS}gcc" $CFLAGS -O2 -c "$SRC/hello.c" -o "$OUT/hello.o"
"${CROSS}ld" -T "$SRC/m0.ld" "$OUT/start.o" "$OUT/hello.o" -o "$OUT/opi-zero2w-m0.elf"
"${CROSS}objcopy" -O binary "$OUT/opi-zero2w-m0.elf" "$OUT/opi-zero2w-m0.bin"

echo "built $OUT/opi-zero2w-m0.bin ($(wc -c < "$OUT/opi-zero2w-m0.bin") bytes)"
magic=$(od -An -tx1 -j 56 -N 4 "$OUT/opi-zero2w-m0.bin" | tr -d ' \n')
echo "arm64 Image magic @0x38: $magic (expect 41524d64 = \"ARM\\x64\")"
[ "$magic" = "41524d64" ] && echo "OK: valid arm64 Image header" || echo "FAIL: bad Image magic"
