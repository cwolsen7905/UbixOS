#!/bin/sh
# mkimage.sh — build an MBR disk image for the 64-bit ports (aarch64 + x86_64).
#
# Partition layout (shared by the two 64-bit arches this script serves):
#   FAT32 (type 0x0C)  — BOOT ONLY.  Carries the kernel for GRUB-style boot.
#                        aarch64/x86_64 here are loaded by QEMU (-kernel), so in
#                        practice the FAT holds only /boot/kernel/kernel (and is
#                        empty if no kernel was built).  It is NO LONGER a
#                        fallback world root — the UbixFS pool is the sole
#                        content store now (kernel boot.c prefers the pool as /).
#   swap  (type 0x82)  — raw swap.
#   UbixFS pool (0x9C) — the native CoW root the kernel mounts at "/".  Built
#                        DIRECTLY from build/${ARCH}/ + the asset sources; it is
#                        no longer mirrored out of the FAT.
#
# Requires mtools (brew install mtools).  Run after `bmake world TARGET=aarch64`.
#
# Usage: tools/mkimage.sh [image] [build-dir]
set -e

# ARCH selects the world build dir + the musl dynamic-linker soname.  Defaults to
# aarch64 (this script's original target); the x86_64 port reuses the identical
# layout (PVH -kernel + virtio-blk), passing ARCH=x86_64.
ARCH=${ARCH:-aarch64}
IMG=${1:-ubixos-arm.img}
BUILD=${2:-build/${ARCH}}
# MBR-partitioned layout: FAT32 (boot only) + swap + UbixFS pool (the native CoW
# root the kernel mounts at /).  The kernel is loaded by QEMU (-kernel), so FAT
# is not a boot partition here — it is boot-only parity for the GRUB path.
FAT_SIZE_MB=64      # FAT32 partition (type 0x0C) — boot only (kernel for GRUB)
SWAP_SIZE_MB=256    # raw swap partition (type 0x82)
POOL_SIZE_MB=3080   # UbixFS pool partition (type 0x9C) — the raw root /
POOL_FS_MB=3072     # pool filesystem size inside that partition (self-hosting: world + src + tools)
SIZE_MB=$(( 1 + FAT_SIZE_MB + SWAP_SIZE_MB + POOL_SIZE_MB + 4 ))

# Image profile (docs/design/console-and-arch-convergence-plan.md Phase 4):
#   desktop — full graphical stack (views + objGFX + vlogin + apps).
#   base    — headless/IoT/safe-mode: CLI world only, no compositor.  The kernel
#             branches on /bin/views being present, so a base image (no views)
#             boots straight to the text-console login.
PROFILE="${PROFILE:-desktop}"
# Binaries that belong only to the desktop profile (skipped for base).
DESKTOP_BINS=" views vlogin diskutil vdoom tessera cubitaire nsfb settings nsfbtest fbtest "
echo "mkimage: profile=${PROFILE}"

if [ ! -f "${BUILD}/lib/libc.so" ]; then
	echo "mkimage: ${BUILD}/lib/libc.so missing — run 'bmake world TARGET=${ARCH}' first" >&2
	exit 1
fi

echo "mkimage: creating ${SIZE_MB} MB MBR image ${IMG} (FAT ${FAT_SIZE_MB} + swap ${SWAP_SIZE_MB} + pool ${POOL_SIZE_MB} MB)"
rm -f "${IMG}"
# Sparse allocation: count=0 seek=N writes no data but extends the file to N MiB.
# Blocks materialize only where the later steps actually write (MBR, FAT, pool),
# so the empty pool tail stays a hole — far less IO than zeroing the whole image.
# (Sparse files require APFS; HFS+ would silently allocate the full size.)
dd if=/dev/zero of="${IMG}" bs=1m count=0 seek=${SIZE_MB} 2>/dev/null

