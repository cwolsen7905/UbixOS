# Makefile for TCC (Tiny C Compiler)
#
#   Both architectures build contrib/tinycc (upstream 0.9.28rc, see
#   UBIXOS_UPSTREAM_REV) and emit dynamic musl programs:
#   aarch64: tcc is itself a dynamic musl PIE; output is PIE (the loader's
#            ET_DYN path), interpreter /lib/ld-musl-aarch64.so.1
#   i386:    tcc is a dynamic musl ET_EXEC; output is ET_EXEC, interpreter
#            /lib/ld-musl-i386.so.1
#
# Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.

include ../../Makefile.incl
include ../Makefile.incl

.if !defined(CC) || ${CC} == "cc"
CC  = ${CROSS_PREFIX}gcc
.endif
AR  ?= ${CROSS_PREFIX}ar

BINARY   = tcc
OBJ_OUT  = ${OBJ_DIR}/obj/bin/tcc
MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl

.if ${_ARCH} == "aarch64"
# ---- aarch64: upstream tinycc --------------------------------------------
# contrib/tinycc/config.h + tccdefs_.h are generated once on the host
# (./configure --cpu=arm64 --config-musl ...; make tccdefs_.h) and committed.
TCC_SRC  = ${SRCTOP}/contrib/tinycc
GCC_INC  != dirname `${CC} -print-libgcc-file-name`
TCC_INC  = -I${TCC_SRC} -I${MUSL_SRC}/include -I${MUSL_OBJ}/obj/include \
           -I${MUSL_SRC}/arch/aarch64 -I${MUSL_SRC}/arch/generic -I${GCC_INC}/include
TCC_CFLAGS = -nostdlib -nostdinc -fno-builtin -fPIC -O2 -w ${TCC_INC}
LIBTCC1_SRCS = lib-arm64.c stdatomic.c builtin.c armflush.c dsohandle.c
LIBTCC1_ASMS = atomic.S

${BINARY}: ${OBJ_OUT}/tcc.o
	@mkdir -p ${OBJ_DIR}/bin ${OBJ_DIR}/lib/tcc/include
	${CC} -nostdlib -pie -Wl,-m,aarch64elf -Wl,-dynamic-linker,/lib/ld-musl-aarch64.so.1 \
	  -Wl,-rpath,/lib -Wl,-z,noexecstack \
	  ${MUSL_OBJ}/lib/Scrt1.o ${MUSL_OBJ}/lib/crti.o ${OBJ_OUT}/tcc.o \
	  -L${OBJ_DIR}/lib -lc ${LIBGCC} ${MUSL_OBJ}/lib/crtn.o -o ${OBJ_DIR}/bin/${BINARY}
.for _s in ${LIBTCC1_SRCS} ${LIBTCC1_ASMS}
	${CC} ${TCC_CFLAGS} -DTCC_TARGET_ARM64 -D__arm64_clear_cache=__builtin___clear_cache -c ${TCC_SRC}/lib/${_s} -o ${OBJ_OUT}/lib-${_s:R}.o
