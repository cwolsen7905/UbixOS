# bin/less/Makefile — build busybox less (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = less

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/miscutils

EXTRA_CFLAGS = -I${BB_SRC} -w

OBJS = main.o less.o libbb_stubs.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
