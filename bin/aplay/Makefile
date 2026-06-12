# Application Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY = aplay
OBJS   = aplay.o

EXTRA_LIBS = ${OBJ_DIR}/lib/libaudio.a ${OBJ_DIR}/lib/ubix_api.a

.include "${UBIX_MK}/ubix.musl.prog.mk"
