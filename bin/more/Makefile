# bin/more/Makefile — build busybox more (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = more

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/util-linux

EXTRA_CFLAGS = -I${BB_SRC} -w

OBJS = main.o more.o libbb_stubs.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
