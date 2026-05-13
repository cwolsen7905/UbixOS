#!/bin/sh
# tools/mkimage.sh — Create a single bootable FAT32 HD image for UbixOS.
#
# Layout:
#   Sector 0        : GRUB boot.img (MBR bootloader)
#   Sectors 1-2047  : GRUB core.img (stage 1.5 — loads FAT driver + grub.cfg)
#   LBA 2048+       : FAT32 partition containing:
#                       boot/grub/grub.cfg
#                       boot/kernel/kernel
#                       bin/  lib/  libexec/
#
# Prerequisites (macOS via Homebrew):
#   brew install qemu mtools
#   brew install i686-elf-grub
#
# Usage:
#   sh tools/mkimage.sh [output.img]

set -e

IMG="${1:-ubixos.img}"
IMG_SIZE_MB=512
KERNEL="build/boot/kernel"
GRUB_CFG="tools/grub.cfg"
BUILD="build"
GRUB_LIB="/opt/homebrew/Cellar/i686-elf-grub/2.12/lib/i686-elf/grub/i386-pc"

# ── Preflight checks ────────────────────────────────────────────────────────
for cmd in qemu-img mformat mmd mcopy i686-elf-grub-mkimage python3; do
  command -v "$cmd" >/dev/null 2>&1 || { echo "ERROR: $cmd not found" >&2; exit 1; }
done
[ -f "$KERNEL" ]   || { echo "ERROR: $KERNEL not found — run 'bmake kernel' first." >&2; exit 1; }
[ -d "$BUILD/bin" ] || { echo "ERROR: $BUILD/bin not found — run 'bmake world' first." >&2; exit 1; }
[ -f "$GRUB_LIB/boot.img" ] || { echo "ERROR: GRUB boot.img not found at $GRUB_LIB" >&2; exit 1; }

echo "==> Creating disk image: $IMG (${IMG_SIZE_MB} MB)"
qemu-img create -f raw "$IMG" "${IMG_SIZE_MB}M"

# ── Partition table: one FAT32 partition at LBA 2048 ───────────────────────
python3 - "$IMG" "$IMG_SIZE_MB" <<'PYEOF'
import sys, struct
img_path = sys.argv[1]
size_mb  = int(sys.argv[2])
SECTOR = 512; HEADS = 16; SECTS = 63
def lba_to_chs(lba):
    c = lba // (HEADS * SECTS); h = (lba // SECTS) % HEADS; s = (lba % SECTS) + 1
    return (h, s | ((min(c,1023) & 0x300) >> 2), min(c,1023) & 0xFF)
