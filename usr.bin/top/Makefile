# bin/top/Makefile — build busybox top (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = top

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/procps ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -w

OBJS = main.o top.o libbb_stubs.o procps.o get_last_path_component.o duration.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
