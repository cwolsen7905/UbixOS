# Views desktop environment Makefile (C) 2002-2026 The UbixOS Project
#
# Groups the display-stack binaries: the compositor (views) and every
# GUI client app that draws into a vmm_share_region buffer.  Add a new
# directory name to SUBDIRS to include it in the build and clean targets.

SUBDIRS = views taskbar term

# bin/Makefile recurses into this group with a bare ${MAKE} (no target), so the
# default target must build every SUBDIR — not just the first .for-generated one.
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