part_start = 2048
part_end   = (size_mb * 1024 * 1024 // SECTOR) - 1
entry = struct.pack('<BBBBBBBBII',
    0x80, *lba_to_chs(part_start), 0x0C, *lba_to_chs(part_end),
    part_start, part_end - part_start + 1)
with open(img_path, 'r+b') as f:
    f.seek(446); f.write(entry); f.write(b'\x00'*48); f.write(b'\x55\xAA')
print(f"  Partition: LBA {part_start}-{part_end} (FAT32 type 0x0C)")
PYEOF

# ── Format FAT32 partition ──────────────────────────────────────────────────
echo "==> Formatting FAT32 partition"
mformat -i "$IMG"@@1M -F -v UBIXOS ::

# ── Build GRUB core image embedding FAT + multiboot support ────────────────
echo "==> Building GRUB core image"
CORE_IMG=$(mktemp /tmp/grub_core.XXXXXX.img)
trap 'rm -f "$CORE_IMG"' EXIT

i686-elf-grub-mkimage \
  -O i386-pc \
  -o "$CORE_IMG" \
  -p '(hd0,msdos1)/boot/grub' \
  biosdisk part_msdos fat multiboot normal echo

# Write GRUB MBR (boot.img) into sector 0, preserving our partition table.
# boot.img is 512 bytes; we copy it then restore the partition table we wrote.
python3 - "$IMG" "$GRUB_LIB/boot.img" <<'PYEOF'
import sys
img_path  = sys.argv[1]
boot_path = sys.argv[2]
boot = open(boot_path, 'rb').read(512)
with open(img_path, 'r+b') as f:
    # Save partition table (offsets 446-511)
    f.seek(446); saved = f.read(66)
    # Write boot.img
    f.seek(0); f.write(boot)
    # Restore partition table
    f.seek(446); f.write(saved)
    # Patch boot.img to point core.img at sectors 1..N (little-endian 32-bit at offset 0x44)
    # boot.img expects the core.img LBA at offset 0x44
    f.seek(0x44); f.write((1).to_bytes(4, 'little'))
print("  GRUB boot.img written to sector 0")
PYEOF

# Write core.img into sectors 1-2047 (the embedding zone between MBR and partition).
CORE_SIZE=$(wc -c < "$CORE_IMG" | tr -d ' ')
CORE_SECTORS=$(( (CORE_SIZE + 511) / 512 ))
if [ "$CORE_SECTORS" -ge 2048 ]; then
  echo "ERROR: GRUB core image too large ($CORE_SECTORS sectors, max 2047)" >&2
  exit 1
fi
echo "  GRUB core.img: $CORE_SIZE bytes ($CORE_SECTORS sectors)"
python3 - "$IMG" "$CORE_IMG" <<'PYEOF'
import sys
img_path  = sys.argv[1]
core_path = sys.argv[2]
core = open(core_path, 'rb').read()
with open(img_path, 'r+b') as f:
    f.seek(512)   # sector 1
    f.write(core)
print(f"  core.img written at sector 1 ({len(core)} bytes)")
PYEOF

# ── Copy files into the FAT partition ──────────────────────────────────────
echo "==> Installing GRUB config"
mmd -i "$IMG"@@1M ::/boot
mmd -i "$IMG"@@1M ::/boot/grub
mmd -i "$IMG"@@1M ::/boot/kernel
mcopy -i "$IMG"@@1M "$GRUB_CFG" ::/boot/grub/grub.cfg
mcopy -i "$IMG"@@1M "$KERNEL"   ::/boot/kernel/kernel

# Copy GRUB modules so grub.cfg can use insmod at runtime.
TMPMOD=$(mktemp -d)
trap 'rm -rf "$TMPMOD"; rm -f "$CORE_IMG"' EXIT
cp "$GRUB_LIB"/*.mod "$GRUB_LIB"/*.lst "$TMPMOD"/ 2>/dev/null || true
mmd -i "$IMG"@@1M ::/boot/grub/i386-pc
mcopy -i "$IMG"@@1M "$TMPMOD"/*.mod "$TMPMOD"/*.lst ::/boot/grub/i386-pc/ 2>/dev/null || true

echo "==> Installing world files (bin/ lib/ libexec/)"
mmd -i "$IMG"@@1M ::/bin
mmd -i "$IMG"@@1M ::/lib
mmd -i "$IMG"@@1M ::/libexec

for f in "$BUILD/bin"/*;    do [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/bin/;    done
for f in "$BUILD/lib"/*;    do [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/lib/;    done
for f in "$BUILD/libexec"/*; do [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/libexec/; done

echo "==> Installing TCC compiler (lib/tcc/ and usr/bin/)"
mmd -i "$IMG"@@1M ::/lib/tcc 2>/dev/null || true
mmd -i "$IMG"@@1M ::/lib/tcc/include 2>/dev/null || true
for f in "$BUILD/lib/tcc"/*;    do [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/lib/tcc/;    done
for f in "$BUILD/lib/tcc/include"/*; do [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/lib/tcc/include/; done
mmd -i "$IMG"@@1M ::/usr 2>/dev/null || true
mmd -i "$IMG"@@1M ::/usr/bin 2>/dev/null || true
[ -f "$BUILD/bin/tcc" ] && mcopy -i "$IMG"@@1M "$BUILD/bin/tcc" ::/usr/bin/tcc

echo "==> Installing system headers (include/)"
mmd -i "$IMG"@@1M ::/include 2>/dev/null || true
for f in include/*.h; do [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/include/; done
# sys/ sub-directory of headers
mmd -i "$IMG"@@1M ::/include/sys 2>/dev/null || true
for f in include/sys/*.h; do [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/include/sys/; done
# machine/ sub-directory
mmd -i "$IMG"@@1M ::/include/machine 2>/dev/null || true
for f in include/machine/*.h; do [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/include/machine/; done

echo "==> Installing source examples (src/)"
mmd -i "$IMG"@@1M ::/src 2>/dev/null || true
for f in tools/src/*; do [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/src/; done

echo "==> Installing system files (etc/)"
mmd -i "$IMG"@@1M ::/etc
[ -f tools/userdb ] && mcopy -i "$IMG"@@1M tools/userdb ::/etc/userdb
[ -f tools/fstab  ] && mcopy -i "$IMG"@@1M tools/fstab  ::/etc/fstab
[ -f tools/motd   ] && mcopy -i "$IMG"@@1M tools/motd   ::/etc/motd

echo "==> Installing assets (var/)"
mmd -i "$IMG"@@1M ::/var 2>/dev/null || true
mmd -i "$IMG"@@1M ::/var/background 2>/dev/null || true
[ -f sys/sde/assets/ubix.bmp ] && mcopy -i "$IMG"@@1M sys/sde/assets/ubix.bmp ::/var/background/ubix.bmp
mmd -i "$IMG"@@1M ::/var/fonts 2>/dev/null || true
for f in tools/*.DPF; do [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/var/fonts/; done
[ -f lib/objgfx40/BOLD.DPF ] && mcopy -i "$IMG"@@1M lib/objgfx40/BOLD.DPF ::/var/fonts/

echo ""
echo "Done: $IMG"
echo "Run with: bmake run"
