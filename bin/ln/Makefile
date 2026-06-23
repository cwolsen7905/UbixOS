# bin/ln/Makefile — build busybox ln (1.36.1) for UbixOS.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = ln

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/coreutils ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -I${BB_SRC}/coreutils -w

OBJS = main.o ln.o libbb_stubs.o get_last_path_component.o isdirectory.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
