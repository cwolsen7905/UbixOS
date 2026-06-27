# bin/diff/Makefile — build busybox diff (1.36.1) for UbixOS.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = diff

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/editors ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -I${BB_SRC}/editors -w

OBJS = main.o diff.o libbb_stubs.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
