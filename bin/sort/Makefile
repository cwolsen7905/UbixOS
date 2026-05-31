# bin/sort/Makefile — build busybox sort (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = sort

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/coreutils

EXTRA_CFLAGS = -I${BB_SRC} -w

OBJS = main.o sort.o libbb_stubs.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
