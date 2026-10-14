# (C) 2002-2026 The UbixOS Project
# crtbegin/crtend: the constructor/destructor bracket objects clang's Linux
# driver adds around every link (crtbeginS.o/crtendS.o for PIE, crtbeginT.o for
# -static, crtbegin.o otherwise).  Built from compiler-rt's sources so a bare
# on-device `clang hello.c` links; the world's own links pass -nostdlib.
# musl runs .init_array itself (CRT_HAS_INITFINI_ARRAY) and libunwind finds
# unwind tables through PT_GNU_EH_FRAME, so no EH frame registry.

include ../../Makefile.incl
include ../Makefile.incl

CRT_SRC   = ${SRCTOP}/contrib/compiler-rt-crt
CRT_FLAGS = ${CROSS_M32} ${TC_NOSTDINC} -ffreestanding -fno-builtin -fno-stack-protector ${ARCH_NOSIMD} \
            -std=c11 -O2 -DCRT_HAS_INITFINI_ARRAY ${INCLUDES}
CRT_OBJS  = crtbegin.o crtbeginS.o crtbeginT.o crtend.o crtendS.o

all: ${CRT_OBJS:S|^|${OBJ_DIR}/lib/|}

${OBJ_DIR}/lib/crtbegin.o ${OBJ_DIR}/lib/crtbeginT.o: ${CRT_SRC}/crtbegin.c
	@mkdir -p ${OBJ_DIR}/lib
	$(CC) ${CRT_FLAGS} -fno-pic -c -o ${.TARGET} ${CRT_SRC}/crtbegin.c

${OBJ_DIR}/lib/crtbeginS.o: ${CRT_SRC}/crtbegin.c
	@mkdir -p ${OBJ_DIR}/lib
	$(CC) ${CRT_FLAGS} -fPIC -c -o ${.TARGET} ${CRT_SRC}/crtbegin.c

${OBJ_DIR}/lib/crtend.o: ${CRT_SRC}/crtend.c
	@mkdir -p ${OBJ_DIR}/lib
	$(CC) ${CRT_FLAGS} -fno-pic -c -o ${.TARGET} ${CRT_SRC}/crtend.c

${OBJ_DIR}/lib/crtendS.o: ${CRT_SRC}/crtend.c
	@mkdir -p ${OBJ_DIR}/lib
	$(CC) ${CRT_FLAGS} -fPIC -c -o ${.TARGET} ${CRT_SRC}/crtend.c

clean:
	$(REMOVE) ${CRT_OBJS:S|^|${OBJ_DIR}/lib/|}
