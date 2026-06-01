# bin/find/Makefile — build busybox find (1.36.1) for UbixOS/i386.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC = ${SRCTOP}/contrib/busybox

BINARY = find

.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/findutils ${BB_SRC}/libbb

EXTRA_CFLAGS = -I${BB_SRC} -w

OBJS = main.o find.o libbb_stubs.o get_last_path_component.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
