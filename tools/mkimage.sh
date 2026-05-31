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
FAT_SIZE_MB=448     # FAT32 partition (type 0x0C)
SWAP_SIZE_MB=64     # raw swap partition (type 0x82)
KERNEL="build/boot/kernel"
GRUB_CFG="tools/grub.cfg"
BUILD="build"
# Detect GRUB library directory and mkimage command dynamically.
if command -v brew >/dev/null 2>&1 && brew --prefix i686-elf-grub >/dev/null 2>&1; then
    GRUB_LIB="$(brew --prefix i686-elf-grub)/lib/i686-elf/grub/i386-pc"
    GRUB_MKIMAGE=i686-elf-grub-mkimage
elif [ -d /usr/lib/grub/i386-pc ]; then
    GRUB_LIB="/usr/lib/grub/i386-pc"
    GRUB_MKIMAGE=grub-mkimage
elif [ -d /usr/lib/grub2/i386-pc ]; then
    GRUB_LIB="/usr/lib/grub2/i386-pc"
    GRUB_MKIMAGE=grub2-mkimage
else
    echo "ERROR: cannot locate GRUB i386-pc modules" >&2; exit 1
fi

# ── Preflight checks ────────────────────────────────────────────────────────
for cmd in qemu-img mformat mmd mcopy "${GRUB_MKIMAGE}" python3; do
  command -v "$cmd" >/dev/null 2>&1 || { echo "ERROR: $cmd not found" >&2; exit 1; }
done
[ -f "$KERNEL" ]   || { echo "ERROR: $KERNEL not found — run 'bmake kernel' first." >&2; exit 1; }
[ -d "$BUILD/bin" ] || { echo "ERROR: $BUILD/bin not found — run 'bmake world' first." >&2; exit 1; }
[ -f "$GRUB_LIB/boot.img" ] || { echo "ERROR: GRUB boot.img not found at $GRUB_LIB" >&2; exit 1; }

echo "==> Creating disk image: $IMG (${IMG_SIZE_MB} MB)"
qemu-img create -f raw "$IMG" "${IMG_SIZE_MB}M"

# ── Partition table: FAT32 + raw swap ──────────────────────────────────────
python3 - "$IMG" "$FAT_SIZE_MB" "$SWAP_SIZE_MB" <<'PYEOF'
import sys, struct
img_path    = sys.argv[1]
fat_size_mb = int(sys.argv[2])
SECTOR = 512; HEADS = 16; SECTS = 63
def lba_to_chs(lba):
    c = lba // (HEADS * SECTS); h = (lba // SECTS) % HEADS; s = (lba % SECTS) + 1
    return (h, s | ((min(c,1023) & 0x300) >> 2), min(c,1023) & 0xFF)
fat_start   = 2048
fat_sectors = fat_size_mb * 1024 * 1024 // SECTOR
fat_end     = fat_start + fat_sectors - 1
swap_start  = fat_end + 1
swap_end    = swap_start + int(sys.argv[3]) * 1024 * 1024 // SECTOR - 1
fat_entry  = struct.pack('<BBBBBBBBII',
    0x80, *lba_to_chs(fat_start),  0x0C, *lba_to_chs(fat_end),
    fat_start,  fat_sectors)
swap_entry = struct.pack('<BBBBBBBBII',
    0x00, *lba_to_chs(swap_start), 0x82, *lba_to_chs(swap_end),
    swap_start, swap_end - swap_start + 1)
with open(img_path, 'r+b') as f:
    f.seek(446)
    f.write(fat_entry)
    f.write(swap_entry)
    f.write(b'\x00' * 32)   # entries 2 and 3 unused
    f.write(b'\x55\xAA')
print(f"  FAT32: LBA {fat_start}-{fat_end} ({fat_size_mb} MB)")
print(f"  Swap:  LBA {swap_start}-{swap_end} ({sys.argv[3]} MB, type 0x82)")
PYEOF

# ── Format FAT32 partition (size-limited to FAT_SIZE_MB) ───────────────────
FAT_SECTORS=$(( FAT_SIZE_MB * 1024 * 1024 / 512 ))
echo "==> Formatting FAT32 partition (${FAT_SIZE_MB} MB, ${FAT_SECTORS} sectors)"
mformat -i "$IMG"@@1M -F -v SYS -T "$FAT_SECTORS" ::

