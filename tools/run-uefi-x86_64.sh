#!/bin/sh
# run-uefi-x86_64.sh -- boot an x86_64 disk image through QEMU + OVMF (UEFI),
# with no -kernel: firmware -> \EFI\BOOT\BOOTX64.EFI -> kernel.
#
# Works on a PRIVATE COPY of the image in a temp directory (the original is never
# touched).  Usage:
#   tools/run-uefi-x86_64.sh [--smoke] [--timeout SEC] [--fw CODE.fd] [--vars VARS.fd] IMAGE
# --smoke   headless; exit 0 only if the serial console reaches a login prompt.
# Needs: qemu-system-x86_64, mtools, the loader at build/x86_64/boot/BOOTX64.EFI
# (`bmake -C stand TARGET=x86_64`).
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LOADER="${LOADER:-$ROOT/build/x86_64/boot/BOOTX64.EFI}"
QEMU_SHARE="${QEMU_SHARE:-$(dirname "$(command -v qemu-system-x86_64)")/../share/qemu}"
KERNEL="${KERNEL:-$ROOT/build/x86_64/boot/kernel}"
FW="${FW:-$QEMU_SHARE/edk2-x86_64-code.fd}"
VARS="${VARS:-$QEMU_SHARE/edk2-i386-vars.fd}"
SMOKE=0
TIMEOUT=240
MEM=512

while [ $# -gt 0 ]; do
	case "$1" in
	--smoke) SMOKE=1 ;;
	--timeout) TIMEOUT=$2; shift ;;
	--fw) FW=$2; shift ;;
	--vars) VARS=$2; shift ;;
	-*) echo "unknown option $1" >&2; exit 2 ;;
	*) IMG=$1 ;;
	esac
	shift
done
[ -n "${IMG:-}" ] && [ -f "$IMG" ] || { echo "usage: $0 [--smoke] IMAGE" >&2; exit 2; }
[ -f "$LOADER" ] || { echo "missing $LOADER (bmake -C stand TARGET=x86_64)" >&2; exit 2; }
[ -f "$FW" ] || { echo "missing OVMF firmware $FW" >&2; exit 2; }

WORK=$(mktemp -d "${TMPDIR:-/tmp}/ubix-uefi.XXXXXX")
trap 'kill $QPID 2>/dev/null || true; rm -rf "$WORK"' EXIT INT TERM
QPID=
cp "$IMG" "$WORK/disk.img"
cp "$VARS" "$WORK/vars.fd"

# Install the loader at the removable-media path on the ESP (partition 1 at 1 MB).
mmd -i "$WORK/disk.img@@1M" ::/EFI ::/EFI/BOOT 2>/dev/null || true
mcopy -o -i "$WORK/disk.img@@1M" "$LOADER" ::/EFI/BOOT/BOOTX64.EFI

# Refresh the kernel on the copy so a rebuilt kernel is tested without a new image.
if [ -f "$KERNEL" ]; then
	mmd -i "$WORK/disk.img@@1M" ::/boot ::/boot/kernel 2>/dev/null || true
	mcopy -o -i "$WORK/disk.img@@1M" "$KERNEL" ::/boot/kernel/kernel
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
		if grep -q "login:" "$WORK/serial.log" 2>/dev/null; then
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
