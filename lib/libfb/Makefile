# Copyright (c) 2002-2026 The UbixOS Project.

include ../../Makefile.incl
include ../Makefile.incl

OBJS   = fb.o font8x8.o
OUTPUT = libfb.so

lib.so: $(OBJS)
	$(CC) -m32 -nostdlib -shared -Wl,-soname,$(OUTPUT) -Wl,-m,elf_i386 \
	    -o ${BUILD_DIR}/lib/$(OUTPUT) $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -Wall -nostdinc -O -I../../include -c $<

clean:
	$(REMOVE) $(OBJS) ${OBJS:.o=.d} ${BUILD_DIR}/lib/$(OUTPUT)

.for _d in ${OBJS:.o=.d}
.sinclude "${_d}"
.endfor
