# bin/vmtest/Makefile — minimal heap demand-paging reproducer.

include ../../Makefile.incl
include ../Makefile.incl

BINARY = vmtest

EXTRA_CFLAGS = -I${SRCTOP}/contrib/stb

OBJS = main.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
