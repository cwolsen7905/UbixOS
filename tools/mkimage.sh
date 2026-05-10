#!/bin/sh
# tools/mkimage.sh — Create a bootable QEMU disk image for UbixOS.
#
# Prerequisites (macOS via Homebrew):
#   brew install qemu mtools
#   brew install grub  (provides grub-install, grub-mkrescue, etc.)
#
# Usage:
#   sh tools/mkimage.sh [output.img]
#
# NOTE: This script requires multiboot support in sys/init/start.S.
# That work is tracked on this feature branch (feature/macos-build-qemu).
# Until start.S is updated this script will produce a bootable image but
# the kernel will not initialise correctly.

set -e

IMG="${1:-ubixos.img}"
IMG_SIZE_MB=512
KERNEL="sys/compile/kernel"
GRUB_CFG="tools/grub.cfg"

# Verify the kernel binary exists.
if [ ! -f "$KERNEL" ]; then
  echo "ERROR: $KERNEL not found — run 'bmake kernel' first." >&2
  exit 1
fi

echo "==> Creating disk image: $IMG (${IMG_SIZE_MB} MB)"
qemu-img create -f raw "$IMG" "${IMG_SIZE_MB}M"

# Write a protective MBR + one FAT32 partition covering the whole disk.
# We use Python to write the partition table directly into the image so
# the script has no dependency on platform-specific fdisk variants.
python3 - "$IMG" "$IMG_SIZE_MB" <<'PYEOF'
import sys, struct

img_path = sys.argv[1]
size_mb  = int(sys.argv[2])

SECTOR    = 512
HEADS     = 16
SECTS     = 63
CYLINDERS = (size_mb * 1024 * 1024) // (HEADS * SECTS * SECTOR)

def lba_to_chs(lba):
    c = lba // (HEADS * SECTS)
    h = (lba // SECTS) % HEADS
    s = (lba % SECTS) + 1
    c = min(c, 1023)
    return (h, s | ((c & 0x300) >> 2), c & 0xFF)

# Partition starts at LBA 2048 (1 MB offset, standard alignment).
part_start = 2048
part_end   = (size_mb * 1024 * 1024 // SECTOR) - 1
part_size  = part_end - part_start + 1

chs_start = lba_to_chs(part_start)
chs_end   = lba_to_chs(part_end)

entry = struct.pack('<BBBBBBBBII',
    0x80,           # bootable
    chs_start[0], chs_start[1], chs_start[2],
    0x0C,           # FAT32 with LBA
    chs_end[0], chs_end[1], chs_end[2],
    part_start, part_size)

with open(img_path, 'r+b') as f:
    # Write MBR signature and the single partition entry at offset 446.
    f.seek(446)
    f.write(entry)
    f.write(b'\x00' * 48)   # remaining three empty partition entries
    f.write(b'\x55\xAA')    # MBR signature

print(f"  Partition: LBA {part_start}–{part_end} ({part_size} sectors, FAT32)")
PYEOF

echo "==> Formatting FAT32 partition"
# mformat from mtools writes a FAT32 filesystem into a partition in a raw image
# without needing root or a loop device.
mformat -i "$IMG"@@1M -F -v UBIXOS ::

echo "==> Installing GRUB"
# grub-install needs a directory to use as the EFI/BIOS target.
GRUB_DIR=$(mktemp -d)
trap 'rm -rf "$GRUB_DIR"' EXIT

mmd -i "$IMG"@@1M ::/boot
mmd -i "$IMG"@@1M ::/boot/grub

# Copy GRUB modules into the image.
GRUB_LIB=$(grub-install --target=i386-pc --print-libdir 2>/dev/null || true)
if [ -z "$GRUB_LIB" ]; then
  echo "WARNING: Could not find GRUB library path — GRUB may not be installed correctly." >&2
  echo "  Install with: brew install grub" >&2
else
  mcopy -i "$IMG"@@1M -s "$GRUB_LIB"/i386-pc ::/boot/grub/
fi

grub-install \
  --target=i386-pc \
  --boot-directory="$GRUB_DIR/boot" \
  --no-floppy \
  "$IMG" 2>/dev/null || \
  echo "WARNING: grub-install failed — you may need 'brew install grub'."

echo "==> Installing GRUB config"
mcopy -i "$IMG"@@1M "$GRUB_CFG" ::/boot/grub/grub.cfg

echo "==> Installing kernel"
mmd  -i "$IMG"@@1M ::/boot/kernel 2>/dev/null || true
mcopy -i "$IMG"@@1M "$KERNEL" ::/boot/kernel/kernel

echo ""
echo "Done: $IMG"
echo "Run with: bmake run"
echo "     or:  qemu-system-i386 -m 256 -drive file=$IMG,format=raw,if=ide -serial stdio -vga std"
