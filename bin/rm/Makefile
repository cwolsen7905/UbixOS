# bin/rm/Makefile — build busybox rm (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = rm

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/coreutils ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -I${BB_SRC}/coreutils -w

OBJS = main.o rm.o libbb_stubs.o remove_file.o get_last_path_component.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