# ── MBR partition table: FAT32 (0x0C) + swap (0x82) + UbixFS pool (0x9C) ──────
# Identical scheme to the legacy i386 image (frozen on releng/2) so the kernel's
# partition parser + raw pool vdev behave the same on both arches.  Stash the pool's start
# LBA for the dd-into-partition step below.
python3 - "${IMG}" "${FAT_SIZE_MB}" "${SWAP_SIZE_MB}" "${POOL_SIZE_MB}" <<'PYEOF'
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
    0x80, *lba_to_chs(fat_start),  0x0C, *lba_to_chs(fat_end),  fat_start,  fat_sectors)
swap_entry = struct.pack('<BBBBBBBBII',
    0x00, *lba_to_chs(swap_start), 0x82, *lba_to_chs(swap_end),  swap_start, swap_end - swap_start + 1)
pool_entry = struct.pack('<BBBBBBBBII',
    0x00, *lba_to_chs(pool_start), 0x9C, *lba_to_chs(pool_end),  pool_start, pool_end - pool_start + 1)
with open(img_path, 'r+b') as f:
    f.seek(446)
    f.write(fat_entry); f.write(swap_entry); f.write(pool_entry)
    f.write(b'\x00' * 16)   # slot 3 unused
    f.write(b'\x55\xAA')
with open(img_path + '.poollba', 'w') as g:
    g.write(str(pool_start))
print(f"  FAT32: LBA {fat_start}-{fat_end} ({fat_size_mb} MB, type 0x0C)")
print(f"  Swap:  LBA {swap_start}-{swap_end} (type 0x82)")
print(f"  Pool:  LBA {pool_start}-{pool_end} (type 0x9C)")
PYEOF
POOL_LBA=$(cat "${IMG}.poollba"); rm -f "${IMG}.poollba"

# Format the FAT32 partition (partition 1 begins at LBA 2048 = 1 MB).
mformat -i "${IMG}@@1M" -F ::

# ── FAT is boot-only ─────────────────────────────────────────────────────────
# Carry just the kernel for the GRUB-style boot path.  aarch64/x86_64 here load
# the kernel via QEMU -kernel, so this is parity/future-GRUB; the FAT holds
# nothing else (the world lives entirely in the UbixFS pool below).
if [ -f "${BUILD}/boot/kernel" ]; then
	mmd -i "${IMG}@@1M" ::/boot ::/boot/kernel 2>/dev/null || true
	mcopy -o -i "${IMG}@@1M" "${BUILD}/boot/kernel" ::/boot/kernel/kernel
	echo "mkimage: FAT (boot-only) carries /boot/kernel/kernel"
else
	echo "mkimage: FAT left empty (no ${BUILD}/boot/kernel; kernel loaded via QEMU -kernel)"
fi

# ── Build the root tree on the host, then lay it into the UbixFS pool ─────────
# STAGE mirrors the on-disk root layout exactly; `ubfs cpr` copies each top-level
# dir into the pool in one mount/commit cycle.  cpr skips symlinks, so the musl
# dynamic linker is installed as a real second copy (not a symlink).
STAGE=$(mktemp -d -t ubixstage)
mkdir -p "${STAGE}"/bin "${STAGE}"/lib "${STAGE}"/sbin \
         "${STAGE}"/usr/bin "${STAGE}"/usr/sbin "${STAGE}"/usr/lib \
         "${STAGE}"/etc "${STAGE}"/etc/init.d \
         "${STAGE}"/var/log

# libc.so IS musl's dynamic linker; install it under the DT_NEEDED soname and
# expose the INTERP path (/lib/ld-musl-${ARCH}.so.1) as a symlink to it.  cpr
# preserves the symlink into the pool, and the kernel's ubixfs resolver follows
# it (relative target, resolved against /lib) when loading the program interp.
cp "${BUILD}/lib/libc.so" "${STAGE}/lib/libc.so"
ln -s libc.so "${STAGE}/lib/ld-musl-${ARCH}.so.1"
for _l in libubix_api libpw libbearssl; do
	[ -f "${BUILD}/lib/${_l}.so" ] && cp "${BUILD}/lib/${_l}.so" "${STAGE}/lib/${_l}.so" || true
