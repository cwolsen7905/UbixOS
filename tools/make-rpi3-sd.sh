#!/bin/sh
# Build a bootable Raspberry Pi 3 microSD image: an MBR with a single FAT32 boot
# partition (type 0x0C) holding the Pi firmware + config.txt + DTB + our
# kernel8.img.  macOS-friendly (sparse dd + a Python MBR + mtools - no mount),
# mirroring tools/mkimage.sh.  See docs/design/raspberry-pi-3b-bringup.md.
#
# Prereqs:
#   sh tools/build-rpi3-hello.sh      # build kernel8.img
#   sh tools/fetch-rpi3-firmware.sh   # fetch firmware -> build/rpi3b/boot/
#
# Usage:
#   sh tools/make-rpi3-sd.sh                 # -> build/rpi3b/rpi3-sd.img (then dd it yourself)
#   sh tools/make-rpi3-sd.sh /dev/rdiskN     # ALSO write it straight to that device (DANGEROUS)
set -e

BOOT=build/rpi3b/boot
IMG=build/rpi3b/rpi3-sd.img
SIZE_MB=64
DEV="$1"

[ -f "$BOOT/start.elf" ]        || { echo "missing $BOOT/start.elf - run tools/fetch-rpi3-firmware.sh first"; exit 1; }
[ -f build/rpi3b/kernel8.img ]  || { echo "missing build/rpi3b/kernel8.img - run tools/build-rpi3-hello.sh first"; exit 1; }
command -v mformat >/dev/null   || { echo "mtools not found - brew install mtools"; exit 1; }

# Always stage the freshest kernel build into the boot dir so a rebuild is picked
# up without re-fetching the firmware.
cp build/rpi3b/kernel8.img "$BOOT/kernel8.img"

# Sparse image: count=0 seek=N extends to N MiB without writing data.
rm -f "$IMG"
dd if=/dev/zero of="$IMG" bs=1m count=0 seek=${SIZE_MB} 2>/dev/null

# MBR: one bootable FAT32-LBA (0x0C) partition starting at LBA 2048 (1 MiB).
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

# Format the FAT32 partition (at the 1 MiB offset) + copy every boot file to root.
mformat -i "${IMG}@@1M" -F ::
mcopy -o -i "${IMG}@@1M" "$BOOT"/* ::

echo "built $IMG"
mdir -i "${IMG}@@1M" ::

if [ -z "$DEV" ]; then
	cat <<EOF

To flash it (find the SD with: diskutil list):
  diskutil unmountDisk /dev/diskN
  sudo dd if=$IMG of=/dev/rdiskN bs=1m      # rdiskN (raw) is much faster than diskN
  sync && diskutil eject /dev/diskN
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
echo "wrote $IMG to $DEV - insert into the Pi 3 and power on (serial @ 115200)."
