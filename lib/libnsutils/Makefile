# (C) 2002-2026 The uBixOS Project
# libnsutils — NetSurf small utilities (time/base64; vendored, MIT). Leaf lib.
include ../../Makefile.incl
include ../Makefile.incl
MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
NSU      = ${SRCTOP}/contrib/libnsutils
# Per-arch knobs (NetSurf C libs): i386 disables x86 SIMD + links elf_i386;
# aarch64 uses its musl arch headers + the aarch64 ld emulation, no SSE flags.
.if ${_ARCH} == "aarch64"
ARCH_SIMD     =
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/aarch64
ARCH_LDEMUL   = aarch64elf
.elif ${_ARCH} == "x86_64"
ARCH_SIMD     = -mno-sse -mno-sse2 -mno-mmx -mno-3dnow
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/x86_64
ARCH_LDEMUL   = elf_x86_64
.else
ARCH_SIMD     = -mno-sse -mno-sse2 -mno-mmx -mno-3dnow
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/i386
ARCH_LDEMUL   = elf_i386
.endif

INCLUDES = -I${NSU}/include -I${NSU}/src -I${MUSL_SRC}/include -I${MUSL_OBJ}/obj/include ${ARCH_MUSL_INC} -I${MUSL_SRC}/arch/generic
CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC -fcommon ${ARCH_SIMD} -MMD -MP -Wall -O2
OBJDIR = ${OBJ_DIR}/obj/lib/libnsutils
OBJS   = base64.o time.o unistd.o 
.PATH: ${NSU}/src
.PATH.o: ${OBJDIR}
.SUFFIXES: .o .c
.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}
all: ${OBJ_DIR}/lib/libnsutils.so
${OBJ_DIR}/lib/libnsutils.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m ${ARCH_LDEMUL} -shared -soname libnsutils.so ${OBJS:S|^|${OBJDIR}/|} -L${OBJ_DIR}/lib -lc -o ${OBJ_DIR}/lib/libnsutils.so
clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libnsutils.so
.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
