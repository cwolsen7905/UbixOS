#!/bin/sh
# firmware.sh ARCH — print "CODE VARS": the edk2 firmware code and (template) variable
# store for ARCH (x86_64 = OVMF, aarch64 = AAVMF).  Probes, in order: $FW/$VARS,
# QEMU's share directory (Homebrew), then the Debian/Ubuntu packages (ovmf,
# qemu-efi-aarch64) the CI toolchain image carries.  Exit 1 and name what was
# searched when nothing is found.
arch="$1"
qshare=
q=$(command -v "qemu-system-$arch" 2>/dev/null) && qshare="$(dirname "$q")/../share/qemu"
case "$arch" in
x86_64)
	codes="${FW:-} $qshare/edk2-x86_64-code.fd /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/qemu/edk2-x86_64-code.fd"
	varss="${VARS:-} $qshare/edk2-i386-vars.fd /usr/share/OVMF/OVMF_VARS_4M.fd /usr/share/OVMF/OVMF_VARS.fd /usr/share/qemu/edk2-i386-vars.fd" ;;
aarch64)
	codes="${FW:-} $qshare/edk2-aarch64-code.fd /usr/share/AAVMF/AAVMF_CODE.fd /usr/share/qemu-efi-aarch64/QEMU_EFI.fd /usr/share/qemu/edk2-aarch64-code.fd"
	varss="${VARS:-} $qshare/edk2-arm-vars.fd /usr/share/AAVMF/AAVMF_VARS.fd" ;;
*) echo "usage: $0 x86_64|aarch64" >&2; exit 2 ;;
esac
first() { for f in "$@"; do [ -n "$f" ] && [ -f "$f" ] && { echo "$f"; return 0; }; done; return 1; }
# shellcheck disable=SC2086
code=$(first $codes) || { echo "firmware.sh: no $arch UEFI code image; searched: $codes" >&2; exit 1; }
# shellcheck disable=SC2086
vars=$(first $varss) || vars=
echo "$code $vars"
