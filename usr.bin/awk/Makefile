include ../../Makefile.incl
include ../Makefile.incl
BB_SRC = ${SRCTOP}/contrib/busybox
BINARY = awk
.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/editors ${BB_SRC}/libbb
EXTRA_CFLAGS = -I${BB_SRC} -I${BB_SRC}/editors -w
OBJS = main.o awk.o libbb_stubs.o
.include "${UBIX_MK}/ubix.musl.prog.mk"
