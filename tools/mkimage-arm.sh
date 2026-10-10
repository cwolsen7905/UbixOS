#!/bin/sh
# mkimage-arm.sh — build a FAT32 disk image for the aarch64 (QEMU virt) port.
#
# Lays the dynamically-linked world + the musl dynamic linker into a raw FAT32
# image (BPB at sector 0, no partition table — the virtio-blk root the kernel
# mounts at "/").  Attach it with the run-aarch64 / run-debug-aarch64 targets,
# which select the modern virtio-mmio transport the driver needs.
#
# Requires mtools (brew install mtools).  Run after `bmake world TARGET=aarch64`.
#
# Usage: tools/mkimage-arm.sh [image] [build-dir]
set -e

IMG=${1:-ubixos-arm.img}
BUILD=${2:-build/aarch64}
# MBR-partitioned layout (plan K5/M4): FAT32 (fallback root + parity with i386) +
# swap + UbixFS pool (the native CoW root the kernel mounts at /).  The kernel is
# loaded by QEMU (-kernel), so FAT is not a boot partition here — it is the
# fallback root that keeps the desktop bootable while the raw-pool root beds in.
FAT_SIZE_MB=48      # FAT32 partition (type 0x0C) — fallback root (full world)
SWAP_SIZE_MB=16     # raw swap partition (type 0x82) — parity; unused on aarch64
# Native LLVM (clang + lld, built by tools/build-llvm.sh): installed into the
# root pool when its dist/ tree exists.  It is ~140 MB, so the pool defaults
# to 1200 MB then (room to build in); otherwise 96 MB.  Override either way
# with POOL_SIZE_MB=..., or LLVM_DIST=none to leave LLVM out.
LLVM_DIST=${LLVM_DIST:-${LLVM_WORK:-build/llvm}/dist}
if [ -x "${LLVM_DIST}/bin/clang" ]; then _pool_default=1200; else _pool_default=96; fi
POOL_SIZE_MB=${POOL_SIZE_MB:-${_pool_default}} # UbixFS pool partition (type 0x9C) — the raw root /
POOL_FS_MB=${POOL_FS_MB:-$((POOL_SIZE_MB - 8))} # pool filesystem size inside that partition
SIZE_MB=$(( 1 + FAT_SIZE_MB + SWAP_SIZE_MB + POOL_SIZE_MB + 4 ))

# Image profile (docs/design/console-and-arch-convergence-plan.md Phase 4):
#   desktop — full graphical stack (views + objGFX + vlogin + apps).
#   base    — headless/IoT/safe-mode: CLI world only, no compositor.  The kernel
#             branches on /bin/views being present, so a base image (no views)
#             boots straight to the text-console login.
PROFILE="${PROFILE:-desktop}"
# Binaries that belong only to the desktop profile (skipped for base).
DESKTOP_BINS=" views vlogin diskutil vdoom tessera nsfb settings nsfbtest fbtest "
echo "mkimage-arm: profile=${PROFILE}"

if [ ! -f "${BUILD}/lib/libc.so" ]; then
	echo "mkimage-arm: ${BUILD}/lib/libc.so missing — run 'bmake world TARGET=aarch64' first" >&2
	exit 1
fi

echo "mkimage-arm: creating ${SIZE_MB} MB MBR image ${IMG} (FAT ${FAT_SIZE_MB} + swap ${SWAP_SIZE_MB} + pool ${POOL_SIZE_MB} MB)"
rm -f "${IMG}"
dd if=/dev/zero of="${IMG}" bs=1m count=${SIZE_MB} 2>/dev/null

# ── MBR partition table: FAT32 (0x0C) + swap (0x82) + UbixFS pool (0x9C) ──────
# Identical scheme to the i386 image (tools/mkimage.sh) so the kernel's partition
# parser + raw pool vdev behave the same on both arches.  Stash the pool's start
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

mmd -i "${IMG}@@1M" ::/bin ::/lib ::/etc

