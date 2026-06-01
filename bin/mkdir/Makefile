# bin/mkdir/Makefile — build busybox mkdir (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = mkdir

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/coreutils ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -I${BB_SRC}/coreutils -w

OBJS = main.o mkdir.o libbb_stubs.o make_directory.o parse_mode.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
