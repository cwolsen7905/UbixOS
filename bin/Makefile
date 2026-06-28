# System Makefile (C) 2002 The UbixOS Project
#
# /bin holds ONLY the essential user commands needed early by the kernel and the
# shell (FreeBSD/macOS convention).  The rest of the world lives in sibling trees,
# each with its own Makefile + Makefile.incl (which sets BINDIR):
#   sbin/      -> /sbin       disk/filesystem/system admin (fdisk, mount, ubfs, …)
#   usr.bin/   -> /usr/bin    bulk user commands + GUI apps (grep, vi, vdoom, …)
#   usr.sbin/  -> /usr/sbin   daemons + services (views, authd, logd, sshd, …)
#   tests/     -> /usr/tests  test/dev harnesses (not shipped apps)
#
# Add a directory name to SUBDIRS to include it in the build and clean targets.
# The 64-bit arches build the same essential set; i386 (frozen on releng/2) a
# slightly smaller one.
# This dispatcher reads ${_ARCH} at parse time but does not include Makefile.incl,
# so a top-down build (bmake world) inherits _ARCH from the environment while a
# DIRECT in-tree build (bmake -C /usr/src/bin on-device) would see it empty and
# fail "Malformed conditional".  Default to a 64-bit arch (aarch64 and x86_64 pick
# the same SUBDIRS; i386 is frozen) — an exported value still wins via ?=.
_ARCH ?= aarch64
.if ${_ARCH} == "aarch64" || ${_ARCH} == "x86_64"
SUBDIRS = init login shell cp ls cat echo ln date mkdir mv rm sleep pwd expr ed tcsh
.else
SUBDIRS = init login shell cp ls cat ed tcsh
.endif

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
