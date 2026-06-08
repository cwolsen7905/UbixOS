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
# objGFX rendering library — needed by the views compositor + every GUI app.
[ -f "${BUILD}/lib/libobjgfx.so" ] && mcopy -i "${IMG}" "${BUILD}/lib/libobjgfx.so" ::/lib/libobjgfx.so || true

# The whole world (all dynamically-linked PIE binaries).
for b in "${BUILD}"/bin/*; do
	[ -f "${b}" ] && mcopy -i "${IMG}" "${b}" "::/bin/$(basename "${b}")"
done

# Credentials for login (root / user), matching the i386 image.
[ -f tools/userdb ] && mcopy -i "${IMG}" tools/userdb ::/etc/userdb || true

# TrueType fonts for objGFX's scalable-font backend (vlogin/taskbar/term load
# /var/fonts/DejaVuSans*.ttf) + desktop wallpapers.
mmd -i "${IMG}" ::/var ::/var/fonts ::/var/background 2>/dev/null || true
for f in tools/*.ttf; do [ -f "$f" ] && mcopy -o -i "${IMG}" "$f" ::/var/fonts/; done
for f in tools/backgrounds/*.bmp tools/backgrounds/*.png; do
	[ -f "$f" ] && mcopy -o -i "${IMG}" "$f" ::/var/background/
done

echo "mkimage-arm: done — contents:"
mdir -i "${IMG}" ::/bin | tail -n +4 | head -20
echo "  (boot with: bmake run-debug-aarch64 TARGET=aarch64)"
