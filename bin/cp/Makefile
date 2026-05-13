# Application Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY    = cp
OBJS      = main.o
LIBRARIES = ${OBJ_DIR}/obj/lib/*/*.o
POST_LINK = strip ${BUILD_DIR}/bin/${BINARY}

.include "${UBIX_MK}/ubix.prog.mk"
