# bin/expr/Makefile — build busybox expr (1.36.1) for UbixOS.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = expr

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/coreutils ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -I${BB_SRC}/coreutils -w

OBJS = main.o expr.o libbb_stubs.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
