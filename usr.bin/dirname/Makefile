# bin/dirname/Makefile — build busybox dirname (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = dirname

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/coreutils ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -w

OBJS = main.o dirname.o libbb_stubs.o single_argv.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
