#!/bin/sh
# End-to-end check for the UbixFS macOS-kit C foundation.
#   $1 = path to the `ubfs` host CLI   $2 = path to the built image_test
# Builds a populated raw pool, reads it through the shim, then wraps a copy in an
# MBR disk image (UbixFS partition type 0x9C at LBA 2048) and reads that too.
set -e

UBFS="$1"
TEST="$2"
[ -x "$UBFS" ] || { echo "FAIL: ubfs CLI not found at $UBFS (run 'bmake' in tools/ubixfs first)"; exit 1; }

RAW=raw.pool
DISK=disk.img
PART_LBA=2048          # 1 MiB
POOL_BYTES=$((16 * 1024 * 1024))
POOL_SECTORS=$((POOL_BYTES / 512))

echo "== building a populated raw pool =="
rm -f "$RAW" "$DISK"
"$UBFS" mkpool "$RAW" 16M
"$UBFS" mkdir "$RAW" /etc
"$UBFS" mkdir "$RAW" /bin
echo "hello from ubixfs" > /tmp/.ubfs_hello.$$
"$UBFS" cp /tmp/.ubfs_hello.$$ "$RAW":/etc/motd
rm -f /tmp/.ubfs_hello.$$

echo "== reading the RAW pool through the shim =="
"$TEST" "$RAW" root /
echo
"$TEST" "$RAW" root /etc /etc/motd

echo
echo "== wrapping the pool in an MBR disk image (type 0x9C @ LBA $PART_LBA) =="
# zero the leading 1 MiB, then place the pool at the partition offset.
dd if=/dev/zero of="$DISK" bs=512 count="$PART_LBA" status=none
cat "$RAW" >> "$DISK"

# Partition record at 0x1BE: boot=0x80, type=0x9C, lba_start, sector_count (LE).
le32() { # $1 = value -> 4 little-endian bytes as printf escapes
	v=$1
	printf '\\x%02x\\x%02x\\x%02x\\x%02x' \
		$((v & 255)) $(((v >> 8) & 255)) $(((v >> 16) & 255)) $(((v >> 24) & 255))
}
# boot flag + CHS(3) + type + CHS(3)
printf '\x80\x00\x00\x00\x9c\x00\x00\x00' | dd of="$DISK" bs=1 seek=446 conv=notrunc status=none
# LBA start
printf "$(le32 $PART_LBA)" | dd of="$DISK" bs=1 seek=454 conv=notrunc status=none
# sector count
printf "$(le32 $POOL_SECTORS)" | dd of="$DISK" bs=1 seek=458 conv=notrunc status=none
# boot signature
printf '\x55\xaa' | dd of="$DISK" bs=1 seek=510 conv=notrunc status=none

echo "== reading the MBR-WRAPPED pool through the shim =="
"$TEST" "$DISK" root /
echo
"$TEST" "$DISK" root /etc /etc/motd

echo
echo "ALL IMAGE_TEST CHECKS PASS"
