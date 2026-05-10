#!/bin/sh
# tools/mkhd.sh — Create a FAT32 IDE hard disk image with the UbixOS world files.
#
# The kernel mounts this disk as "sys:" (major=2/minor=1, primary IDE slave).
# QEMU exposes it as: -drive file=ubixos_hd.img,format=raw,if=ide,index=1
#
# Prerequisites: mtools (mformat, mmd, mcopy)
#
# Usage:
#   sh tools/mkhd.sh [output.img]

set -e

IMG="${1:-ubixos_hd.img}"
IMG_SIZE_MB=128
BUILD="build"

if ! command -v mformat >/dev/null 2>&1; then
  echo "ERROR: mtools not found. Install with: brew install mtools" >&2
  exit 1
fi

if [ ! -d "$BUILD/bin" ]; then
  echo "ERROR: $BUILD/bin not found — run 'bmake world' first." >&2
  exit 1
fi

echo "==> Creating HD image: $IMG (${IMG_SIZE_MB} MB)"
qemu-img create -f raw "$IMG" "${IMG_SIZE_MB}M"

# Write MBR with one FAT32 partition (type 0x0C) starting at LBA 2048.
python3 - "$IMG" "$IMG_SIZE_MB" <<'PYEOF'
import sys, struct

img_path = sys.argv[1]
size_mb  = int(sys.argv[2])

SECTOR    = 512
HEADS     = 16
SECTS     = 63

def lba_to_chs(lba):
    c = lba // (HEADS * SECTS)
    h = (lba // SECTS) % HEADS
    s = (lba % SECTS) + 1
    c = min(c, 1023)
    return (h, s | ((c & 0x300) >> 2), c & 0xFF)

part_start = 2048
part_end   = (size_mb * 1024 * 1024 // SECTOR) - 1
part_size  = part_end - part_start + 1

chs_start = lba_to_chs(part_start)
chs_end   = lba_to_chs(part_end)

entry = struct.pack('<BBBBBBBBII',
    0x80,
    chs_start[0], chs_start[1], chs_start[2],
    0x0C,           # FAT32 with LBA
    chs_end[0], chs_end[1], chs_end[2],
    part_start, part_size)

with open(img_path, 'r+b') as f:
    f.seek(446)
    f.write(entry)
    f.write(b'\x00' * 48)
    f.write(b'\x55\xAA')

print(f"  Partition: LBA {part_start}-{part_end} ({part_size} sectors, FAT32 type 0x0C)")
PYEOF

echo "==> Formatting FAT32 partition (offset 1MB)"
mformat -i "$IMG"@@1M -F -v UBIXOS ::

echo "==> Copying world files"
mmd -i "$IMG"@@1M ::/bin
mmd -i "$IMG"@@1M ::/lib
mmd -i "$IMG"@@1M ::/libexec

for f in "$BUILD/bin"/*; do
  [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/bin/
done

for f in "$BUILD/lib"/*; do
  [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/lib/
done

for f in "$BUILD/libexec"/*; do
  [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/libexec/
done

echo ""
echo "Done: $IMG"
echo "Add to QEMU: -drive file=$IMG,format=raw,if=ide,index=1"
