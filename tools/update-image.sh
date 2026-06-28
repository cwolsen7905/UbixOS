#!/bin/sh
# update-image.sh — fast incremental refresh of an EXISTING disk image.
#
# Copies only the world binaries/libraries that changed since the last update
# straight into the UbixFS pool (in place, via tools/ubixfs/ubfs cp) — no full
# mkimage pool rebuild, and the source tree (/usr/src, staged by bmake stage-src)
# is left alone.  Driven by `bmake update` after `bmake image` has created the
# image once.
#
# NOTE: for QEMU runs the kernel is loaded via -kernel ${OBJ_DIR}/boot/kernel
# DIRECTLY, so `bmake run-*` already picks up a freshly built kernel — this script
# only needs to refresh the world binaries the pool root serves.
#
# Usage: sh tools/update-image.sh [image] [build-dir]    (ARCH/PROFILE via env)
# WARNING: do NOT run while a QEMU instance has the image open (pool corruption).
set -e

ARCH=${ARCH:-aarch64}
IMG=${1:-ubixos-arm.img}
BUILD=${2:-build/${ARCH}}
PROFILE="${PROFILE:-desktop}"
# Binaries that belong only to the desktop profile (skipped for base) — must match
# tools/mkimage.sh.
DESKTOP_BINS=" views vlogin diskutil vdoom tessera cubitaire nsfb settings nsfbtest fbtest "
UBFS=${UBFS:-tools/ubixfs/ubfs}
# Per-image stamp: the FIRST update of a given image (no stamp) full-syncs it to the
# current build, then later runs copy only what changed.  Keyed by image basename so
# different images (and a fresh 'bmake image') don't share a stale "up to date" mark.
STAMP="${BUILD}/.update-stamp-$(basename "${IMG}")"

[ -f "${IMG}" ] || { echo "update-image: ${IMG} not found — run 'bmake image' first" >&2; exit 1; }
if [ ! -x "${UBFS}" ]; then ( cd tools/ubixfs && bmake ubfs ) >/dev/null 2>&1 || true; fi
[ -x "${UBFS}" ] || { echo "update-image: ${UBFS} unavailable" >&2; exit 1; }

# Locate the UbixFS pool (MBR partition type 0x9C) byte offset within the image.
POOL_OFF=$(python3 - "${IMG}" <<'PY'
import struct, sys
with open(sys.argv[1], 'rb') as f:
    mbr = f.read(512)
for i in range(4):
    e = mbr[446 + i * 16: 446 + i * 16 + 16]
    if e[4] == 0x9c:
        print(struct.unpack('<I', e[8:12])[0] * 512)
        break
PY
)
[ -n "${POOL_OFF}" ] || { echo "update-image: no UbixFS pool (0x9C) partition in ${IMG}" >&2; exit 1; }
POOL="${IMG}@${POOL_OFF}"

# Only files newer than the last update are copied.  First run (no stamp) copies
# everything; thereafter just what `bmake world` rebuilt this round.
if [ -f "${STAMP}" ]; then NEWER="-newer ${STAMP}"; else NEWER=""; fi

echo "update-image: ${IMG} (pool @ ${POOL_OFF}, profile=${PROFILE})"
n=0
# World binary trees -> the matching on-disk path.
for sub in bin sbin usr/bin usr/sbin usr/tests; do
	[ -d "${BUILD}/${sub}" ] || continue
	for f in $(find "${BUILD}/${sub}" -maxdepth 1 -type f ${NEWER} 2>/dev/null); do
		bn=$(basename "${f}")
		case "${bn}" in *.dbg) continue ;; esac
		if [ "${PROFILE}" != desktop ]; then
			case "${DESKTOP_BINS}" in *" ${bn} "*) continue ;; esac
		fi
		"${UBFS}" cp "${f}" "${POOL}:/${sub}/${bn}" >/dev/null 2>&1 &&
			{ echo "  + /${sub}/${bn}"; n=$((n + 1)); }
	done
done
# Shared libraries (/lib/*.so).
for f in $(find "${BUILD}/lib" -maxdepth 1 -name '*.so' ${NEWER} 2>/dev/null); do
	bn=$(basename "${f}")
	"${UBFS}" cp "${f}" "${POOL}:/lib/${bn}" >/dev/null 2>&1 &&
		{ echo "  + /lib/${bn}"; n=$((n + 1)); }
done

touch "${STAMP}"
echo "update-image: refreshed ${n} file(s) (source tree untouched; kernel via -kernel)"
