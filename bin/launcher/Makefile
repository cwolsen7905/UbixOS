# (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY = launcher

OBJS = launcher.o ubixButton.o ubixDesktop.o

INCLUDES += -I./include \
            -I../../lib/views/sunlight/include \
            -I../../lib/objgfx40/objgfx40

LIBRARIES += ${OBJ_DIR}/obj/gfx/objgfx40/*.o \
             ${OBJ_DIR}/obj/gfx/sunlight/*.o \
             ../../lib/libcpp/libcpp.o

.include "${UBIX_MK}/ubix.prog.mk"