# musl's dynamic linker IS libc.so; install it under both the INTERP path and
# the soname programs record in DT_NEEDED.
mcopy -i "${IMG}@@1M" "${BUILD}/lib/libc.so" ::/lib/libc.so
mcopy -i "${IMG}@@1M" "${BUILD}/lib/libc.so" ::/lib/ld-musl-aarch64.so.1
[ -f "${BUILD}/lib/ubix_api.so" ] && mcopy -i "${IMG}@@1M" "${BUILD}/lib/ubix_api.so" ::/lib/ubix_api.so || true
# Crypto libs for the real authd (PBKDF2 over BearSSL).
[ -f "${BUILD}/lib/libpw.so" ] && mcopy -i "${IMG}@@1M" "${BUILD}/lib/libpw.so" ::/lib/libpw.so || true
[ -f "${BUILD}/lib/libbearssl.so" ] && mcopy -i "${IMG}@@1M" "${BUILD}/lib/libbearssl.so" ::/lib/libbearssl.so || true
# Desktop-only shared libraries: objGFX (compositor + every GUI app) and the
# NetSurf stack.  The base profile ships none of them.
if [ "${PROFILE}" = desktop ]; then
	[ -f "${BUILD}/lib/libobjgfx.so" ] && mcopy -i "${IMG}@@1M" "${BUILD}/lib/libobjgfx.so" ::/lib/libobjgfx.so || true
	for _l in libcss libdom libhubbub libparserutils libwapcaplet libnsfb \
	          libnsgif libnsbmp libnsutils libutf8proc libz libhttp; do
		[ -f "${BUILD}/lib/${_l}.so" ] && mcopy -i "${IMG}@@1M" "${BUILD}/lib/${_l}.so" "::/lib/${_l}.so" || true
	done
fi

