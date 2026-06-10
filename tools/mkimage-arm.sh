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
SIZE_MB=64

# Image profile (docs/design/console-and-arch-convergence-plan.md Phase 4):
#   desktop — full graphical stack (views + objGFX + vlogin + apps).
#   base    — headless/IoT/safe-mode: CLI world only, no compositor.  The kernel
#             branches on /bin/views being present, so a base image (no views)
#             boots straight to the text-console login.
PROFILE="${PROFILE:-desktop}"
# Binaries that belong only to the desktop profile (skipped for base).
DESKTOP_BINS=" views vlogin vdoom tessera nsfb settings nsfbtest fbtest "
echo "mkimage-arm: profile=${PROFILE}"

if [ ! -f "${BUILD}/lib/libc.so" ]; then
	echo "mkimage-arm: ${BUILD}/lib/libc.so missing — run 'bmake world TARGET=aarch64' first" >&2
	exit 1
fi

echo "mkimage-arm: creating ${SIZE_MB} MB FAT32 image ${IMG}"
rm -f "${IMG}"
dd if=/dev/zero of="${IMG}" bs=1m count=${SIZE_MB} 2>/dev/null
mformat -i "${IMG}" -F ::

mmd -i "${IMG}" ::/bin ::/lib ::/etc

# musl's dynamic linker IS libc.so; install it under both the INTERP path and
# the soname programs record in DT_NEEDED.
mcopy -i "${IMG}" "${BUILD}/lib/libc.so" ::/lib/libc.so
mcopy -i "${IMG}" "${BUILD}/lib/libc.so" ::/lib/ld-musl-aarch64.so.1
[ -f "${BUILD}/lib/ubix_api.so" ] && mcopy -i "${IMG}" "${BUILD}/lib/ubix_api.so" ::/lib/ubix_api.so || true
# Crypto libs for the real authd (PBKDF2 over BearSSL).
[ -f "${BUILD}/lib/libpw.so" ] && mcopy -i "${IMG}" "${BUILD}/lib/libpw.so" ::/lib/libpw.so || true
[ -f "${BUILD}/lib/libbearssl.so" ] && mcopy -i "${IMG}" "${BUILD}/lib/libbearssl.so" ::/lib/libbearssl.so || true
# Desktop-only shared libraries: objGFX (compositor + every GUI app) and the
# NetSurf stack.  The base profile ships none of them.
if [ "${PROFILE}" = desktop ]; then
	[ -f "${BUILD}/lib/libobjgfx.so" ] && mcopy -i "${IMG}" "${BUILD}/lib/libobjgfx.so" ::/lib/libobjgfx.so || true
	for _l in libcss libdom libhubbub libparserutils libwapcaplet libnsfb \
	          libnsgif libnsbmp libnsutils libutf8proc libz libhttp; do
		[ -f "${BUILD}/lib/${_l}.so" ] && mcopy -i "${IMG}" "${BUILD}/lib/${_l}.so" "::/lib/${_l}.so" || true
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
	mcopy -i "${IMG}" "${b}" "::/bin/${_bn}"
done

# System config files (etc/), mirroring mkimage.sh.  resolv.conf is the
# critical one: musl's gethostbyname()/getaddrinfo() read /etc/resolv.conf for
# the nameserver, and with no file they fall back to 127.0.0.1 — so every
# hostname lookup (NetSurf, wget, ping <host>) fails.  Also brings the shell rc
# (csh.cshrc/csh.login), motd, termcap, fstab to parity with the i386 image.
for f in etc/*; do
	[ -f "$f" ] && mcopy -o -i "${IMG}" "$f" ::/etc/
done
if [ -d etc/init.d ]; then
	mmd -i "${IMG}" ::/etc/init.d 2>/dev/null || true
	for f in etc/init.d/*; do [ -f "$f" ] && mcopy -o -i "${IMG}" "$f" ::/etc/init.d/; done
fi

# Credentials for login (root / user), matching the i386 image.  Staged after
# etc/* so tools/userdb (the build's canonical copy) wins over any etc/userdb.
[ -f tools/userdb ] && mcopy -o -i "${IMG}" tools/userdb ::/etc/userdb || true

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
	mcopy -o -i "${IMG}" "$_doom_wad" ::/bin/doom1.wad
	echo "mkimage-arm: installed DOOM IWAD: $_doom_wad -> /bin/doom1.wad"
else
	echo "mkimage-arm: doom1.wad not found (tools/ or ~/Downloads) — vdoom will be a black window"
fi

# TrueType fonts for objGFX's scalable-font backend (vlogin/taskbar/term load
# /var/fonts/DejaVuSans*.ttf) + desktop wallpapers.
mmd -i "${IMG}" ::/var ::/var/fonts ::/var/background 2>/dev/null || true
for f in tools/*.ttf; do [ -f "$f" ] && mcopy -o -i "${IMG}" "$f" ::/var/fonts/; done
for f in tools/backgrounds/*.bmp tools/backgrounds/*.png; do
	[ -f "$f" ] && mcopy -o -i "${IMG}" "$f" ::/var/background/
done

# ubistry registry seed (wallpaper/theme/per-user prefs) — the daemon loads it
# from /var/db/ubistry.db at startup.
mmd -i "${IMG}" ::/var/db 2>/dev/null || true
[ -f tools/ubistry.db ] && mcopy -o -i "${IMG}" tools/ubistry.db ::/var/db/ubistry.db || true

# NetSurf browser runtime resources (Messages, CSS, icons) at the path baked into
# nsfb's resource search list, plus the stb_truetype faces (8.3 names so the FAT
# driver never disambiguates colliding long-name aliases).  Mirrors mkimage.sh.
if [ -d contrib/netsurf-res ]; then
	mmd -i "${IMG}" ::/usr ::/usr/local ::/usr/local/share ::/usr/local/share/netsurf 2>/dev/null || true
	mcopy -s -i "${IMG}" contrib/netsurf-res/* ::/usr/local/share/netsurf/
	install_face() { [ -f "tools/$1" ] && mcopy -o -i "${IMG}" "tools/$1" "::/usr/local/share/netsurf/$2"; }
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

fi # end desktop-profile assets

echo "mkimage-arm: done — contents:"
mdir -i "${IMG}" ::/bin | tail -n +4 | head -20
echo "  (boot with: bmake run-debug-aarch64 TARGET=aarch64)"
