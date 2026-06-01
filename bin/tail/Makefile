# bin/tail/Makefile — build busybox tail (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = tail

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/coreutils

EXTRA_CFLAGS = -I${BB_SRC} -w

OBJS = main.o tail.o libbb_stubs.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
