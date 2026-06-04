# bin/date/Makefile — build busybox date (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = date

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/coreutils ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -w

OBJS = main.o date.o libbb_stubs.o time.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
