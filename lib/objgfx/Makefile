# (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl

OBJS = objgfx.o ogEdgeTable.o ogImage.o ogPixCon.o ogPixelFmt.o vWindow.o ogFont.o vWidget.o

INCLUDES  = -I${MUSL_SRC}/include \
            -I${MUSL_OBJ}/obj/include \
            -I${MUSL_SRC}/arch/i386 \
            -I${MUSL_SRC}/arch/generic \
            -I${SRCTOP}/include

OBJDIR = ${OBJ_DIR}/obj/gfx/objgfx

.include "${UBIX_MK}/ubix.sublib.mk"
