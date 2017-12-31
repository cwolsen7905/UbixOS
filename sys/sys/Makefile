#  (C) 2002 The UbixOS Project
#  $Id: Makefile 140 2016-01-17 01:36:09Z reddawg $

# Include Global 'Source' Options
include ../../Makefile.incl
include ../Makefile.incl

#  Objects
OBJS = dma.o idt.o io.o video.o device.o elf.o

all: $(OBJS)

#  Compile Types
.cc.o:
	$(CXX) ${CFLAGS} ${INCLUDES} -c -o $@ $<
.cc.s:
	$(CXX) ${CFLAGS} ${INCLUDES} -S -o $@ $<
.c.o:
	$(CC) ${CFLAGS} ${INCLUDES} -c -o $@ $<
.c.s:
	$(CC) ${CFLAGS} ${INCLUDES} -S -o $@ $<
.S.o:
	$(CC) ${CFLAGS} ${INCLUDES} -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS)
