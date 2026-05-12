# Makefile for TCC (Tiny C Compiler) — UbixOS i386 cross-build
# Built from contrib/tcc/tcc.c (version 0.9.26)
#
# Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.

include ../../Makefile.incl
include ../Makefile.incl

# Cross-compiler: overridden by command-line CC= from 'bmake world'
.if !defined(CC) || ${CC} == "cc"
CC  = ${CROSS_PREFIX}gcc
.endif
AR  ?= ${CROSS_PREFIX}ar

BINARY   = tcc
TCC_SRC  = ../../contrib/tcc
OBJ_OUT  = ${BUILD_DIR}/obj/bin/tcc

# Compiler flags — strip clang-only flag before adding TCC defines
TCC_CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin \
  -mno-sse -mno-sse2 -mno-mmx -mno-3dnow \
  ${INCLUDES} -w -O2 \
  -DONE_SOURCE \
  -DTCC_TARGET_I386 \
  -DCONFIG_TCC_STATIC \
  -DCONFIG_TCCDIR=\"/lib/tcc\" \
  -DCONFIG_TCC_SYSINCLUDEPATHS=\"/include:/lib/tcc/include\" \
  -DCONFIG_TCC_LIBPATHS=\"/lib:/lib/tcc\" \
  -DCONFIG_TCC_CRTPREFIX=\"/lib/tcc\" \
  -DCONFIG_TCC_ELFINTERP=\"sys:/libexec/ld.so\"

# Main TCC object
OBJS = ${OBJ_OUT}/tcc_main.o

# Shim objects: missing libc symbols needed by TCC
SHIM_OBJS = ${OBJ_OUT}/shim_syscalls.o ${OBJ_OUT}/shim_stdio.o ${OBJ_OUT}/shim_stdlib.o \
            ${OBJ_OUT}/shim_math.o ${OBJ_OUT}/shim_time.o ${OBJ_OUT}/shim_moddi3.o \
            ${OBJ_OUT}/shim_misc.o

${BINARY}: ${OBJS} ${SHIM_OBJS}
	@mkdir -p ${BUILD_DIR}/lib/tcc
	${CC} -m32 -nostdlib ${LDFLAGS} -o ${BUILD_DIR}/bin/${BINARY} ${STARTUP} \
	  ${LIBRARIES} ${SHIM_OBJS} ${OBJS}
	${CC} ${CFLAGS} -w -O2 -c ${TCC_SRC}/lib/libtcc1.c -o ${OBJ_OUT}/libtcc1.o
	${CC} ${CFLAGS} -w -O2 -c ${TCC_SRC}/lib/alloca86.S -o ${OBJ_OUT}/alloca86.o
	${CROSS_PREFIX}ar rcs ${BUILD_DIR}/lib/tcc/libtcc1.a \
	  ${OBJ_OUT}/libtcc1.o ${OBJ_OUT}/alloca86.o ${OBJ_OUT}/shim_moddi3.o
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

SHIM_CC = ${CC} -m32 -nostdlib -nostdinc -fno-builtin \
          -mno-sse -mno-sse2 -mno-mmx -mno-3dnow ${INCLUDES} -w -O2

${OBJ_OUT}/shim_syscalls.o: ${TCC_SRC}/ubixos_shim/syscalls.S
	@mkdir -p ${OBJ_OUT}
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/syscalls.S -o ${OBJ_OUT}/shim_syscalls.o

${OBJ_OUT}/shim_stdio.o: ${TCC_SRC}/ubixos_shim/stdio_ext.c
	@mkdir -p ${OBJ_OUT}
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/stdio_ext.c -o ${OBJ_OUT}/shim_stdio.o

${OBJ_OUT}/shim_stdlib.o: ${TCC_SRC}/ubixos_shim/stdlib_ext.c
	@mkdir -p ${OBJ_OUT}
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/stdlib_ext.c -o ${OBJ_OUT}/shim_stdlib.o

${OBJ_OUT}/shim_math.o: ${TCC_SRC}/ubixos_shim/math_ext.c
	@mkdir -p ${OBJ_OUT}
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/math_ext.c -o ${OBJ_OUT}/shim_math.o

${OBJ_OUT}/shim_time.o: ${TCC_SRC}/ubixos_shim/time_ext.c
	@mkdir -p ${OBJ_OUT}
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/time_ext.c -o ${OBJ_OUT}/shim_time.o

${OBJ_OUT}/shim_moddi3.o: ${TCC_SRC}/ubixos_shim/moddi3.c
	@mkdir -p ${OBJ_OUT}
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/moddi3.c -o ${OBJ_OUT}/shim_moddi3.o

${OBJ_OUT}/shim_misc.o: ${TCC_SRC}/ubixos_shim/misc_ext.c
	@mkdir -p ${OBJ_OUT}
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/misc_ext.c -o ${OBJ_OUT}/shim_misc.o

clean:
	${REMOVE} ${OBJ_OUT}
	${REMOVE} ${BUILD_DIR}/bin/${BINARY}
	${REMOVE} ${BUILD_DIR}/lib/tcc/libtcc1.a
