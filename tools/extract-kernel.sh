#!/bin/sh
# extract-kernel.sh — pull the kernel that uBixOS built ON-DEVICE back out to the
# host, so QEMU can boot it via -kernel.
#
# The on-device `bmake kernel OBJ_DIR=/usr/obj/${ARCH}` writes its ELF to
# /usr/obj/${ARCH}/boot/kernel INSIDE the UbixFS pool — it never touches the host
# FS.  This script copies that ELF out of the pool to a host file (default
# build/${ARCH}/boot/kernel.selfbuilt) using `ubfs cp <img>:<path> <host>`.
#
# Usage: sh tools/extract-kernel.sh [image] [out-file]   (ARCH via env, default aarch64)
set -e

ARCH=${ARCH:-aarch64}
IMG=${1:-ubixos-arm.img}
OUT=${2:-build/${ARCH}/boot/kernel.selfbuilt}
SRC=${SRC:-/usr/obj/${ARCH}/boot/kernel}
UBFS=${UBFS:-tools/ubixfs/ubfs}

[ -f "${IMG}" ] || { echo "extract-kernel: ${IMG} not found" >&2; exit 1; }
if [ ! -x "${UBFS}" ]; then ( cd tools/ubixfs && bmake ubfs ) >/dev/null 2>&1 || true; fi
[ -x "${UBFS}" ] || { echo "extract-kernel: ${UBFS} unavailable" >&2; exit 1; }

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
[ -n "${POOL_OFF}" ] || { echo "extract-kernel: no UbixFS pool (0x9C) partition in ${IMG}" >&2; exit 1; }

mkdir -p "$(dirname "${OUT}")"
"${UBFS}" cp "${IMG}@${POOL_OFF}:${SRC}" "${OUT}"
sz=$(wc -c < "${OUT}" | tr -d ' ')
echo "extract-kernel: ${IMG}${SRC} (pool @ ${POOL_OFF}) -> ${OUT} (${sz} bytes)"
[ "${sz}" -gt 4096 ] || { echo "extract-kernel: WARNING extracted kernel is suspiciously small (${sz} B) — did the on-device build run?" >&2; }
