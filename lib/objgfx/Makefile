# (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl

OBJS = objgfx.o ogEdgeTable.o ogImage.o ogPixCon.o ogPixelFmt.o vWindow.o ogFont.o vWidget.o og_stbimpl.o

INCLUDES  = -I${MUSL_SRC}/include \
            -I${MUSL_OBJ}/obj/include \
            -I${MUSL_SRC}/arch/i386 \
            -I${MUSL_SRC}/arch/generic \
            -I${SRCTOP}/contrib/stb \
            -I${SRCTOP}/include

OBJDIR = ${OBJ_DIR}/obj/gfx/objgfx

.include "${UBIX_MK}/ubix.sublib.mk"

all: ${OBJ_DIR}/lib/libobjgfx.so

${OBJ_DIR}/lib/libobjgfx.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m elf_i386 -shared -soname libobjgfx.so \
		${OBJS:S|^|${OBJDIR}/|} \
		-L${OBJ_DIR}/lib -lc \
		-o ${OBJ_DIR}/lib/libobjgfx.so

clean: clean-so
clean-so:
	$(REMOVE) ${OBJ_DIR}/lib/libobjgfx.so
