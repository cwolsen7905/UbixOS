#!/bin/sh
# tools/mkimage.sh — Create a bootable QEMU ISO image for UbixOS.
#
# Prerequisites (macOS via Homebrew):
#   brew install qemu xorriso
#   brew install i686-elf-grub   (provides i686-elf-grub-mkrescue)
#
# Usage:
#   sh tools/mkimage.sh [output.iso]

set -e

ISO="${1:-ubixos.iso}"
KERNEL="sys/compile/kernel"
GRUB_CFG="tools/grub.cfg"

# Verify prerequisites.
if [ ! -f "$KERNEL" ]; then
  echo "ERROR: $KERNEL not found — run 'bmake kernel' first." >&2
  exit 1
fi

if ! command -v i686-elf-grub-mkrescue >/dev/null 2>&1; then
  echo "ERROR: i686-elf-grub-mkrescue not found." >&2
  echo "  Install with: brew install i686-elf-grub" >&2
  exit 1
fi

if ! command -v xorriso >/dev/null 2>&1; then
  echo "ERROR: xorriso not found (required by grub-mkrescue)." >&2
  echo "  Install with: brew install xorriso" >&2
  exit 1
fi

# Build the ISO staging tree.
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/boot/kernel"
mkdir -p "$STAGE/boot/grub"

cp "$KERNEL"  "$STAGE/boot/kernel/kernel"
cp "$GRUB_CFG" "$STAGE/boot/grub/grub.cfg"

echo "==> Creating bootable ISO: $ISO"
i686-elf-grub-mkrescue -o "$ISO" "$STAGE"

echo ""
echo "Done: $ISO"
echo "Run with: bmake run"
echo "     or:  qemu-system-i386 -m 256 -cdrom $ISO -serial stdio -vga std"
