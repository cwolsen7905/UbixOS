# bin/vi/Makefile — build busybox-vi (1.36.1) for UbixOS/i386.
# Sources live in contrib/busybox-vi; objects go to build/obj/bin/vi/.

include ../../Makefile.incl
include ../Makefile.incl

VI_SRC = ${SRCTOP}/contrib/busybox-vi

BINARY = vi

# Tell bmake where to find the .c sources.
.PATH.c: ${VI_SRC}

# Headers live in the same directory; -w suppresses busybox-side warnings
# we don't want to chase in upstream code.
EXTRA_CFLAGS = -I${VI_SRC} -w

OBJS = vi.o libbb_stubs.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
