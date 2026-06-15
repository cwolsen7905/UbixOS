# (C) 2002-2026 The UbixOS Project
# libhttp — HTTP/1.0 + HTTPS client over lwIP sockets and BearSSL.

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
BEARSSL  = ${SRCTOP}/contrib/bearssl

# Per-arch knobs: i386 disables x86 SIMD + links elf_i386; aarch64 uses its musl
# arch headers + the aarch64 ld emulation, no SSE flags.
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

INCLUDES = -I${SRCTOP}/include \
           -I${BEARSSL}/inc \
           -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           ${ARCH_MUSL_INC} \
           -I${MUSL_SRC}/arch/generic

CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC \
         ${ARCH_SIMD} -MMD -MP \
         -Wall -O2

OBJDIR = ${OBJ_DIR}/obj/lib/libhttp
OBJS   = http.o

.PATH: ${.CURDIR}
.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libhttp.so

# Links libbearssl.so (TLS) and libc; both resolved from /lib at load time.
${OBJ_DIR}/lib/libhttp.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m ${ARCH_LDEMUL} -shared -soname libhttp.so \
		${OBJS:S|^|${OBJDIR}/|} \
		-L${OBJ_DIR}/lib -lbearssl -lc \
		-o ${OBJ_DIR}/lib/libhttp.so

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libhttp.so

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
