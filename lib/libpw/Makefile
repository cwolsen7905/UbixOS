# (C) 2002-2026 The UbixOS Project
# libpw — password key-derivation (PBKDF2-HMAC-SHA256) built on BearSSL.
# Thin policy layer; the crypto primitives live in libbearssl.so (analogous to
# macOS libpw-style code calling CommonCrypto rather than reimplementing SHA).

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
BEARSSL  = ${SRCTOP}/contrib/bearssl

.if ${_ARCH} == "aarch64"
ARCH_SIMD     =
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/aarch64
ARCH_LDEMUL   = aarch64elf
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

OBJDIR = ${OBJ_DIR}/obj/lib/libpw
OBJS   = pbkdf2.o pwhash.o

.PATH: ${.CURDIR}
.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libpw.so

# Links libbearssl.so (br_hmac/br_sha256) and libc; both become NEEDED entries
# resolved from /lib at load time.
${OBJ_DIR}/lib/libpw.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m ${ARCH_LDEMUL} -shared -soname libpw.so \
		${OBJS:S|^|${OBJDIR}/|} \
		-L${OBJ_DIR}/lib -lbearssl -lc \
		-o ${OBJ_DIR}/lib/libpw.so

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libpw.so

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
