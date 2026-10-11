#!/bin/sh
# boot-smoke — boot each arch headless on a COPY of the disk image and require
# the login prompt on the serial console, with no panic.  Never touches
# ubixos*.img in place or any running QEMU.
#
#   CHECK_IMAGE_<ARCH>   image to copy (default: ubixos-arm.img / ubixos-x86_64.img,
#                        the Makefile's DISK_IMAGE_ARM / DISK_IMAGE_X86_64)
#   CHECK_BOOT_TIMEOUT   seconds per arch (default 240; TCG is slow)
#   CHECK_BOOT_MARKER    regex that means "booted" (default: "Login: ", bin/login)
. "$CHECK_ROOT/tools/check/lib.sh"
cd "$CHECK_ROOT" || exit 2
TIMEOUT="${CHECK_BOOT_TIMEOUT:-240}"
MARKER="${CHECK_BOOT_MARKER:-Login: }"
bad=0; ran=0
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT

for a in $CHECK_ARCHES; do
	# Same invocation shape as the Makefile's run-debug-* targets: the kernel is
	# injected with -kernel, the root disk is a virtio-blk copy, serial to a file.
	kern="$CHECK_ROOT/build/$a/boot/kernel"
	case "$a" in
	aarch64)
		img="${CHECK_IMAGE_AARCH64:-$CHECK_ROOT/ubixos-arm.img}"; q=qemu-system-aarch64
		qargs="-machine virt,gic-version=2 -cpu cortex-a72 -m 512 -global virtio-mmio.force-legacy=false"
		qdisk="-device virtio-blk-device,drive=hd0" ;;
	x86_64)
		img="${CHECK_IMAGE_X86_64:-$CHECK_ROOT/ubixos-x86_64.img}"; q=qemu-system-x86_64
		qargs="-m 256 -cpu qemu64,+x2apic"
		qdisk="-device virtio-blk-pci,drive=hd0,disable-modern=true" ;;
	*) continue ;;
	esac
	command -v "$q" >/dev/null 2>&1 || { echo "$q not installed"; continue; }
	[ -f "$img" ] && [ -f "$kern" ] || { echo "[$a] need $kern and $img (bmake kernel image TARGET=$a)"; continue; }
	ran=1
	cp "$img" "$work/$a.img"
	log="$work/$a.log"
	# shellcheck disable=SC2086
	"$q" $qargs -kernel "$kern" -display none -monitor none -serial "file:$log" \
		-drive "file=$work/$a.img,format=raw,if=none,id=hd0" $qdisk >/dev/null 2>&1 &
	pid=$!
	i=0
	while [ $i -lt "$TIMEOUT" ]; do
		grep -Eq "$MARKER" "$log" 2>/dev/null && break
		grep -Eqi "panic|assertion failed" "$log" 2>/dev/null && break
		kill -0 $pid 2>/dev/null || break
		sleep 1; i=$((i + 1))
	done
	kill $pid 2>/dev/null; wait $pid 2>/dev/null
	if grep -Eqi "panic" "$log" 2>/dev/null; then
		echo "[$a] kernel panic:"; grep -i -m3 panic "$log" | sed 's/^/    /'; bad=1
	elif ! grep -Eq "$MARKER" "$log" 2>/dev/null; then
		echo "[$a] no '$MARKER' within ${TIMEOUT}s; last serial output:"; tail -n 5 "$log" 2>/dev/null | sed 's/^/    /'; bad=1
	else
		echo "[$a] booted in ${i}s"
	fi
done
[ $ran -eq 0 ] && { echo "no qemu/image available"; exit $CHECK_SKIP; }
exit $bad