.endfor
	${AR} rcs ${OBJ_DIR}/lib/tcc/libtcc1.a ${OBJ_OUT}/lib-*.o
	${CC} ${TCC_CFLAGS} -DTCC_TARGET_ARM64 -c ${TCC_SRC}/lib/runmain.c -o ${OBJ_DIR}/lib/tcc/runmain.o
	cp ${TCC_SRC}/include/*.h ${OBJ_DIR}/lib/tcc/include/

${OBJ_OUT}/tcc.o: ${TCC_SRC}/tcc.c ${TCC_SRC}/config.h ${TCC_SRC}/tccdefs_.h
	@mkdir -p ${OBJ_OUT}
	${CC} ${TCC_CFLAGS} -DONE_SOURCE=1 -c ${TCC_SRC}/tcc.c -o ${OBJ_OUT}/tcc.o

clean:
	${REMOVE} ${OBJ_OUT} ${OBJ_DIR}/bin/${BINARY} ${OBJ_DIR}/lib/tcc

.else
# ---- i386: the same upstream tinycc, i386 target --------------------------
# contrib/tinycc/config.h only applies its arm64 settings when no target is
# predefined, so the i386 settings come in as -D flags.  Unlike aarch64 the
# i386 loader maps ET_EXEC at its link address, so no PIE.  tcc is a dynamic
# musl program on both arches: `tcc -run` dlopen()s libc.so, which a static
# binary cannot.
TCC_SRC  = ${SRCTOP}/contrib/tinycc
SHIM_SRC = ${SRCTOP}/contrib/tcc/ubixos_shim
MUSL_INC = -I${MUSL_SRC}/include -I${MUSL_OBJ}/obj/include \
           -I${MUSL_SRC}/arch/i386 -I${MUSL_SRC}/arch/generic
GCC_INC  != dirname `${CC} -m32 -print-libgcc-file-name`
I386_CF  = -m32 -nostdlib -nostdinc -fno-builtin -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -w -O2
TCC_DEFS = -DTCC_TARGET_I386=1 -DCONFIG_TCC_MUSL=1 \
           -DCONFIG_TCC_SYSINCLUDEPATHS=\"/lib/tcc/include:/usr/include\" \
           -DCONFIG_TCC_LIBPATHS=\"/lib/tcc:/usr/lib:/lib\" \
           -DCONFIG_TCC_CRTPREFIX=\"/usr/lib\" \
           -DCONFIG_TCC_ELFINTERP=\"/lib/ld-musl-i386.so.1\"
TCC_CFLAGS = ${I386_CF} -I${TCC_SRC} ${MUSL_INC} -I${GCC_INC}/include ${TCC_DEFS}
LIBTCC1_SRCS = libtcc1.c stdatomic.c builtin.c dsohandle.c
LIBTCC1_ASMS = alloca.S alloca-bt.S atomic.S

${BINARY}: ${OBJ_OUT}/tcc.o ${OBJ_OUT}/shim_moddi3.o
	@mkdir -p ${OBJ_DIR}/bin ${OBJ_DIR}/lib/tcc/include
	${CC} -m32 -nostdlib -Wl,-m,elf_i386 -Wl,-dynamic-linker,/lib/ld-musl-i386.so.1 \
	  -Wl,-rpath,/lib -Wl,-z,noexecstack \
	  ${MUSL_OBJ}/lib/crt1.o ${MUSL_OBJ}/lib/crti.o ${OBJ_OUT}/tcc.o ${OBJ_OUT}/shim_moddi3.o \
	  -Wl,--start-group -L${OBJ_DIR}/lib -lc ${OBJ_DIR}/lib/libgcc32.a ${LIBGCC} -Wl,--end-group \
	  ${MUSL_OBJ}/lib/crtn.o -o ${OBJ_DIR}/bin/${BINARY}
.for _s in ${LIBTCC1_SRCS} ${LIBTCC1_ASMS}
	${CC} ${TCC_CFLAGS} -Wa,--noexecstack -c ${TCC_SRC}/lib/${_s} -o ${OBJ_OUT}/lib-${_s:R}.o
.endfor
	${AR} rcs ${OBJ_DIR}/lib/tcc/libtcc1.a ${OBJ_OUT}/lib-*.o
	${CC} ${TCC_CFLAGS} -c ${TCC_SRC}/lib/runmain.c -o ${OBJ_DIR}/lib/tcc/runmain.o
	cp ${TCC_SRC}/include/*.h ${OBJ_DIR}/lib/tcc/include/

${OBJ_OUT}/tcc.o: ${TCC_SRC}/tcc.c ${TCC_SRC}/config.h ${TCC_SRC}/tccdefs_.h
	@mkdir -p ${OBJ_OUT}
	${CC} ${TCC_CFLAGS} -DONE_SOURCE=1 -c ${TCC_SRC}/tcc.c -o ${OBJ_OUT}/tcc.o

# The hand-rolled i386 libgcc lacks the 64-bit division helpers tcc itself uses.
${OBJ_OUT}/shim_moddi3.o: ${SHIM_SRC}/moddi3.c
	@mkdir -p ${OBJ_OUT}
	${CC} ${I386_CF} ${MUSL_INC} -c ${SHIM_SRC}/moddi3.c -o ${OBJ_OUT}/shim_moddi3.o

clean:
	${REMOVE} ${OBJ_OUT} ${OBJ_DIR}/bin/${BINARY} ${OBJ_DIR}/lib/tcc
.endif