done
# Desktop-only shared libraries: objGFX (compositor + every GUI app) and the
# NetSurf stack.  The base profile ships none of them.
if [ "${PROFILE}" = desktop ]; then
	for _l in libobjgfx libcss libdom libhubbub libparserutils libwapcaplet \
	          libnsfb libnsgif libnsbmp libnsutils libutf8proc libz libhttp; do
		[ -f "${BUILD}/lib/${_l}.so" ] && cp "${BUILD}/lib/${_l}.so" "${STAGE}/lib/${_l}.so" || true
	done
fi

# The whole world (all dynamically-linked PIE binaries).  Skip *.dbg sidecars
# (unstripped debug copies) and, for the base profile, the graphical apps.
for b in "${BUILD}"/bin/*; do
	case "${b}" in *.dbg) continue ;; esac
	[ -f "${b}" ] || continue
	_bn=$(basename "${b}")
	if [ "${PROFILE}" != desktop ]; then
		case "${DESKTOP_BINS}" in *" ${_bn} "*) continue ;; esac
	fi
	cp "${b}" "${STAGE}/bin/${_bn}"
done

# /usr-hierarchy: copy build/${ARCH}/{sbin,usr/bin,usr/sbin,usr/lib} verbatim.
for _sub in sbin usr/bin usr/sbin usr/lib; do
	[ -d "${BUILD}/${_sub}" ] || continue
	for b in "${BUILD}/${_sub}"/*; do
		case "${b}" in *.dbg) continue ;; esac
		[ -f "${b}" ] || continue
		cp "${b}" "${STAGE}/${_sub}/$(basename "${b}")"
	done
done

# System config files (etc/).  resolv.conf is the critical
# one (musl reads it for the nameserver); also csh.cshrc/csh.login, motd,
# termcap, fstab to parity with the i386 image.
for f in etc/*; do
	[ -f "$f" ] && cp "$f" "${STAGE}/etc/"
done
if [ -d etc/init.d ]; then
	for f in etc/init.d/*; do [ -f "$f" ] && cp "$f" "${STAGE}/etc/init.d/"; done
fi
# Credentials for login (root / user).  Staged last so tools/userdb (the build's
# canonical copy) wins over any etc/userdb.
[ -f tools/userdb ] && cp tools/userdb "${STAGE}/etc/userdb" || true

# --- desktop-profile assets (the base profile ships none of these) ----------
if [ "${PROFILE}" = desktop ]; then
	mkdir -p "${STAGE}"/var/fonts "${STAGE}"/var/background "${STAGE}"/var/db \
	         "${STAGE}"/usr/local/share/netsurf

	# DOOM IWAD — vdoom defaults to /bin/doom1.wad.  Search tools/ then ~/Downloads.
	_doom_wad=""
	for _candidate in "tools/doom1.wad" "$HOME/Downloads/doom1.wad"; do
		[ -f "$_candidate" ] && { _doom_wad="$_candidate"; break; }
	done
	if [ -n "$_doom_wad" ]; then
		cp "$_doom_wad" "${STAGE}/bin/doom1.wad"
		echo "mkimage: installed DOOM IWAD: $_doom_wad -> /bin/doom1.wad"
	else
		echo "mkimage: doom1.wad not found (tools/ or ~/Downloads) — vdoom will be a black window"
	fi

	# TrueType fonts for objGFX's scalable-font backend + desktop wallpapers.
	for f in tools/*.ttf; do [ -f "$f" ] && cp "$f" "${STAGE}/var/fonts/"; done
	for f in tools/backgrounds/*.bmp tools/backgrounds/*.png; do
		[ -f "$f" ] && cp "$f" "${STAGE}/var/background/"
	done

	# ubistry registry seed.  Regenerate from its single source of truth
	# (tools/makereg.c); tools/ubistry.db is a build artifact (not tracked).
	_makereg="$(mktemp -t ubxmakereg.XXXXXX)" && cc -o "$_makereg" tools/makereg.c \
		&& ( cd tools && "$_makereg" >/dev/null ) && rm -f "$_makereg"
	[ -f tools/ubistry.db ] && cp tools/ubistry.db "${STAGE}/var/db/ubistry.db" || true

	# NetSurf browser runtime resources + the stb_truetype faces (8.3 names so the
	# FAT driver never disambiguates colliding long-name aliases).
	if [ -d contrib/netsurf-res ]; then
		cp -R contrib/netsurf-res/. "${STAGE}/usr/local/share/netsurf/"
		install_face() { [ -f "tools/$1" ] && cp "tools/$1" "${STAGE}/usr/local/share/netsurf/$2"; }
		install_face DejaVuSans.ttf                 SANS.TTF
		install_face DejaVuSans-Bold.ttf            SANSB.TTF
		install_face DejaVuSans-Oblique.ttf         SANSI.TTF
		install_face DejaVuSans-BoldOblique.ttf     SANSBI.TTF
		install_face DejaVuSerif.ttf                SERIF.TTF
		install_face DejaVuSerif-Bold.ttf           SERIFB.TTF
		install_face DejaVuSerif-Italic.ttf         SERIFI.TTF
		install_face DejaVuSerif-BoldItalic.ttf     SERIFBI.TTF
		install_face DejaVuSansMono.ttf             MONO.TTF
		install_face DejaVuSansMono-Bold.ttf        MONOB.TTF
		install_face DejaVuSansMono-Oblique.ttf     MONOI.TTF
		install_face DejaVuSansMono-BoldOblique.ttf MONOBI.TTF
		echo "mkimage: staged NetSurf resources (/usr/local/share/netsurf)"
	fi
fi # end desktop-profile assets

# ── UbixFS pool ROOT (plan K5/M4): build the pool directly from STAGE and write
#    it into the raw pool partition (type 0x9C).  The kernel mounts this as / via
#    the virtio-blk MBR partition device + the bcache raw vdev — the same path
#    proven on i386.  The host `ubfs` is the same portable core. ───────────────
if ( cd tools/ubixfs && bmake ubfs ) >/dev/null 2>&1 && [ -x tools/ubixfs/ubfs ]; then
	UBFS=tools/ubixfs/ubfs
	# Author the pool DIRECTLY inside its partition via ubfs's '@<offset>' suffix —
	# no temp pool file, no multi-GB dd splice.  ubfs writes only the real metadata
	# + file blocks at byte (POOL_LBA × 512); the rest of the partition stays a hole,
	# so the image stays sparse and the build moves megabytes, not gigabytes.
	POOL_OFF=$(( POOL_LBA * 512 ))
	echo "mkimage: building UbixFS pool root (${POOL_FS_MB} MB) in place at LBA ${POOL_LBA} (byte ${POOL_OFF})"
	"${UBFS}" mkpool "${IMG}@${POOL_OFF}" "${POOL_FS_MB}M" >/dev/null
	for _d in "${STAGE}"/*; do
		[ -d "${_d}" ] && "${UBFS}" cpr "${_d}" "${IMG}@${POOL_OFF}:/$(basename "${_d}")" >/dev/null 2>&1
	done
	_mib=$(du -m "${STAGE}" 2>/dev/null | tail -1 | cut -f1)
	echo "mkimage: installed pool root (~${_mib} MiB) -> UbixFS pool / (LBA ${POOL_LBA})"
	echo "mkimage: done — pool /bin contents:"
	"${UBFS}" ls "${IMG}@${POOL_OFF}" /bin 2>/dev/null | head -20
else
	echo "mkimage: ERROR — host ubfs tool unavailable; pool root not built" >&2
	rm -rf "${STAGE}"
	exit 1
fi

rm -rf "${STAGE}"
echo "  (boot with: bmake run-debug-aarch64 TARGET=aarch64)"
