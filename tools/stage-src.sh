#!/bin/sh
# stage-src.sh — stage the uBixOS source tree into a built image's UbixFS pool as
# a FreeBSD-style /usr/src, plus an empty /usr/obj/<arch> (build output) and an
# installed /usr/share/mk.  This is what lets `bmake` run from /usr/src on-device
# (see docs/design/usr-src-build-layout-plan.md).
#
# Standalone by design: it does NOT edit mkimage.sh.  Run it AFTER `bmake image`:
#   sh tools/stage-src.sh [image] [arch]
#   bmake stage-src                       (Makefile wrapper)
#
# It locates the pool by parsing the image MBR for the UbixFS partition (type
# 0x9C), so it stays correct regardless of mkimage's partition sizing.
#
# WARNING: staging the full tree is a ~100s+ ubfs cpr run, and ubfs cpr is NOT
# crash-atomic — interrupting it (SIGTERM/timeout/Ctrl-C) corrupts the pool
# metadata.  Let it finish; do not wrap it in a short timeout.  If the pool is
# damaged, regenerate the image with `bmake image` and re-run this.
set -e

IMG=${1:-ubixos-arm.img}
ARCH=${2:-aarch64}
SRCTOP=$(cd "$(dirname "$0")/.." && pwd)
UBFS=${UBFS:-${SRCTOP}/tools/ubixfs/ubfs}

[ -f "${IMG}" ] || { echo "stage-src: image not found: ${IMG}" >&2; exit 1; }
if [ ! -x "${UBFS}" ]; then
	( cd "${SRCTOP}/tools/ubixfs" && bmake ubfs ) >/dev/null 2>&1
fi
[ -x "${UBFS}" ] || { echo "stage-src: host ubfs tool unavailable" >&2; exit 1; }

# Byte offset of the UbixFS pool partition (MBR type 0x9C) within the image.
POOL_OFF=$(python3 - "${IMG}" <<'PY'
import sys, struct
with open(sys.argv[1], 'rb') as f:
    mbr = f.read(512)
for i in range(4):
    e = mbr[446 + i * 16: 446 + (i + 1) * 16]
    if e[4] == 0x9C:                       # partition type 0x9C = UbixFS pool
        print(struct.unpack('<I', e[8:12])[0] * 512)
        break
PY
)
[ -n "${POOL_OFF}" ] || { echo "stage-src: no UbixFS pool (0x9C) partition in ${IMG}" >&2; exit 1; }
POOL="${IMG}@${POOL_OFF}"
echo "stage-src: UbixFS pool at byte ${POOL_OFF} in ${IMG}"

# From here the staging is best-effort per entry: a single cpr returning non-zero
# (e.g. a skipped symlink) must not abort the whole tree, so warn and continue.
set +e

# /usr/src — the whole source tree.  The shell glob skips dotfiles (so .git is
# excluded automatically); we additionally skip build/ (the host object tree) and
# transient build artifacts (serial.log).
"${UBFS}" mkdir "${POOL}" /usr/src >/dev/null 2>&1
for p in "${SRCTOP}"/*; do
	b=$(basename "${p}")
	# Skip the host object tree + build/image artifacts that live in the repo root.
	case "${b}" in build|serial.log|*.core|*.img|*.iso|*.log) continue;; esac
	if [ -d "${p}" ]; then
		"${UBFS}" cpr "${p}" "${POOL}:/usr/src/${b}" >/dev/null 2>&1 ||
			echo "stage-src: warn: failed to stage dir ${b}"
	elif [ -f "${p}" ]; then
		"${UBFS}" cp "${p}" "${POOL}:/usr/src/${b}" >/dev/null 2>&1 ||
			echo "stage-src: warn: failed to stage file ${b}"
	fi
done

# /usr/obj/<arch> — writable build output dir (OBJ_DIR target on-device).
"${UBFS}" mkdir "${POOL}" /usr/obj/${ARCH} >/dev/null 2>&1
# Pre-seed the GENERATED musl headers (bits/alltypes.h, bits/syscall.h, …) at the
# path the world build's -I expects (${OBJ_DIR}/obj/musl/obj/include).  These are
# produced by musl's configure/build on the host and are NOT under /usr/src, so an
# on-device compile would fail "bits/alltypes.h file not found" without them.
_GEN="${SRCTOP}/build/${ARCH}/obj/musl/obj/include"
if [ -d "${_GEN}" ]; then
	"${UBFS}" mkdir "${POOL}" /usr/obj/${ARCH}/obj >/dev/null 2>&1
	"${UBFS}" mkdir "${POOL}" /usr/obj/${ARCH}/obj/musl >/dev/null 2>&1
	"${UBFS}" mkdir "${POOL}" /usr/obj/${ARCH}/obj/musl/obj >/dev/null 2>&1
	"${UBFS}" cpr "${_GEN}" "${POOL}:/usr/obj/${ARCH}/obj/musl/obj/include" >/dev/null 2>&1 ||
		echo "stage-src: warn: failed to stage generated musl headers"
fi

# /usr/share/mk — installed make macros (FreeBSD convention; UBIX_MK can point here).
"${UBFS}" cpr "${SRCTOP}/share/mk" "${POOL}:/usr/share/mk" >/dev/null 2>&1 ||
	echo "stage-src: warn: failed to stage /usr/share/mk"

echo "stage-src: staged /usr/src + /usr/obj/${ARCH} + /usr/share/mk"
echo "stage-src: /usr/src contents:"
"${UBFS}" ls "${POOL}" /usr/src 2>/dev/null | head -30
