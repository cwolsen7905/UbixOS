# Copyright (c) 2002-2026 The UbixOS Project.
# lib/crt — 64-bit GCC runtime helpers missing from the x86_64-elf cross-compiler's libgcc.

include ../../Makefile.incl
include ../Makefile.incl

OBJS   = compiler_rt.o
OBJDIR = ${OBJ_DIR}/obj/lib/crt

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) ${CROSS_M32} -nostdlib -nostdinc -fno-builtin \
	    -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -O2 \
	    -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} \
	    -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all:
	@mkdir -p ${OBJDIR}
	$(CC) ${CROSS_M32} -nostdlib -nostdinc -fno-builtin \
	    -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -O2 \
	    -c -o ${OBJDIR}/compiler_rt.o compiler_rt.c
	$(AR) rcs ${OBJ_DIR}/lib/crt.a ${OBJDIR}/compiler_rt.o

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/crt.a

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
