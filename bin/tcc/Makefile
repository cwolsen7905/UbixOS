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

# Compiler flags — strip clang-only flag before adding TCC defines
TCC_CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin \
  -mno-sse -mno-sse2 -mno-mmx -mno-3dnow \
  ${INCLUDES} -w -O2 \
  -DONE_SOURCE \
  -DTCC_TARGET_I386 \
  -DCONFIG_TCC_STATIC \
  -DCONFIG_TCCDIR=\"sys:/lib/tcc\" \
  -DCONFIG_TCC_SYSINCLUDEPATHS=\"sys:/include:sys:/lib/tcc/include\" \
  -DCONFIG_TCC_LIBPATHS=\"sys:/lib:sys:/lib/tcc\" \
  -DCONFIG_TCC_CRTPREFIX=\"sys:/lib/tcc\"

# Main TCC object
OBJS = tcc_main.o

# Shim objects: missing libc symbols needed by TCC
SHIM_OBJS = shim_syscalls.o shim_stdio.o shim_stdlib.o \
            shim_math.o shim_time.o shim_moddi3.o shim_misc.o

${BINARY}: ${OBJS} ${SHIM_OBJS}
	${CC} -m32 -nostdlib ${LDFLAGS} -o ${BUILD_DIR}/bin/${BINARY} ${STARTUP} \
	  ${LIBRARIES} ${SHIM_OBJS} ${OBJS}
	${CC} ${CFLAGS} -w -O2 -c ${TCC_SRC}/lib/libtcc1.c -o libtcc1.o
	${CC} ${CFLAGS} -w -O2 -c ${TCC_SRC}/lib/alloca86.S -o alloca86.o
	${CROSS_PREFIX}ar rcs ${BUILD_DIR}/lib/tcc/libtcc1.a libtcc1.o alloca86.o shim_moddi3.o
	mkdir -p ${BUILD_DIR}/lib/tcc/include
	cp ${TCC_SRC}/include/*.h ${BUILD_DIR}/lib/tcc/include/

tcc_main.o:
	${CC} ${TCC_CFLAGS} -c ${TCC_SRC}/tcc.c -o tcc_main.o

SHIM_CC = ${CC} -m32 -nostdlib -nostdinc -fno-builtin \
          -mno-sse -mno-sse2 -mno-mmx -mno-3dnow ${INCLUDES} -w -O2

shim_syscalls.o:
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/syscalls.S -o shim_syscalls.o

shim_stdio.o:
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/stdio_ext.c -o shim_stdio.o

shim_stdlib.o:
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/stdlib_ext.c -o shim_stdlib.o

shim_math.o:
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/math_ext.c -o shim_math.o

shim_time.o:
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/time_ext.c -o shim_time.o

shim_moddi3.o:
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/moddi3.c -o shim_moddi3.o

shim_misc.o:
	${SHIM_CC} -c ${TCC_SRC}/ubixos_shim/misc_ext.c -o shim_misc.o

clean:
	${REMOVE} ${OBJS} ${SHIM_OBJS} libtcc1.o alloca86.o
	${REMOVE} ${BUILD_DIR}/bin/${BINARY}
	${REMOVE} ${BUILD_DIR}/lib/tcc/libtcc1.a
