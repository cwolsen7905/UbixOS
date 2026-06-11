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
IMG_SIZE_MB=656
# /boot-only FAT (kernel + GRUB ≈ 0.5 MB of real content; 33 MB is the FAT32
# floor — ≥65525 clusters).  The world no longer lives here: the UbixFS pool is
# the root filesystem (mounted at / by the kernel), so the pool gets the bulk of
# the disk and FAT carries only what the firmware/GRUB must read pre-mount.
FAT_SIZE_MB=33      # FAT32 /boot partition (type 0x0C) — kernel + GRUB only
SWAP_SIZE_MB=64     # raw swap partition (type 0x82)
POOL_SIZE_MB=550    # UbixFS pool partition (type 0x9C; raw mount) — the root /
POOL_FS_MB=540      # pool filesystem size inside that partition
# BUILD/KERNEL honor the environment so the arch-homed build dir (build/i386)
# can be passed in by the `image` target; default to the flat layout otherwise.
BUILD="${BUILD:-build}"
KERNEL="${KERNEL:-$BUILD/boot/kernel}"
GRUB_CFG="tools/grub.cfg"

# Root filesystem selector (passed by the `image` target; default fat).
# The native CoW root is the lite-ZFS pool (docs/design/ubixfs-pool-plan.md);
# its kernel VFS driver is not wired into the image yet, so fat is the only
# supported root for now.  (The earlier ubixfs2/BeFS design was superseded.)
FS="${FS:-fat}"
case "$FS" in
fat) ;; # the path below builds a FAT32 root
*)
    echo "ERROR: unknown FS '$FS' (only 'fat' is wired into the image today)." >&2
    echo "       Native CoW root in progress: docs/design/ubixfs-pool-plan.md" >&2
    exit 1
    ;;
esac
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
python3 - "$IMG" "$FAT_SIZE_MB" "$SWAP_SIZE_MB" "$POOL_SIZE_MB" <<'PYEOF'
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
pool_start  = swap_end + 1
pool_end    = pool_start + int(sys.argv[4]) * 1024 * 1024 // SECTOR - 1
fat_entry  = struct.pack('<BBBBBBBBII',
    0x80, *lba_to_chs(fat_start),  0x0C, *lba_to_chs(fat_end),
    fat_start,  fat_sectors)
swap_entry = struct.pack('<BBBBBBBBII',
    0x00, *lba_to_chs(swap_start), 0x82, *lba_to_chs(swap_end),
    swap_start, swap_end - swap_start + 1)
pool_entry = struct.pack('<BBBBBBBBII',
    0x00, *lba_to_chs(pool_start), 0x9C, *lba_to_chs(pool_end),
    pool_start, pool_end - pool_start + 1)
with open(img_path, 'r+b') as f:
    f.seek(446)
    f.write(fat_entry)
    f.write(swap_entry)
    f.write(pool_entry)
    f.write(b'\x00' * 16)   # entry 3 unused
    f.write(b'\x55\xAA')
    # Stash the pool partition start LBA for the dd step below.
with open(img_path + '.poollba', 'w') as g:
    g.write(str(pool_start))
print(f"  FAT32: LBA {fat_start}-{fat_end} ({fat_size_mb} MB)")
print(f"  Swap:  LBA {swap_start}-{swap_end} ({sys.argv[3]} MB, type 0x82)")
print(f"  Pool:  LBA {pool_start}-{pool_end} ({sys.argv[4]} MB, type 0x9C)")
PYEOF
POOL_LBA=$(cat "$IMG.poollba"); rm -f "$IMG.poollba"

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

# ── Build the complete root (everything except /boot) in a host staging dir,
#    then load it into the UbixFS pool.  The pool IS the root filesystem — the
#    kernel mounts it at / — so the world lives there, not on FAT (FAT carries
#    only /boot above).  Building into a host dir first keeps one staging path
#    that feeds the pool via the host `ubfs` CLI (the same portable core the
#    kernel links). ────────────────────────────────────────────────────────────
echo "==> Building host UbixFS tool"
if ! ( cd tools/ubixfs && bmake ubfs ) >/dev/null 2>&1 || [ ! -x tools/ubixfs/ubfs ]; then
    echo "ERROR: host ubfs tool unavailable — cannot build the pool root." >&2
    exit 1
