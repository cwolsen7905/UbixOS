# Makefile for clock — Phase 2 musl static binary
# (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY   = clock
MUSL_LIB = ${OBJ_DIR}/obj/musl/lib
MUSL_SRC = ${SRCTOP}/contrib/musl

# Override includes: musl public headers + generated bits/ (alltypes.h, syscall.h)
MUSL_INC = -I${MUSL_SRC}/include \
           -I${OBJ_DIR}/obj/musl/obj/include \
           -I${MUSL_SRC}/arch/i386 \
           -I${MUSL_SRC}/arch/generic

# Override CFLAGS to drop -fno-exceptions/-fno-rtti (C only) and set musl paths
MUSL_CFLAGS = ${CROSS_M32} -nostdlib -nostdinc -fno-builtin \
              -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP \
              -Wall -O

OBJDIR = ${OBJ_DIR}/obj/bin/clock

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	${CC} ${MUSL_CFLAGS} ${MUSL_INC} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/bin/${BINARY}

${OBJ_DIR}/bin/${BINARY}: main.o
	@mkdir -p ${OBJ_DIR}/bin
	${CC} ${CROSS_M32} -nostdlib -static -Wl,-m,elf_i386 \
		${MUSL_LIB}/crt1.o \
		${MUSL_LIB}/crti.o \
		${OBJDIR}/main.o \
		${OBJ_DIR}/lib/musl.a \
		${OBJ_DIR}/lib/crt.a \
		${LIBGCC} \
		${MUSL_LIB}/crtn.o \
		-o ${OBJ_DIR}/bin/${BINARY}

clean:
	rm -rf ${OBJDIR} ${OBJ_DIR}/bin/${BINARY}

.for _d in main.d
.sinclude "${OBJDIR}/${_d}"
.endfor
