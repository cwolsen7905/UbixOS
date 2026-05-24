# (C) 2002-2026 The UbixOS Project
# Minimal C++ ABI for UbixOS — single-threaded, no exceptions.

include ../../Makefile.incl
include ../../lib/Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl

OBJDIR   = ${OBJ_DIR}/obj/libcxxabi

INCLUDE  = -I./include \
           -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           -I${MUSL_SRC}/arch/i386 \
           -I${MUSL_SRC}/arch/generic

OBJS     = cxxabi.o

OUTPUT   = ${OBJ_DIR}/lib/libcxxabi.a

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .cc

.cc.o:
	@mkdir -p ${OBJDIR}
	$(CXX) ${CROSS_M32} -Wall -nostdinc -fno-builtin -fno-rtti -fno-exceptions \
		-mno-sse -mno-sse2 -mno-mmx -mno-3dnow \
		-fomit-frame-pointer -O ${INCLUDE} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OUTPUT}

${OUTPUT}: ${OBJS}
	@mkdir -p ${OBJ_DIR}/lib
	$(AR) rcs ${OUTPUT} ${OBJS:S|^|${OBJDIR}/|}

clean:
	$(REMOVE) ${OBJDIR} ${OUTPUT}
