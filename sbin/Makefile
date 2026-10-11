# sbin — system/boot-critical admin tools installed to /sbin (C) 2002-2026 The UbixOS Project
#
# FreeBSD/macOS convention: disk + filesystem + low-level system administration
# utilities live here (fdisk, disklabel, mount, the ubixfs pool tools, …).  Add a
# directory name to SUBDIRS to include it in the build and clean targets.
SUBDIRS = fdisk disklabel format mount ubpool ubfs ubdisk ping reboot
# The per-directory targets below are defined before "all"; keep "all" the
# default so a bare `bmake` builds every subdirectory (on-device it built only
# the first one — the host world always names "all" explicitly).
.MAIN: all

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