fi
UBFS=tools/ubixfs/ubfs
STAGE=$(mktemp -d -t ubixroot)

echo "==> Staging world (bin/ lib/ libexec/)"
mkdir -p "$STAGE/bin" "$STAGE/lib" "$STAGE/libexec"
for f in "$BUILD/bin"/*;     do [ -f "$f" ] && cp "$f" "$STAGE/bin/";     done
for f in "$BUILD/lib"/*;     do [ -f "$f" ] && cp "$f" "$STAGE/lib/";     done
# musl dynamic linker: the pool FS has no symlinks, so install libc.so a second
# time under the LDSO_PATHNAME the kernel ld.so + ELF binaries expect.
[ -f "$BUILD/lib/libc.so" ] && cp "$BUILD/lib/libc.so" "$STAGE/lib/ld-musl-i386.so.1"
for f in "$BUILD/libexec"/*; do [ -f "$f" ] && cp "$f" "$STAGE/libexec/"; done

echo "==> Staging TCC compiler (lib/tcc/ and usr/bin/)"
mkdir -p "$STAGE/lib/tcc/include" "$STAGE/usr/bin"
for f in "$BUILD/lib/tcc"/*;         do [ -f "$f" ] && cp "$f" "$STAGE/lib/tcc/";         done
for f in "$BUILD/lib/tcc/include"/*; do [ -f "$f" ] && cp "$f" "$STAGE/lib/tcc/include/"; done
[ -f "$BUILD/bin/tcc" ] && cp "$BUILD/bin/tcc" "$STAGE/usr/bin/tcc"

# NetSurf browser runtime resources (Messages, CSS, icons) at a path baked into
# nsfb's resource search list (/usr/local/share/netsurf).
if [ -d contrib/netsurf-res ]; then
  echo "==> Staging NetSurf resources (/usr/local/share/netsurf)"
  mkdir -p "$STAGE/usr/local/share/netsurf"
  cp -R contrib/netsurf-res/* "$STAGE/usr/local/share/netsurf/"
  # TrueType faces for the stb_truetype font backend (font_stbtt.c), under unique
  # 8.3 names (kept for parity with the backend's resource names).
  install_face() { # src 8.3-name
    [ -f "tools/$1" ] && cp "tools/$1" "$STAGE/usr/local/share/netsurf/$2"
  }
  install_face DejaVuSans.ttf               SANS.TTF
  install_face DejaVuSans-Bold.ttf          SANSB.TTF
  install_face DejaVuSans-Oblique.ttf       SANSI.TTF
  install_face DejaVuSans-BoldOblique.ttf   SANSBI.TTF
  install_face DejaVuSerif.ttf              SERIF.TTF
  install_face DejaVuSerif-Bold.ttf         SERIFB.TTF
  install_face DejaVuSerif-Italic.ttf       SERIFI.TTF
  install_face DejaVuSerif-BoldItalic.ttf   SERIFBI.TTF
  install_face DejaVuSansMono.ttf           MONO.TTF
  install_face DejaVuSansMono-Bold.ttf      MONOB.TTF
  install_face DejaVuSansMono-Oblique.ttf   MONOI.TTF
  install_face DejaVuSansMono-BoldOblique.ttf MONOBI.TTF
fi

echo "==> Staging system headers (include/)"
mkdir -p "$STAGE/include/sys" "$STAGE/include/machine"
for f in include/*.h;         do [ -f "$f" ] && cp "$f" "$STAGE/include/";         done
for f in include/sys/*.h;     do [ -f "$f" ] && cp "$f" "$STAGE/include/sys/";     done
for f in include/machine/*.h; do [ -f "$f" ] && cp "$f" "$STAGE/include/machine/"; done

echo "==> Staging source examples (src/)"
mkdir -p "$STAGE/src"
for f in tools/src/*; do [ -f "$f" ] && cp "$f" "$STAGE/src/"; done

echo "==> Staging system files (etc/)"
mkdir -p "$STAGE/etc/init.d"
for f in etc/*;        do [ -f "$f" ] && cp "$f" "$STAGE/etc/";        done
for f in etc/init.d/*; do [ -f "$f" ] && cp "$f" "$STAGE/etc/init.d/"; done

echo "==> Staging DOOM WAD (optional)"
_doom_wad=""
for _candidate in "tools/doom1.wad" "$HOME/Downloads/doom1.wad"; do
    if [ -f "$_candidate" ]; then
        _doom_wad="$_candidate"
        break
    fi
done
if [ -n "$_doom_wad" ]; then
    cp "$_doom_wad" "$STAGE/bin/doom1.wad"
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

echo "==> Staging /home (user home directories from userdb)"
mkdir -p "$STAGE/home"
for entry in $USERDB_ENTRIES; do
    _user="${entry%%:*}"; _home="${entry#*:}"
    echo "    /home/$_user ($entry)"
    mkdir -p "$STAGE$_home"
    for _f in tools/skel/.*; do
        case "$_f" in */.|*/..) continue;; esac
        [ -f "$_f" ] && cp "$_f" "$STAGE$_home/"
    done
