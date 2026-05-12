# (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY = muffin

OBJS = main.o

INCLUDES += -I../../lib/objgfx

LIBRARIES += ${OBJ_DIR}/obj/gfx/objgfx/*.o \
             ../../lib/libcpp/libcpp.o

.include "${UBIX_MK}/ubix.prog.mk"
