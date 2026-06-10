# (C) 2002-2026 The uBixOS Project
# libutf8proc — Unicode normalisation (JuliaStrings, MIT/Unicode). NetSurf core
# dep. utf8proc.c #includes the shipped utf8proc_data.c table (no codegen).
include ../../Makefile.incl
include ../Makefile.incl
MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
U8P      = ${SRCTOP}/contrib/libutf8proc
# Per-arch knobs (NetSurf C libs): i386 disables x86 SIMD + links elf_i386;
# aarch64 uses its musl arch headers + the aarch64 ld emulation, no SSE flags.
.if ${_ARCH} == "aarch64"
ARCH_SIMD     =
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/aarch64
ARCH_LDEMUL   = aarch64elf
.else
ARCH_SIMD     = -mno-sse -mno-sse2 -mno-mmx -mno-3dnow
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/i386
ARCH_LDEMUL   = elf_i386
.endif

INCLUDES = -I${U8P} -I${MUSL_SRC}/include -I${MUSL_OBJ}/obj/include ${ARCH_MUSL_INC} -I${MUSL_SRC}/arch/generic
CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC -fcommon -DUTF8PROC_STATIC ${ARCH_SIMD} -MMD -MP -Wall -O2
OBJDIR = ${OBJ_DIR}/obj/lib/libutf8proc
OBJS   = utf8proc.o
.PATH: ${U8P}
.PATH.o: ${OBJDIR}
.SUFFIXES: .o .c
.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}
all: ${OBJ_DIR}/lib/libutf8proc.so
${OBJ_DIR}/lib/libutf8proc.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m ${ARCH_LDEMUL} -shared -soname libutf8proc.so ${OBJS:S|^|${OBJDIR}/|} -L${OBJ_DIR}/lib -lc -o ${OBJ_DIR}/lib/libutf8proc.so
clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libutf8proc.so
.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
