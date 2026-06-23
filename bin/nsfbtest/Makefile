# bin/nsfbtest/Makefile — libnsfb uBixOS surface backend validation.
include ../../Makefile.incl
include ../Makefile.incl

BINARY = nsfbtest
OBJS   = main.o

EXTRA_CFLAGS = -I${SRCTOP}/contrib/libnsfb/include
# --export-dynamic: keep this exe's dynamic symbols visible so libnsfb.so's ubix
# surface backend can resolve mpi_* at load (now provided by libubix_api.so, which is
# also in the global scope as a DT_NEEDED of this exe — harmless to keep exported).
EXTRA_LDFLAGS = -Wl,--export-dynamic
EXTRA_LIBS    = ${OBJ_DIR}/lib/libnsfb.so -L${OBJ_DIR}/lib -lubix_api

.include "${UBIX_MK}/ubix.musl.ubix.prog.mk"
