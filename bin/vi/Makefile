# bin/vi/Makefile — build busybox vi (1.36.1) for UbixOS/i386.
# Sources live in contrib/busybox; objects go to build/obj/bin/vi/.

include ../../Makefile.incl
include ../Makefile.incl

BB_SRC  = ${SRCTOP}/contrib/busybox

BINARY = vi

# Pick sources from busybox subtree (editors/vi.c) and the shared libbb shim.
.PATH.c: ${.CURDIR} ${BB_SRC} ${BB_SRC}/editors

# Headers live in the busybox root; -w suppresses busybox-side warnings we
# don't want to chase in upstream code.
EXTRA_CFLAGS = -I${BB_SRC} -w

OBJS = main.o vi.o libbb_stubs.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
