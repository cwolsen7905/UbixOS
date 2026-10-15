# tests/gpttest/Makefile — GPT write / re-read / validate round trip (W-058).
# Links libgpt.a (static) and libubix_api.so (ubix_disk_query / ubix_disk_rawio).

include ../../Makefile.incl
include ../Makefile.incl

BINARY = gpttest
OBJS   = main.o

EXTRA_CFLAGS = -I${SRCTOP}/include
EXTRA_LIBS   = ${OBJ_DIR}/lib/libgpt.a -L${OBJ_DIR}/lib -lubix_api

.include "${UBIX_MK}/ubix.musl.prog.mk"
