#!/bin/sh
# run-uefi-x86_64.sh -- boot an x86_64 disk image through QEMU + OVMF (UEFI),
# with no -kernel: firmware -> \EFI\BOOT\BOOTX64.EFI -> kernel.
#
# Works on a PRIVATE COPY of the image in a temp directory (the original is never
# touched).  Usage:
#   tools/run-uefi-x86_64.sh [--smoke] [--inject] [--timeout SEC] [--fw CODE.fd] [--vars VARS.fd] IMAGE
# --smoke   headless; exit 0 only if the serial console reaches a login prompt
#           (regex MARKER, default '[Ll]ogin: ').
# --inject  dev loop: first copy the loader and build/ kernel onto the copy's ESP.  Without it
#           the image is booted exactly as shipped (loader + \EFI\BOOT\kernel already on it).
# Needs: qemu-system-x86_64, OVMF (tools/ci/firmware.sh finds it); mtools and the loader
# at build/x86_64/boot/BOOTX64.EFI (`bmake -C stand TARGET=x86_64`) only with --inject.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LOADER="${LOADER:-$ROOT/build/x86_64/boot/BOOTX64.EFI}"
KERNEL="${KERNEL:-$ROOT/build/x86_64/boot/kernel}"
FW="${FW:-}"
VARS="${VARS:-}"
SMOKE=0
INJECT=0
MARKER="${MARKER:-[Ll]ogin: }"
TIMEOUT=240
MEM=512

while [ $# -gt 0 ]; do
	case "$1" in
	--smoke) SMOKE=1 ;;
	--inject) INJECT=1 ;;
	--timeout) TIMEOUT=$2; shift ;;
	--fw) FW=$2; shift ;;
	--vars) VARS=$2; shift ;;
	-*) echo "unknown option $1" >&2; exit 2 ;;
	*) IMG=$1 ;;
	esac
	shift
done
[ -n "${IMG:-}" ] && [ -f "$IMG" ] || { echo "usage: $0 [--smoke] [--inject] IMAGE" >&2; exit 2; }
[ "$INJECT" = 0 ] || [ -f "$LOADER" ] || { echo "missing $LOADER (bmake -C stand TARGET=x86_64)" >&2; exit 2; }
if [ -z "$FW" ] || [ -z "$VARS" ]; then
	set -- $("$ROOT/tools/ci/firmware.sh" x86_64) || exit 2
	[ -n "$FW" ] || FW=$1
	[ -n "$VARS" ] || VARS=${2:-}
fi
[ -f "$FW" ] || { echo "missing firmware $FW" >&2; exit 2; }
[ -f "$VARS" ] || { echo "missing OVMF vars template '$VARS' (--vars)" >&2; exit 2; }

WORK=$(mktemp -d "${TMPDIR:-/tmp}/ubix-uefi.XXXXXX")
trap 'kill $QPID 2>/dev/null || true; rm -rf "$WORK"' EXIT INT TERM
QPID=
cp "$IMG" "$WORK/disk.img"
cp "$VARS" "$WORK/vars.fd"

if [ "$INJECT" = 1 ]; then
	# Install the loader at the removable-media path on the ESP (partition 1 at 1 MB).
	mmd -i "$WORK/disk.img@@1M" ::/EFI ::/EFI/BOOT 2>/dev/null || true
	mcopy -o -i "$WORK/disk.img@@1M" "$LOADER" ::/EFI/BOOT/BOOTX64.EFI
	# Refresh the kernel beside it so a rebuilt kernel is tested without a new image.
	[ -f "$KERNEL" ] && mcopy -o -i "$WORK/disk.img@@1M" "$KERNEL" ::/EFI/BOOT/kernel
fi

set -- qemu-system-x86_64 -m "$MEM" -cpu qemu64,+x2apic \
	-drive if=pflash,format=raw,readonly=on,file="$FW" \
	-drive if=pflash,format=raw,file="$WORK/vars.fd" \
	-drive file="$WORK/disk.img",format=raw,if=none,id=hd0 \
	-device virtio-blk-pci,drive=hd0,disable-modern=true \
	-vga std -no-reboot

if [ "$SMOKE" = 1 ]; then
	"$@" -display none -serial file:"$WORK/serial.log" -monitor none &
	QPID=$!
	i=0
	while [ $i -lt "$TIMEOUT" ]; do
		if grep -Eq "$MARKER" "$WORK/serial.log" 2>/dev/null; then
			echo "uefi-smoke: PASS (login prompt reached)"
			exit 0
		fi
		kill -0 $QPID 2>/dev/null || break
		sleep 1
		i=$((i + 1))
	done
	echo "uefi-smoke: FAIL (no login prompt); serial tail:" >&2
	tail -n 40 "$WORK/serial.log" >&2
	exit 1
fi
exec "$@" -serial mon:stdio
