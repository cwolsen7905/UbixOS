# Copyright (c) 2002-2026 The UbixOS Project.

include ../../Makefile.incl
include ../Makefile.incl

BINARY = taskbar
OBJS   = taskbar.o
SRCS   = taskbar.cc

INCLUDES += -I../../include

LIBRARIES += ${OBJ_DIR}/obj/gfx/objgfx/*.o \
             ../../lib/libcpp/libcpp.o

.include "${UBIX_MK}/ubix.prog.mk"
