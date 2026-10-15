# (C) 2002-2026 The UbixOS Project
# libpkg — package format library (ustar+gzip reader, manifest/index parser,
# SHA-256 and ECDSA P-256 verify).  Static archive; links zlib and BearSSL at
# the consumer.  See docs/design/packages-plan.md.

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
BEARSSL  = ${SRCTOP}/contrib/bearssl
ZLIB     = ${SRCTOP}/contrib/zlib

.if ${_ARCH} == "aarch64"
ARCH_SIMD     =
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/aarch64
.elif ${_ARCH} == "x86_64"
ARCH_SIMD     = -mno-sse -mno-sse2 -mno-mmx -mno-3dnow
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/x86_64
.else
ARCH_SIMD     = -mno-sse -mno-sse2 -mno-mmx -mno-3dnow
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/i386
.endif

# musl's headers come before ${SRCTOP}/include: zlib.h pulls <sys/types.h>, and
# the tree's own would shadow musl's.  <ubix/pkg.h> is unambiguous either way.
INCLUDES = -I${BEARSSL}/inc \
           -I${ZLIB} \
           -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           ${ARCH_MUSL_INC} \
           -I${MUSL_SRC}/arch/generic \
           -I${SRCTOP}/include

CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC \
         ${ARCH_SIMD} -MMD -MP \
         -Wall -O2

OBJDIR = ${OBJ_DIR}/obj/lib/libpkg
OBJS   = hash.o kv.o version.o archive.o manifest.o index.o

.PATH: ${.CURDIR}
.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libpkg.a

${OBJ_DIR}/lib/libpkg.a: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(AR) rcs ${OBJ_DIR}/lib/libpkg.a ${OBJS:S|^|${OBJDIR}/|}

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libpkg.a

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
