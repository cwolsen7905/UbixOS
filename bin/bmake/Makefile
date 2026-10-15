# bin/bmake/Makefile (C) 2002-2026 The UbixOS Project
# bmake (Simon Gerraty's portable NetBSD make), the make uBixOS builds itself
# with.  Sources live in contrib/bmake-20240212 (pristine upstream, unit-tests
# removed); config.h here is configure's host output adapted for musl, and
# shim/sys/cdefs.h supplies the BSD macros musl does not ship.  Installed as
# /bin/bmake; its system makefiles (contrib/bmake-20240212/mk) go to
# /usr/share/mk via tools/mkimage.sh.
#
# Not built: meta.c (meta mode needs filemon, which uBixOS lacks) and the compat
# shims musl already covers.  HAVE_MMAP is off in config.h: bmake read()s
# makefiles instead of mmap()ing them.

include ../../Makefile.incl
include ../Makefile.incl

BMAKE_SRC = ${SRCTOP}/contrib/bmake-20240212

BINARY = bmake

.PATH.c: ${BMAKE_SRC}

EXTRA_CFLAGS = -I${.CURDIR} -I${.CURDIR}/shim -I${BMAKE_SRC} -std=gnu99 -w \
	-DHAVE_CONFIG_H -DMAKE_NATIVE -DBMAKE_PATH_MAX=1024 \
	-DMAKE_VERSION='"20240212"' \
	-DMACHINE='"${_ARCH}"' -DMACHINE_ARCH='"${_ARCH}"' \
	-D_PATH_DEFSYSPATH='"/usr/share/mk"'

OBJS = main.o job.o arch.o buf.o compat.o cond.o dir.o for.o hash.o lst.o \
	make.o make_malloc.o metachar.o parse.o sigcompat.o str.o suff.o \
	targ.o trace.o var.o util.o stresep.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
