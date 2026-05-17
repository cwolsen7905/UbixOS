# Makefile for TCC (Tiny C Compiler) — UbixOS i386 cross-build
# Built from contrib/tcc/tcc.c (version 0.9.26)
#
# Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.

include ../../Makefile.incl
include ../Makefile.incl

.if !defined(CC) || ${CC} == "cc"
CC  = ${CROSS_PREFIX}gcc
.endif
AR  ?= ${CROSS_PREFIX}ar

BINARY   = tcc
TCC_SRC  = ../../contrib/tcc
OBJ_OUT  = ${OBJ_DIR}/obj/bin/tcc

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
MUSL_INC = -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           -I${MUSL_SRC}/arch/i386 \
           -I${MUSL_SRC}/arch/generic

TCC_CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin \
  -mno-sse -mno-sse2 -mno-mmx -mno-3dnow \
  ${MUSL_INC} -w -O2 \
  -DONE_SOURCE \
  -DTCC_TARGET_I386 \
  -DCONFIG_TCC_STATIC \
  -DCONFIG_TCCDIR=\"/lib/tcc\" \
  -DCONFIG_TCC_SYSINCLUDEPATHS=\"/include:/lib/tcc/include\" \
  -DCONFIG_TCC_LIBPATHS=\"/lib:/lib/tcc\" \
  -DCONFIG_TCC_CRTPREFIX=\"/lib/tcc\" \
  -DCONFIG_TCC_ELFINTERP=\"sys:/libexec/ld.so\"

OBJS     = ${OBJ_OUT}/tcc_main.o
MODDI3   = ${OBJ_OUT}/shim_moddi3.o

${BINARY}: ${OBJS} ${MODDI3}
	@mkdir -p ${BUILD_DIR}/lib/tcc
	${CC} -m32 -nostdlib -Wl,-m,elf_i386 \
	  ${MUSL_OBJ}/lib/crt1.o \
	  ${MUSL_OBJ}/lib/crti.o \
	  ${OBJS} ${MODDI3} \
	  ${OBJ_DIR}/lib/musl.a \
	  ${LIBGCC} \
	  ${MUSL_OBJ}/lib/crtn.o \
	  -o ${BUILD_DIR}/bin/${BINARY}
	${CC} ${TCC_CFLAGS} -c ${TCC_SRC}/lib/libtcc1.c -o ${OBJ_OUT}/libtcc1.o
	${CC} -m32 -nostdlib -nostdinc -fno-builtin \
	  -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -w \
	  -c ${TCC_SRC}/lib/alloca86.S -o ${OBJ_OUT}/alloca86.o
	${AR} rcs ${BUILD_DIR}/lib/tcc/libtcc1.a \
	  ${OBJ_OUT}/libtcc1.o ${OBJ_OUT}/alloca86.o ${MODDI3}
	${CC} -m32 -nostdlib -nostdinc -fno-builtin \
	  -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -w \
	  -c ${TCC_SRC}/ubixos_shim/crt1.S -o ${BUILD_DIR}/lib/tcc/crt1.o
	${CC} -m32 -nostdlib -nostdinc -fno-builtin \
	  -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -w \
	  -c ${TCC_SRC}/ubixos_shim/crti.S -o ${BUILD_DIR}/lib/tcc/crti.o
	${CC} -m32 -nostdlib -nostdinc -fno-builtin \
	  -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -w \
	  -c ${TCC_SRC}/ubixos_shim/crtn.S -o ${BUILD_DIR}/lib/tcc/crtn.o
	mkdir -p ${BUILD_DIR}/lib/tcc/include
	cp ${TCC_SRC}/include/*.h ${BUILD_DIR}/lib/tcc/include/

${OBJ_OUT}/tcc_main.o:
	@mkdir -p ${OBJ_OUT}
	${CC} ${TCC_CFLAGS} -c ${TCC_SRC}/tcc.c -o ${OBJ_OUT}/tcc_main.o

${OBJ_OUT}/shim_moddi3.o: ${TCC_SRC}/ubixos_shim/moddi3.c
	@mkdir -p ${OBJ_OUT}
	${CC} -m32 -nostdlib -nostdinc -fno-builtin \
	  -mno-sse -mno-sse2 -mno-mmx -mno-3dnow \
	  ${MUSL_INC} -w -O2 \
	  -c ${TCC_SRC}/ubixos_shim/moddi3.c -o ${OBJ_OUT}/shim_moddi3.o

clean:
	${REMOVE} ${OBJ_OUT}
	${REMOVE} ${BUILD_DIR}/bin/${BINARY}
	${REMOVE} ${BUILD_DIR}/lib/tcc/libtcc1.a
	${REMOVE} ${BUILD_DIR}/lib/tcc/crt1.o ${BUILD_DIR}/lib/tcc/crti.o ${BUILD_DIR}/lib/tcc/crtn.o
