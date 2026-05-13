# Application Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY    = ubistry
OBJS      = db.o message.o main.o
POST_LINK = strip ${BUILD_DIR}/bin/${BINARY}

.include "${UBIX_MK}/ubix.prog.mk"
