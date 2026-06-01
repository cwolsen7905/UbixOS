# bin/grep/Makefile — build busybox grep (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = grep

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/findutils

EXTRA_CFLAGS = -I${BB_SRC} -w

OBJS = main.o grep.o libbb_stubs.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
