#!/bin/sh
# tools/ports/u-boot/build.sh — build Das U-Boot for QEMU `virt` (aarch64) as the
# bootloader that loads the uBixOS kernel from the FAT boot partition, so the
# system boots through a real firmware -> bootloader -> filesystem -> kernel chain
# (`bmake run-uboot-aarch64`, -bios u-boot.bin) instead of QEMU's -kernel shortcut.
#
# U-Boot's baked bootcmd fatloads the flat arm64 Image (start.S _head header) from
# the FAT partition to the kernel's fixed link address 0x40200000 and booti's it,
# passing QEMU's control DTB ($fdtcontroladdr) the kernel needs to size RAM.
#
# The kernel lives at the FAT ROOT (/kernel/kernel), not /boot/kernel/kernel: the
# OS mounts this FAT partition at /boot, so an internal boot/ dir would double up
# to /boot/boot/kernel/kernel on-device.  Flat FAT root => on-device /boot/kernel/kernel.
#
# Output: build/ports/u-boot/u-boot.bin  (referenced by the run-uboot targets).
#
# One-time host deps (macOS): brew install make dtc openssl@3   (+ aarch64-elf-gcc).
# U-Boot needs GNU Make >= 3.82 (macOS /usr/bin/make is 3.81) and OpenSSL headers
# for its host tools; both are wired below without touching the caller's shell.
set -e

VER=2024.01
CROSS=aarch64-elf-
SRCTOP=$(cd "$(dirname "$0")/../../.." && pwd)
OUT="${SRCTOP}/build/ports/u-boot"
SRC="${OUT}/u-boot-${VER}"
# NOTE: single-quoted so $fdtcontroladdr stays literal — U-Boot expands it at boot.
BOOTCMD='fatload virtio 0:1 0x40200000 /kernel/kernel; booti 0x40200000 - $fdtcontroladdr'

# GNU make >= 3.82 (macOS /usr/bin/make is 3.81 — too old for U-Boot's Makefile).
GM=$(command -v gmake 2>/dev/null || true)
[ -x "$GM" ] || GM=/opt/homebrew/opt/make/bin/gmake
[ -x "$GM" ] || { echo "u-boot: need GNU make >= 3.82 (brew install make)" >&2; exit 1; }

command -v ${CROSS}gcc >/dev/null 2>&1 || { echo "u-boot: need ${CROSS}gcc" >&2; exit 1; }
command -v dtc >/dev/null 2>&1 || { echo "u-boot: need dtc (brew install dtc)" >&2; exit 1; }

# OpenSSL headers for U-Boot's host tools (mkimage/FIT); keg-only on brew.
SSL=$(brew --prefix openssl@3 2>/dev/null || echo /opt/homebrew/opt/openssl@3)
export C_INCLUDE_PATH="${SSL}/include${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}"
export LIBRARY_PATH="${SSL}/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"

mkdir -p "${OUT}"
if [ ! -d "${SRC}/.git" ] && [ ! -f "${SRC}/Makefile" ]; then
	echo "u-boot: cloning v${VER} ..."
	git clone --depth 1 --branch "v${VER}" https://github.com/u-boot/u-boot.git "${SRC}"
fi

cd "${SRC}"
"${GM}" qemu_arm64_defconfig CROSS_COMPILE=${CROSS}
./scripts/config --set-str BOOTCOMMAND "${BOOTCMD}"
./scripts/config --enable USE_BOOTCOMMAND
./scripts/config --set-val BOOTDELAY 1
"${GM}" olddefconfig CROSS_COMPILE=${CROSS}
"${GM}" CROSS_COMPILE=${CROSS} -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

cp -f u-boot.bin "${OUT}/u-boot.bin"
echo "u-boot: ${OUT}/u-boot.bin ($(wc -c < "${OUT}/u-boot.bin") bytes)"
