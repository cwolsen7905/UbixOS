#!/bin/sh
# Build a bootable Raspberry Pi 4 / Pi 400 microSD image.  Two shapes:
#
#   FULL  (default, when ubixos-arm.img exists): reuse the QEMU aarch64 pool image's
#         MBR layout (FAT boot + swap + UbixFS pool, type 0x9C) and its 3 GB world,
#         swapping the FAT contents for the Pi 4 firmware (start4.elf/fixup4.dat) +
#         config.txt + bcm2711 DTBs + our kernel8.img.  The kernel then mounts the
#         UbixFS pool as / (R4.3 EMMC2 SD driver) = the full uBixOS world.
#   BOOT-ONLY (no ubixos-arm.img): a single 64 MB FAT boot partition — the kernel
#         boots its embedded ramfs (R4.0 serial hello, no pool).
#
# Unlike the Pi 3 the Pi 4 has NO bootcode.bin (its first-stage bootloader lives in
# the SPI EEPROM); it loads start4.elf/fixup4.dat directly.  See
# docs/design/raspberry-pi-4-400-bringup.md.
#
# Prereqs:  sh tools/build-rpi4-kernel.sh ; sh tools/fetch-rpi4-firmware.sh
#           (and `bmake image TARGET=aarch64` for the pool, for the FULL image)
# Usage:    sh tools/make-rpi4-sd.sh                # -> build/aarch64/boards/rpi4/rpi4-sd.img
#           sh tools/make-rpi4-sd.sh /dev/rdiskN    # ALSO write it straight to the device
set -e

BOARD_DIR=build/aarch64/boards/rpi4
BOOT=$BOARD_DIR/boot
IMG=$BOARD_DIR/rpi4-sd.img
SIZE_MB=64
DEV="$1"

[ -f "$BOOT/start4.elf" ] || { echo "missing $BOOT/start4.elf - run tools/fetch-rpi4-firmware.sh first"; exit 1; }
[ -f "$BOOT/kernel8.img" ] || { echo "missing $BOOT/kernel8.img - run tools/build-rpi4-kernel.sh first"; exit 1; }
command -v mformat >/dev/null || { echo "mtools not found - brew install mtools"; exit 1; }

rm -f "$IMG"
POOL_IMG=ubixos-arm.img
if [ -f "$POOL_IMG" ]; then
	# FULL image: reuse the QEMU pool image's UbixFS pool (the world) + its MBR
	# layout (FAT boot at LBA 2048 + swap + pool type 0x9C), swapping the FAT
	# contents for the Pi 4 firmware + kernel8.img.  (cp clones sparsely on APFS;
	# only the FAT diverges.)  mcopy -s recurses into overlays/ (disable-bt.dtbo).
	echo "make-rpi4-sd: FULL image — reusing $POOL_IMG's UbixFS pool (the world)"
	cp "$POOL_IMG" "$IMG"
	mformat -i "${IMG}@@1M" -F ::            # clear the FAT (drop the QEMU kernel)
	mcopy -s -o -i "${IMG}@@1M" "$BOOT"/* :: # Pi 4 firmware + kernel8.img + config + DTBs + overlays/
else
	# Boot-only FAT image (no pool — R4.0 ramfs root).  Run 'bmake image TARGET=aarch64'
	# first for the full pool image.  Sparse: count=0 seek=N extends to N MiB.
	echo "make-rpi4-sd: boot-only FAT image (no $POOL_IMG → ramfs root only)"
	dd if=/dev/zero of="$IMG" bs=1m count=0 seek=${SIZE_MB} 2>/dev/null
	python3 - "$IMG" ${SIZE_MB} <<'PY'
import sys, struct
img, size_mb = sys.argv[1], int(sys.argv[2])
SEC = 512
total = size_mb * 1024 * 1024 // SEC
start = 2048
count = total - start
def chs(lba):
    if lba > 1023 * 255 * 63:
        lba = 1023 * 255 * 63
    c = lba // (255 * 63); h = (lba // 63) % 255; s = lba % 63 + 1
    return bytes([h & 0xff, ((c >> 2) & 0xc0) | (s & 0x3f), c & 0xff])
entry = bytes([0x80]) + chs(start) + bytes([0x0C]) + chs(start + count - 1) + struct.pack('<II', start, count)
with open(img, 'r+b') as f:
    f.seek(446); f.write(entry)
    f.seek(510); f.write(b'\x55\xaa')
print(f"  MBR: FAT32 (0x0C) LBA {start}..{start+count-1} ({count*SEC//1024//1024} MB)")
PY
	mformat -i "${IMG}@@1M" -F ::
	mcopy -s -o -i "${IMG}@@1M" "$BOOT"/* ::
fi

echo "built $IMG"
mdir -i "${IMG}@@1M" :: | grep -iE "kernel8|start4|fixup4|config|bcm2711" || true

if [ -z "$DEV" ]; then
	cat <<EOF

To flash it (find the SD with: diskutil list):
  diskutil unmountDisk /dev/diskN
  sudo dd if=$IMG of=/dev/rdiskN bs=1m      # rdiskN (raw) is much faster than diskN
  sync && diskutil eject /dev/diskN
Then insert into the Pi 4 / Pi 400 and power on (serial @ 115200, GPIO14/15 = pins 8/10).
EOF
	exit 0
fi

# Direct-to-device path (opt-in).
echo
echo "*** About to OVERWRITE $DEV with $IMG - this ERASES the device. ***"
printf "Re-type the device to confirm (%s): " "$DEV"
read confirm
[ "$confirm" = "$DEV" ] || { echo "aborted."; exit 1; }
diskutil unmountDisk "$DEV" || true
sudo dd if="$IMG" of="$DEV" bs=1m
sync
diskutil eject "$DEV" || true
echo "wrote $IMG to $DEV - insert into the Pi 4 / Pi 400 and power on (serial @ 115200)."