# The whole world (all dynamically-linked PIE binaries).  Skip *.dbg sidecars
# (unstripped debug copies, e.g. nsfb.dbg) — they are gdb-only and would bloat
# the image (and re-trip the kernel loader's EXEC_MAX).
for b in "${BUILD}"/bin/*; do
	case "${b}" in *.dbg) continue ;; esac
	[ -f "${b}" ] || continue
	_bn=$(basename "${b}")
	# Base profile: skip the graphical apps (their absence — chiefly /bin/views —
	# is what makes the kernel boot to the text console instead of the desktop).
	if [ "${PROFILE}" != desktop ]; then
		case "${DESKTOP_BINS}" in *" ${_bn} "*) continue ;; esac
	fi
	mcopy -i "${IMG}@@1M" "${b}" "::/bin/${_bn}"
done

# System config files (etc/), mirroring mkimage.sh.  resolv.conf is the
# critical one: musl's gethostbyname()/getaddrinfo() read /etc/resolv.conf for
# the nameserver, and with no file they fall back to 127.0.0.1 — so every
# hostname lookup (NetSurf, wget, ping <host>) fails.  Also brings the shell rc
# (csh.cshrc/csh.login), motd, termcap, fstab to parity with the i386 image.
for f in etc/*; do
	[ -f "$f" ] && mcopy -o -i "${IMG}@@1M" "$f" ::/etc/
done
if [ -d etc/init.d ]; then
	mmd -i "${IMG}@@1M" ::/etc/init.d 2>/dev/null || true
	for f in etc/init.d/*; do [ -f "$f" ] && mcopy -o -i "${IMG}@@1M" "$f" ::/etc/init.d/; done
fi

# Credentials for login (root / user), matching the i386 image.  Staged after
# etc/* so tools/userdb (the build's canonical copy) wins over any etc/userdb.
[ -f tools/userdb ] && mcopy -o -i "${IMG}@@1M" tools/userdb ::/etc/userdb || true

# --- desktop-profile assets (the base profile ships none of these) ----------
if [ "${PROFILE}" = desktop ]; then

# DOOM IWAD — vdoom defaults to /bin/doom1.wad (matches the i386 image); without
# it the game opens a black window.  Search the same locations as mkimage.sh so
# the arm image picks up a WAD in ~/Downloads even when it is not in tools/.
_doom_wad=""
for _candidate in "tools/doom1.wad" "$HOME/Downloads/doom1.wad"; do
	[ -f "$_candidate" ] && { _doom_wad="$_candidate"; break; }
done
if [ -n "$_doom_wad" ]; then
	mcopy -o -i "${IMG}@@1M" "$_doom_wad" ::/bin/doom1.wad
	echo "mkimage-arm: installed DOOM IWAD: $_doom_wad -> /bin/doom1.wad"
else
	echo "mkimage-arm: doom1.wad not found (tools/ or ~/Downloads) — vdoom will be a black window"
fi

# TrueType fonts for objGFX's scalable-font backend (vlogin/taskbar/term load
# /var/fonts/DejaVuSans*.ttf) + desktop wallpapers.
mmd -i "${IMG}@@1M" ::/var ::/var/fonts ::/var/background ::/var/log 2>/dev/null || true
for f in tools/*.ttf; do [ -f "$f" ] && mcopy -o -i "${IMG}@@1M" "$f" ::/var/fonts/; done
for f in tools/backgrounds/*.bmp tools/backgrounds/*.png; do
	[ -f "$f" ] && mcopy -o -i "${IMG}@@1M" "$f" ::/var/background/
done

# ubistry registry seed (wallpaper/theme/per-user prefs) — the daemon loads it
# from /var/db/ubistry.db at startup.
mmd -i "${IMG}@@1M" ::/var/db 2>/dev/null || true
# Regenerate the ubistry seed from its single source of truth (tools/makereg.c);
# tools/ubistry.db is a build artifact (not tracked) so the image always matches
# the committed defaults table.
_makereg="$(mktemp -t ubxmakereg.XXXXXX)" && cc -o "$_makereg" tools/makereg.c \
	&& ( cd tools && "$_makereg" >/dev/null ) && rm -f "$_makereg"
[ -f tools/ubistry.db ] && mcopy -o -i "${IMG}@@1M" tools/ubistry.db ::/var/db/ubistry.db || true

# NetSurf browser runtime resources (Messages, CSS, icons) at the path baked into
# nsfb's resource search list, plus the stb_truetype faces (8.3 names so the FAT
# driver never disambiguates colliding long-name aliases).  Mirrors mkimage.sh.
if [ -d contrib/netsurf-res ]; then
	mmd -i "${IMG}@@1M" ::/usr ::/usr/local ::/usr/local/share ::/usr/local/share/netsurf 2>/dev/null || true
	mcopy -s -i "${IMG}@@1M" contrib/netsurf-res/* ::/usr/local/share/netsurf/
	install_face() { [ -f "tools/$1" ] && mcopy -o -i "${IMG}@@1M" "tools/$1" "::/usr/local/share/netsurf/$2"; }
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
	echo "mkimage-arm: installed NetSurf resources (/usr/local/share/netsurf)"
fi

# UI icons (ubixos-v2 design), pre-rendered PNGs from share/icons/src via
# tools/mkicons.sh -> /usr/share/icons/<size>/.  The pool-root mirror below
# carries them into UbixFS with the rest of /usr.
mmd -i "${IMG}@@1M" ::/usr ::/usr/share ::/usr/share/icons 2>/dev/null || true
for _s in share/icons/*/; do
	_s=$(basename "${_s}")
	[ "${_s}" = src ] && continue
	mmd -i "${IMG}@@1M" "::/usr/share/icons/${_s}" 2>/dev/null || true
	mcopy -o -i "${IMG}@@1M" share/icons/"${_s}"/*.png "::/usr/share/icons/${_s}/"
done
echo "mkimage-arm: installed UI icons (/usr/share/icons)"

fi # end desktop-profile assets

# --- C toolchain (TCC) ---------------------------------------------------------
# tcc (bin/tcc, upstream tinycc on aarch64) compiles C inside uBixOS: it needs
# musl's headers in /usr/include, the crt startup objects in /usr/lib, and its
# own runtime (libtcc1.a + compiler headers) in /lib/tcc.  libc.so is already
# in /lib.  /usr/share/examples/hello.c is a smoke test:
#   tcc -run /usr/share/examples/hello.c
if [ -f "${BUILD}/bin/tcc" ] && [ -f "${BUILD}/lib/tcc/libtcc1.a" ]; then
	_inc=$(mktemp -d -t ubixinc)
	mkdir -p "${_inc}/bits"
	# musl install-headers order: generic bits, then arch bits, then the
	# generated ones (alltypes.h, syscall.h) win, then the public headers.
	cp contrib/musl/arch/generic/bits/*.h "${_inc}/bits/"
	cp contrib/musl/arch/aarch64/bits/*.h "${_inc}/bits/"
	cp "${BUILD}/obj/musl/obj/include/bits/"*.h "${_inc}/bits/"
	cp -R contrib/musl/include/. "${_inc}/"
	mmd -i "${IMG}@@1M" ::/usr ::/usr/include ::/usr/lib ::/usr/share ::/usr/share/examples ::/lib/tcc 2>/dev/null || true
	mcopy -s -o -i "${IMG}@@1M" "${_inc}/"* ::/usr/include/
	rm -rf "${_inc}"
	# crt objects, plus musl's empty libm/libpthread/... stubs (all of it lives
	# in libc) so the usual -lm -lpthread link flags resolve.
	for _o in crt1.o crti.o crtn.o Scrt1.o libm.a libpthread.a libdl.a librt.a libutil.a libxnet.a libcrypt.a libresolv.a; do
		[ -f "${BUILD}/obj/musl/lib/${_o}" ] && mcopy -o -i "${IMG}@@1M" "${BUILD}/obj/musl/lib/${_o}" "::/usr/lib/${_o}"
	done
	mcopy -s -o -i "${IMG}@@1M" "${BUILD}/lib/tcc/"* ::/lib/tcc/
	printf '#include <stdio.h>\n\nint main(void)\n{\n\tprintf("Hello from tcc on uBixOS!\\n");\n\treturn 0;\n}\n' >/tmp/ubix_hello.c
	mcopy -o -i "${IMG}@@1M" /tmp/ubix_hello.c ::/usr/share/examples/hello.c
	rm -f /tmp/ubix_hello.c
	echo "mkimage-arm: installed tcc toolchain (/usr/include, /usr/lib crt, /lib/tcc)"
fi

# UbixFS pool (plan K2/K3): stage a small file-backed (loopback) pool as
# /pool.img, which the kernel mounts read-write at /pool.  Built with the host
# `ubfs` CLI from the same portable core the kernel links.  Both profiles carry
# it — the native copy-on-write filesystem demo and the path off FAT.
if ( cd tools/ubixfs && bmake ubfs ) >/dev/null 2>&1 && [ -x tools/ubixfs/ubfs ]; then
	UBFS=tools/ubixfs/ubfs
	POOL=$(mktemp -t ubixpool).img
	"${UBFS}" mkpool "${POOL}" 2M >/dev/null
	printf 'Hello from a UbixFS pool!\nThis file lives in a lite-ZFS dataset.\n' >"${POOL}.hello"
	printf 'uBixOS native copy-on-write filesystem (lite-ZFS): pool + datasets,\nper-block Fletcher checksums, transaction-group commit.\n' >"${POOL}.readme"
	"${UBFS}" mkdir "${POOL}" /etc >/dev/null
	"${UBFS}" cp "${POOL}.hello"  "${POOL}:/hello.txt"   >/dev/null
	"${UBFS}" cp "${POOL}.readme" "${POOL}:/etc/readme"  >/dev/null
	mcopy -o -i "${IMG}@@1M" "${POOL}" ::/pool.img
	rm -f "${POOL}" "${POOL}.hello" "${POOL}.readme"
	echo "mkimage-arm: staged UbixFS pool at /pool.img (kernel mounts it at /pool)"
else
	echo "mkimage-arm: skipped /pool.img (host ubfs tool unavailable)"
fi

# ── UbixFS pool ROOT (plan K5/M4): mirror the finished FAT root into the raw pool
#    partition (type 0x9C).  The kernel mounts this as / via the virtio-blk MBR
#    partition device + the bcache raw vdev — the same path proven on i386.  The
#    FAT partition remains a fallback root.  Extract the FAT tree to a host dir and
#    cpr it into the pool (the host `ubfs` is the same portable core). ──────────
if [ -x "${UBFS:-}" ] && [ -n "${POOL_LBA:-}" ]; then
	echo "mkimage-arm: building UbixFS pool root (mirror of FAT root) -> raw partition LBA ${POOL_LBA}"
	WPOOL=$(mktemp -t ubixroot).img
	MIRROR=$(mktemp -d -t ubixmirror)
	"${UBFS}" mkpool "${WPOOL}" "${POOL_FS_MB}M" >/dev/null
	# Extract every top-level dir from the FAT partition (skip the loopback demo
	# pool.img — the pool is the root now, not a file inside it).
	for _d in bin lib etc usr var; do
		mcopy -s -i "${IMG}@@1M" "::/${_d}" "${MIRROR}/" 2>/dev/null || true
	done
	for _d in "${MIRROR}"/*; do
		[ -d "${_d}" ] && "${UBFS}" cpr "${_d}" "${WPOOL}:/$(basename "${_d}")" >/dev/null 2>&1
	done
	# Scratch space: compilers (tcc, clang) and mkstemp() expect /tmp.
	"${UBFS}" mkdir "${WPOOL}" /tmp >/dev/null 2>&1 || true
	# Native LLVM straight into the pool (too big for the FAT mirror).
	if [ -x "${LLVM_DIST}/bin/clang" ]; then
		for _d in "${LLVM_DIST}"/*; do
			"${UBFS}" cpr "${_d}" "${WPOOL}:/$(basename "${_d}")" >/dev/null 2>&1 ||
				echo "mkimage-arm: WARNING: installing LLVM $(basename "${_d}") failed (pool too small?)"
		done
		echo "mkimage-arm: installed LLVM clang + lld from ${LLVM_DIST} (/bin, /lib/clang, /usr/lib, /usr/include/c++)"
	fi
	_mib=$(du -m "${MIRROR}" 2>/dev/null | tail -1 | cut -f1)
	dd if="${WPOOL}" of="${IMG}" bs=512 seek="${POOL_LBA}" conv=notrunc 2>/dev/null
	rm -rf "${WPOOL}" "${MIRROR}"
	echo "mkimage-arm: installed pool root (~${_mib} MiB) -> UbixFS pool / (LBA ${POOL_LBA})"
fi

echo "mkimage-arm: done — contents:"
mdir -i "${IMG}@@1M" ::/bin | tail -n +4 | head -20
echo "  (boot with: bmake run-debug-aarch64 TARGET=aarch64)"