# ── Build GRUB core image embedding FAT + multiboot support ────────────────
echo "==> Building GRUB core image"
CORE_IMG=$(mktemp /tmp/grub_core.XXXXXX.img)
trap 'rm -f "$CORE_IMG"' EXIT

${GRUB_MKIMAGE} \
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
# musl dynamic linker: FAT32 has no symlinks, so install libc.so a second time
# under the LDSO_PATHNAME that the kernel ld.so and ELF binaries expect.
[ -f "$BUILD/lib/libc.so" ] && \
    mcopy -o -i "$IMG"@@1M "$BUILD/lib/libc.so" ::/lib/ld-musl-i386.so.1
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
for f in etc/*; do
  [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/etc/
done
mmd -i "$IMG"@@1M ::/etc/init.d
for f in etc/init.d/*; do
  [ -f "$f" ] && mcopy -i "$IMG"@@1M "$f" ::/etc/init.d/
done

echo "==> Installing DOOM WAD (optional)"
_doom_wad=""
for _candidate in "tools/doom1.wad" "$HOME/Downloads/doom1.wad"; do
    if [ -f "$_candidate" ]; then
        _doom_wad="$_candidate"
        break
    fi
done
if [ -n "$_doom_wad" ]; then
    mcopy -o -i "$IMG"@@1M "$_doom_wad" ::/bin/doom1.wad
    echo "    Installed: $_doom_wad -> /bin/doom1.wad"
else
    echo "    WARNING: doom1.wad not found; copy it to tools/doom1.wad to include it"
fi
# Parse tools/userdb (binary struct array) to get username + home path per user.
# struct userdb_entry: char[32] user, char[32] pass, int uid, int gid,
#                      char[128] shell, char[256] realname, char[256] path
# Total = 712 bytes per entry.
USERDB_ENTRIES=$(python3 - tools/userdb <<'PYEOF'
import sys, struct
ENTRY = struct.Struct('<32s 32s i i 128s 256s 256s')
with open(sys.argv[1], 'rb') as f:
    data = f.read()
for i in range(len(data) // ENTRY.size):
    rec = ENTRY.unpack_from(data, i * ENTRY.size)
    user = rec[0].split(b'\x00', 1)[0].decode()
    home = rec[6].split(b'\x00', 1)[0].decode()
    if user:
        print(f"{user}:{home}")
PYEOF
)

echo "==> Creating /home (user home directories from userdb)"
mmd -i "$IMG"@@1M ::/home 2>/dev/null || true
for entry in $USERDB_ENTRIES; do
    _user="${entry%%:*}"; _home="${entry#*:}"
    echo "    /home/$_user ($entry)"
    mmd -i "$IMG"@@1M "::$_home" 2>/dev/null || true
    for _f in tools/skel/.*; do
        case "$_f" in */.|*/..) continue;; esac
        [ -f "$_f" ] && mcopy -o -i "$IMG"@@1M "$_f" "::$_home/"
    done
done

echo "==> Creating /mnt (automountd mount point root)"
mmd -i "$IMG"@@1M ::/mnt 2>/dev/null || true

echo "==> Creating /tmp (scratch space for editors, builds, etc.)"
mmd -i "$IMG"@@1M ::/tmp 2>/dev/null || true

echo "==> Installing assets (var/)"
mmd -i "$IMG"@@1M ::/var 2>/dev/null || true
mmd -i "$IMG"@@1M ::/var/log 2>/dev/null || true
mmd -i "$IMG"@@1M ::/var/background 2>/dev/null || true
for f in tools/backgrounds/*.bmp; do [ -f "$f" ] && mcopy -o -i "$IMG"@@1M "$f" ::/var/background/; done
mmd -i "$IMG"@@1M ::/var/fonts 2>/dev/null || true
for f in tools/*.DPF; do [ -f "$f" ] && mcopy -o -i "$IMG"@@1M "$f" ::/var/fonts/; done
mmd -i "$IMG"@@1M ::/var/db 2>/dev/null || true
[ -f tools/ubistry.db ] && mcopy -o -i "$IMG"@@1M tools/ubistry.db ::/var/db/ubistry.db

echo ""
echo "Done: $IMG"
echo "Run with: bmake run"
