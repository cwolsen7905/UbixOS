# (C) 2002-2026 The uBixOS Project
# libparserutils — NetSurf parser/charset utilities (vendored, MIT).
# charset/aliases.inc is pre-generated on the build host (build/make-aliases.pl)
# and vendored, so no Perl is needed in the uBixOS build.

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
LPU      = ${SRCTOP}/contrib/libparserutils

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

INCLUDES = -I${LPU}/include \
           -I${LPU}/src \
           -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           ${ARCH_MUSL_INC} \
           -I${MUSL_SRC}/arch/generic

CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC \
         ${ARCH_SIMD} -MMD -MP \
         -Wall -O2

OBJDIR = ${OBJ_DIR}/obj/lib/libparserutils
OBJS   = aliases.o codec.o codec_8859.o codec_ascii.o codec_ext8.o codec_utf16.o codec_utf8.o \
         utf16.o utf8.o filter.o inputstream.o buffer.o errors.o stack.o vector.o

.PATH: ${LPU}/src/charset ${LPU}/src/charset/codecs ${LPU}/src/charset/encodings \
       ${LPU}/src/input ${LPU}/src/utils
.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libparserutils.so

${OBJ_DIR}/lib/libparserutils.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m ${ARCH_LDEMUL} -shared -soname libparserutils.so \
		${OBJS:S|^|${OBJDIR}/|} \
		-L${OBJ_DIR}/lib -lc \
		-o ${OBJ_DIR}/lib/libparserutils.so

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libparserutils.so

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
