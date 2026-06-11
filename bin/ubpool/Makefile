# bin/ubpool/Makefile — list mounted UbixFS pools (the zpool analog).
include ../../Makefile.incl
include ../Makefile.incl

BINARY = ubpool
OBJS   = main.o

EXTRA_CFLAGS = -I${SRCTOP}/include
EXTRA_LIBS   = ${OBJ_DIR}/lib/ubix_api.a   # ubix_pool_query (native syscall 67)

.include "${UBIX_MK}/ubix.musl.prog.mk"
