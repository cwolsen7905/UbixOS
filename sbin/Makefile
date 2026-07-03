# sbin — system/boot-critical admin tools installed to /sbin (C) 2002-2026 The UbixOS Project
#
# FreeBSD/macOS convention: disk + filesystem + low-level system administration
# utilities live here (fdisk, disklabel, mount, the ubixfs pool tools, …).  Add a
# directory name to SUBDIRS to include it in the build and clean targets.
SUBDIRS = fdisk disklabel format mount ubpool ubfs ubdisk ping reboot
.PHONY: all clean ${SUBDIRS}

.for _dir in ${SUBDIRS}
${_dir}:
	(cd ${_dir};${MAKE})
.endfor

all: ${SUBDIRS}

clean:
.for _dir in ${SUBDIRS}
	(cd ${_dir};${MAKE} clean)
.endfor
