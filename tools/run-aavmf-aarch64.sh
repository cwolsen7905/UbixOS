#!/bin/sh
# run-aavmf-aarch64.sh -- boot an aarch64 disk image through QEMU virt + AAVMF
# (edk2 UEFI), with no -kernel: firmware -> \EFI\BOOT\BOOTAA64.EFI -> kernel.
#
# Works on a PRIVATE COPY of the image in a temp directory (the original is never
# touched).  Usage:
#   tools/run-aavmf-aarch64.sh [--smoke] [--inject] [--timeout SEC] [--fw CODE.fd] [--vars VARS.fd] IMAGE
# --smoke   headless; exit 0 only if the serial console reaches a login prompt
#           (regex MARKER, default '[Ll]ogin: ').
# --inject  dev loop: first copy the loader and build/ kernel onto the copy's ESP.  Without it
#           the image is booted exactly as shipped (loader + \EFI\BOOT\kernel already on it).
# Env: ACCEL (default tcg; hvf on Apple Silicon is much faster), LOADER, KERNEL.
# Needs: qemu-system-aarch64, AAVMF (tools/ci/firmware.sh finds it); mtools and the loader
# at build/aarch64/boot/BOOTAA64.EFI (`bmake -C stand TARGET=aarch64`) only with --inject.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LOADER="${LOADER:-$ROOT/build/aarch64/boot/BOOTAA64.EFI}"
KERNEL="${KERNEL:-$ROOT/build/aarch64/boot/kernel}"
FW="${FW:-}"
VARS="${VARS:-}"
ACCEL="${ACCEL:-tcg}"
SMOKE=0
INJECT=0
MARKER="${MARKER:-[Ll]ogin: }"
TIMEOUT=300
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
[ "$INJECT" = 0 ] || [ -f "$LOADER" ] || { echo "missing $LOADER (bmake -C stand TARGET=aarch64)" >&2; exit 2; }
if [ -z "$FW" ] || [ -z "$VARS" ]; then
	set -- $("$ROOT/tools/ci/firmware.sh" aarch64) || exit 2
	[ -n "$FW" ] || FW=$1
	[ -n "$VARS" ] || VARS=${2:-}
fi
[ -f "$FW" ] || { echo "missing firmware $FW" >&2; exit 2; }

WORK=$(mktemp -d "${TMPDIR:-/tmp}/ubix-aavmf.XXXXXX")
trap 'kill $QPID 2>/dev/null || true; rm -rf "$WORK"' EXIT INT TERM
QPID=
cp "$IMG" "$WORK/disk.img"
# AAVMF code is 64 MB; pflash needs both banks the same size.
dd if=/dev/zero of="$WORK/code.fd" bs=1048576 count=64 2>/dev/null
dd if="$FW" of="$WORK/code.fd" conv=notrunc 2>/dev/null
if [ -n "$VARS" ]; then cp "$VARS" "$WORK/vars.fd"; else dd if=/dev/zero of="$WORK/vars.fd" bs=1048576 count=64 2>/dev/null; fi

if [ "$INJECT" = 1 ]; then
	# Install the loader at the removable-media path on the ESP (partition 1 at 1 MB).
	mmd -i "$WORK/disk.img@@1M" ::/EFI ::/EFI/BOOT 2>/dev/null || true
	mcopy -o -i "$WORK/disk.img@@1M" "$LOADER" ::/EFI/BOOT/BOOTAA64.EFI
	# Refresh the kernel beside it so a rebuilt kernel is tested without a new image.
	[ -f "$KERNEL" ] && mcopy -o -i "$WORK/disk.img@@1M" "$KERNEL" ::/EFI/BOOT/kernel
fi

CPU=cortex-a72
[ "$ACCEL" = tcg ] || CPU=host
set -- qemu-system-aarch64 -machine virt,gic-version=2,acpi=off -accel "$ACCEL" -cpu "$CPU" -m "$MEM" \
	-drive if=pflash,format=raw,readonly=on,file="$WORK/code.fd" \
	-drive if=pflash,format=raw,file="$WORK/vars.fd" \
	-global virtio-mmio.force-legacy=false \
	-drive file="$WORK/disk.img",format=raw,if=none,id=hd0 \
	-device virtio-blk-device,drive=hd0 \
	-device virtio-gpu-device -device virtio-keyboard-device -device virtio-mouse-device \
	-no-reboot

if [ "$SMOKE" = 1 ]; then
	"$@" -display none -serial file:"$WORK/serial.log" -monitor none &
	QPID=$!
	i=0
	while [ $i -lt "$TIMEOUT" ]; do
		if grep -Eq "$MARKER" "$WORK/serial.log" 2>/dev/null; then
			echo "aavmf-smoke: PASS (login prompt reached)"
			exit 0
		fi
		kill -0 $QPID 2>/dev/null || break
		sleep 1
		i=$((i + 1))
	done
	echo "aavmf-smoke: FAIL (no login prompt); serial tail:" >&2
	tail -n 40 "$WORK/serial.log" >&2
	exit 1
fi
exec "$@" -serial mon:stdio
