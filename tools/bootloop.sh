#!/bin/sh
# Multi-boot test harness for the SMP %gs-per-CPU work.
#
# Boots ubixos.img headless under QEMU with a std VGA device (so the VESA/GUI
# bring-up path — the one that exposed the nondeterministic #GP — actually runs),
# captures serial to a per-iteration log, runs each boot for a fixed window, then
# classifies the log as PASS / PANIC / NOLOGIN.
#
# Usage: tools/bootloop.sh [iterations] [seconds-per-boot]
#   iterations        number of boots (default 5)
#   seconds-per-boot  serial capture window before kill (default 30)
#
# Exit status: 0 if every boot PASSed, 1 otherwise.

set -u
ITERS=${1:-5}
SECS=${2:-30}
IMG=ubixos.img
OUTDIR=/tmp/bootloop
mkdir -p "$OUTDIR"

pass=0
fail=0
for i in $(seq 1 "$ITERS"); do
	log="$OUTDIR/serial.$i.log"
	rm -f "$log"
	# Headless: no window (-display none) but keep -vga std so VESA mode-set +
	# LFB mapping run.  -no-reboot so a triple fault stops instead of looping.
	qemu-system-i386 -m 256 \
		-drive file=$IMG,format=raw,if=ide,index=0 \
		-machine pc \
		-device piix3-usb-uhci,id=uhci-bus \
		-device usb-kbd,bus=uhci-bus.0,port=1 \
		-display none -vga std \
		-device e1000,netdev=net0 -netdev user,id=net0 \
		-serial "file:$log" \
		-no-reboot >/dev/null 2>&1 &
	qpid=$!
	# Run the full window so a crash that lands *after* the GUI marker (the old
	# failure mode was "shortly after display claim") is still caught.  Only a
	# panic short-circuits early.
	waited=0
	verdict=""
	gui=0
	while [ "$waited" -lt "$SECS" ]; do
		sleep 2
		waited=$((waited + 2))
		if [ ! -f "$log" ]; then continue; fi
		if grep -qE "kpanic|INT OFF! KERN|Triple|triple|#GP|cpu_switch BAD GS|PCPU GS LEAK" "$log"; then
			verdict="PANIC"; break
		fi
		if grep -qE "display claimed|views: composit" "$log"; then
			gui=1
		fi
	done
	kill "$qpid" 2>/dev/null
	wait "$qpid" 2>/dev/null
	if [ -z "$verdict" ]; then
		if grep -qE "kpanic|INT OFF! KERN|#GP|cpu_switch BAD GS|PCPU GS LEAK" "$log" 2>/dev/null; then
			verdict="PANIC"
		elif [ "$gui" -eq 1 ] || grep -qE "display claimed|views: composit" "$log" 2>/dev/null; then
			verdict="PASS"
		else
			verdict="NOLOGIN"
		fi
	fi
	# Surface any GS-leak diagnostics regardless of verdict.
	leak=$(grep -cE "cpu_switch BAD GS|PCPU GS LEAK" "$log" 2>/dev/null || echo 0)
	if [ "$verdict" = "PASS" ]; then
		pass=$((pass + 1))
	else
		fail=$((fail + 1))
	fi
	printf "boot %2d/%-2d  %-8s  (%ss)  gs-leaks=%s  -> %s\n" \
		"$i" "$ITERS" "$verdict" "$waited" "$leak" "$log"
done

echo "-----------------------------------------------"
echo "PASS=$pass  FAIL=$fail  of $ITERS"
[ "$fail" -eq 0 ]
