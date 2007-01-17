#  (C) 2002 The UbixOS Project
#  $Id$

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#  Objects
OBJS = dma.o idt.o io.o video.o device.o

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
