# tools/ports/u-boot/Makefile — U-Boot bootloader port (QEMU virt aarch64).
#
# Builds u-boot.bin (the -bios firmware for `bmake run-uboot-aarch64`) that loads
# the uBixOS kernel from the FAT boot partition — a real firmware -> bootloader ->
# filesystem boot chain instead of QEMU's -kernel injection.  See build.sh.
#
#   bmake -C tools/ports/u-boot          # clone + build -> build/ports/u-boot/u-boot.bin
#
# The build is custom (U-Boot has its own Kconfig/GNU-make system), so this is a
# thin wrapper over build.sh rather than the standard ports.mk configure/make.

build all:
	@sh ${.CURDIR}/build.sh

.PHONY: build all
