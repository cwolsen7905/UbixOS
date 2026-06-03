# (C) 2002-2026 The uBixOS Project
# libwapcaplet — NetSurf string-interning library (vendored, MIT).
# First of the NetSurf browser library stack; uses a UbixOS bmake Makefile
# instead of NetSurf's buildsystem.

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
LWC      = ${SRCTOP}/contrib/libwapcaplet

INCLUDES = -I${LWC}/include \
           -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           -I${MUSL_SRC}/arch/i386 \
           -I${MUSL_SRC}/arch/generic

CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC \
         -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP \
         -Wall -O2

OBJDIR = ${OBJ_DIR}/obj/lib/libwapcaplet
OBJS   = libwapcaplet.o

.PATH: ${LWC}/src
.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libwapcaplet.so

${OBJ_DIR}/lib/libwapcaplet.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m elf_i386 -shared -soname libwapcaplet.so \
		${OBJS:S|^|${OBJDIR}/|} \
		-L${OBJ_DIR}/lib -lc \
		-o ${OBJ_DIR}/lib/libwapcaplet.so

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libwapcaplet.so

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
