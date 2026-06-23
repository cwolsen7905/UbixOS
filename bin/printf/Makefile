# bin/printf/Makefile — FreeBSD printf(1) built against musl for UbixOS.

include ../../Makefile.incl
include ../Makefile.incl

BINARY = printf
OBJS   = printf.o
SRCS   = printf.c

EXTRA_CFLAGS = -w

.include "${UBIX_MK}/ubix.musl.prog.mk"
