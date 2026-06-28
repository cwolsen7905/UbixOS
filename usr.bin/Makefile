# usr.bin — the bulk of user commands + graphical apps, installed to /usr/bin
# (C) 2002-2026 The UbixOS Project
#
# FreeBSD/macOS convention: everything that is a user command but not needed
# early by the kernel/shell lives here (text utils, dev tools, net clients, and
# the windowed desktop apps).  Add a directory name to SUBDIRS to include it.
#
# Common set builds on both 64-bit arches; wget/httpsget are gated to x86_64
# (mirrors the historical bin/Makefile split — the bearssl/pw/http net stack was
# only wired into the x86_64 world).
# Default _ARCH for a DIRECT in-tree build (bmake -C /usr/src/usr.bin on-device):
# this dispatcher reads ${_ARCH} below without including Makefile.incl, so without
# a default the conditionals would be "Malformed".  A top-down/cross build's
# exported _ARCH still wins via ?=.  aarch64 default selects the common 64-bit set.
_ARCH ?= aarch64
SUBDIRS = taskbar term settings uname basename cut dirname env find grep head \
          less more sort sed stat tail touch tr uniq wc which cmp install diff \
          printf awk make vi nc diskutil activity files vdoom tessera cubitaire
.if ${_ARCH} == "x86_64"
SUBDIRS += wget httpsget
.endif
# i386-only user apps (frozen on releng/2): the bespoke static clock, the TCC
# compiler, fullscreen doom, and the standalone audio players.  Homed here so
# they build to /usr/bin when/if i386 work resumes; not built on master.
# (top/edit/muffin also live in this tree but were never wired into any build.)
.if ${_ARCH} == "i386"
SUBDIRS += clock tcc doom aplay mp3play
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
