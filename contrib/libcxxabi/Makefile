# (C) 2002-2026 The UbixOS Project
# Minimal C++ ABI for UbixOS — single-threaded, no exceptions.

include ../../Makefile.incl
include ../../lib/Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl

OBJDIR   = ${OBJ_DIR}/obj/libcxxabi

# Per-arch knobs: i386 disables x86 SIMD + uses its musl arch headers; aarch64
# has no such SIMD flags and uses its own.  CROSS_M32 is empty on aarch64.
.if ${_ARCH} == "aarch64"
ARCH_SIMD     =
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/aarch64
# aarch64 world is PIE — the static archive must be position-independent.
ARCH_PIC      = -fPIC
.else
ARCH_SIMD     = -mno-sse -mno-sse2 -mno-mmx -mno-3dnow
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/i386
ARCH_PIC      =
.endif

INCLUDE  = -I./include \
           -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           ${ARCH_MUSL_INC} \
           -I${MUSL_SRC}/arch/generic

OBJS     = cxxabi.o

OUTPUT   = ${OBJ_DIR}/lib/libcxxabi.a

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .cc

.cc.o:
	@mkdir -p ${OBJDIR}
	$(CXX) ${CROSS_M32} -Wall -nostdinc -fno-builtin -fno-rtti -fno-exceptions \
		${ARCH_SIMD} ${ARCH_PIC} \
		-fomit-frame-pointer -O ${INCLUDE} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OUTPUT}

${OUTPUT}: ${OBJS}
	@mkdir -p ${OBJ_DIR}/lib
	$(AR) rcs ${OUTPUT} ${OBJS:S|^|${OBJDIR}/|}

clean:
	$(REMOVE) ${OBJDIR} ${OUTPUT}
