# bin/bmake/Makefile — native BSD make (contrib/bmake, see UBIXOS_UPSTREAM_REV)
# so the UbixOS tree can be built inside UbixOS.  Installed as /bin/bmake;
# its mk library goes to /usr/share/mk (the image scripts copy contrib/bmake/mk).

include ../../Makefile.incl
include ../Makefile.incl

BMAKE_SRC = ${SRCTOP}/contrib/bmake

BINARY = bmake

.PATH.c: ${BMAKE_SRC}

BMAKE_VERSION != cat ${BMAKE_SRC}/VERSION | sed -n 's/^_MAKE_VERSION=[ 	]*//p'

EXTRA_CFLAGS = -I${BMAKE_SRC} -I${BMAKE_SRC}/missing -DHAVE_CONFIG_H -DMAKE_NATIVE \
               -DBMAKE_PATH_MAX=1024 -DMAKE_VERSION=\"${BMAKE_VERSION}\" \
               -DMACHINE=\"${_ARCH}\" -DMACHINE_ARCH=\"${_ARCH}\" -DMAKE_OS=\"uBixOS\" \
               -D_PATH_DEFSYSPATH=\"/usr/share/mk\" -DMAKE_SAVE_DOLLARS_DEFAULT=\"no\" \
               -DUSE_META -w

OBJS = main.o job.o meta.o arch.o buf.o compat.o cond.o dir.o for.o hash.o lst.o \
       make.o make_malloc.o metachar.o parse.o sigcompat.o str.o suff.o targ.o \
       trace.o var.o util.o stresep.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
