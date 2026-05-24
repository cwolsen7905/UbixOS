# Copyright (c) 2002-2026 The UbixOS Project.

include ../../Makefile.incl
include ../Makefile.incl

OBJS   = fb.o font8x8.o
OUTPUT = libfb.so

OBJDIR ?= ${OBJ_DIR}/obj/lib/libfb

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) -Wall -nostdinc -O -I../../include -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

_OBJS_FULL = ${OBJS:S|^|${OBJDIR}/|}

lib.so: $(OBJS)
	$(CC) -m32 -nostdlib -shared -Wl,-soname,$(OUTPUT) -Wl,-m,elf_i386 \
	    -o ${BUILD_DIR}/lib/$(OUTPUT) ${_OBJS_FULL}

all: lib.so

clean:
	$(REMOVE) ${OBJDIR} ${BUILD_DIR}/lib/$(OUTPUT)

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
