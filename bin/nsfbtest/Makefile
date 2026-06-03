# bin/nsfbtest/Makefile — libnsfb uBixOS surface backend validation.
include ../../Makefile.incl
include ../Makefile.incl

BINARY = nsfbtest
OBJS   = main.o

EXTRA_CFLAGS = -I${SRCTOP}/contrib/libnsfb/include
# --export-dynamic: libnsfb.so's ubix surface resolves mpi_* (from ubix_api.a,
# linked into this exe) at load via the executable's dynamic symbol table.
EXTRA_LDFLAGS = -Wl,--export-dynamic
# ubix_api.a after libnsfb.so (both inside the mk's --start-group) so the
# surface backend's mpi_* references resolve.
EXTRA_LIBS    = ${OBJ_DIR}/lib/libnsfb.so ${OBJ_DIR}/lib/ubix_api.a

.include "${UBIX_MK}/ubix.musl.ubix.prog.mk"
