# bin/which/Makefile — build busybox which (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = which

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/debianutils ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -w

OBJS = main.o which.o libbb_stubs.o executable.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
