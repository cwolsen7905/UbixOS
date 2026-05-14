# Application Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY    = disklabel
OBJS      = main.o
LINK_TYPE = dynamic
LIBRARIES = ../../build/lib/libc.so
POST_LINK = strip ${BUILD_DIR}/bin/${BINARY}

.include "${UBIX_MK}/ubix.prog.mk"