done

# Empty mount points + scratch.
mkdir -p "$STAGE/mnt" "$STAGE/tmp"

echo "==> Staging assets (var/)"
mkdir -p "$STAGE/var/log" "$STAGE/var/background" "$STAGE/var/fonts" "$STAGE/var/db"
for f in tools/backgrounds/*.bmp tools/backgrounds/*.png; do [ -f "$f" ] && cp "$f" "$STAGE/var/background/"; done
for f in tools/*.DPF; do [ -f "$f" ] && cp "$f" "$STAGE/var/fonts/"; done
for f in tools/*.ttf; do [ -f "$f" ] && cp "$f" "$STAGE/var/fonts/"; done
[ -f tools/ubistry.db ] && cp tools/ubistry.db "$STAGE/var/db/ubistry.db"

# ── Load the staged root into the UbixFS pool partition (type 0x9C).  The pool
#    IS the root filesystem — the kernel mounts it at / and execs init off it.
#    /boot (kernel + GRUB) is the only thing on FAT.  cpr each top-level tree (one
#    pool mount/sync per tree); the host `ubfs` tool is the same portable core the
#    kernel links, so the on-disk format matches byte-for-byte. ────────────────
if [ -n "${POOL_LBA:-}" ]; then
    echo "==> Building UbixFS pool root -> raw partition LBA $POOL_LBA"
    WPOOL=$(mktemp -t ubixroot).img
    "$UBFS" mkpool "$WPOOL" "${POOL_FS_MB}M" >/dev/null
    for _d in "$STAGE"/*; do
        [ -d "$_d" ] && "$UBFS" cpr "$_d" "$WPOOL:/$(basename "$_d")" >/dev/null 2>&1
    done
    # Any dotfiles staged at the root of the tree.
    for _df in "$STAGE"/.[!.]*; do
        [ -f "$_df" ] && "$UBFS" cp "$_df" "$WPOOL:/$(basename "$_df")" >/dev/null 2>&1
    done
    _mib=$(du -m "$STAGE" 2>/dev/null | tail -1 | cut -f1)
    dd if="$WPOOL" of="$IMG" bs=512 seek="$POOL_LBA" conv=notrunc 2>/dev/null
    rm -f "$WPOOL"
    echo "    Installed: complete root (~${_mib} MiB) -> UbixFS pool / (LBA $POOL_LBA)"
fi
rm -rf "$STAGE"

echo ""
echo "Done: $IMG"
echo "Run with: bmake run"
