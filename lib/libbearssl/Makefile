# (C) 2002-2026 The UbixOS Project
# libbearssl — BearSSL (https://bearssl.org) TLS + crypto, built as a UbixOS
# world shared object.  The library itself is OS-independent C99; all platform
# glue (sockets, entropy seeding, certificate-validation time) lives in the
# consuming application, not here.  See contrib/bearssl/src/config.h for the
# -mno-sse build override.

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
BEARSSL  = ${SRCTOP}/contrib/bearssl

# Per-arch knobs: i386 disables x86 SIMD + links elf_i386; aarch64 uses its musl
# arch headers + the aarch64 ld emulation and has no SSE flags.
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

# BearSSL needs its own inc/ (public API) and src/ (inner.h, config.h) on the
# include path, ahead of the musl headers it pulls (<string.h>, <limits.h>).
INCLUDES = -I${BEARSSL}/inc \
           -I${BEARSSL}/src \
           -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           ${ARCH_MUSL_INC} \
           -I${MUSL_SRC}/arch/generic \
           -I${SRCTOP}/include

# Explicit freestanding CFLAGS (libaudio-style), plus -fPIC for the shared
# object.  Deliberately omit the C++ -fno-rtti/-fno-exceptions flags that the
# inherited lib CFLAGS carry — this is a pure C library.
CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC \
         ${ARCH_SIMD} -MMD -MP \
         -Wall -O2

OBJDIR = ${OBJ_DIR}/obj/lib/libbearssl

# All 277 BearSSL sources live in src/<category>/.  Every basename is unique,
# so a flat .PATH-based build works; gather them at configure time.
SRCS != cd ${BEARSSL}/src && find . -name '*.c' | sort
OBJS  = ${SRCS:T:.c=.o}

.PATH: ${BEARSSL}/src \
       ${BEARSSL}/src/aead ${BEARSSL}/src/codec ${BEARSSL}/src/ec \
       ${BEARSSL}/src/hash ${BEARSSL}/src/int ${BEARSSL}/src/kdf \
       ${BEARSSL}/src/mac ${BEARSSL}/src/rand ${BEARSSL}/src/rsa \
       ${BEARSSL}/src/ssl ${BEARSSL}/src/symcipher ${BEARSSL}/src/x509

# Objects live in OBJDIR, not the source dir; without this, make never finds
# the already-built .o files and rebuilds all 277 sources on every invocation.
.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libbearssl.so

${OBJ_DIR}/lib/libbearssl.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m ${ARCH_LDEMUL} -shared -soname libbearssl.so \
		${OBJS:S|^|${OBJDIR}/|} \
		-L${OBJ_DIR}/lib -lc \
		-o ${OBJ_DIR}/lib/libbearssl.so

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libbearssl.so

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
