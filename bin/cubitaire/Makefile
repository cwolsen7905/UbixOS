# Copyright (c) 2002-2026 The UbixOS Project.

include ../../Makefile.incl
include ../Makefile.incl

BINARY     = cubitaire
OBJS       = main.o
SRCS       = main.cc
EXTRA_LIBS = ${OBJ_DIR}/lib/libaudio.a -L${OBJ_DIR}/lib -lubix_api

.include "${UBIX_MK}/ubix.musl.cxx.ubix.prog.mk"
