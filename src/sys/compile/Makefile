# $Id$
# Kernel Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Objects
OBJS = null.o

#Kernel Parts
KPARTS = ../init/*.o ../sys/*.o ../vmm/*.o ../lib/*.o ../kernel/*.o ../isa/*.o ../vfs/*.o ../pci/*.o ../devfs/*.o ../mpi/*.o ../ufs/*.o
# ../sde/*.o ../graphics/*.o ../ld/*.o  -Ttext 0x30000 -Tdata 0x34000 ../ubixfs/*.o

# Link the kernel statically with fixed text+data address @1M
$(KERNEL) : $(OBJS)
	#$(LD)  -nostdlib -nostdinc --warn-section-align -o $@ $(OBJS) $(KPARTS) -Ttext 0x30000 -Tdata 0x20000
	$(LD)  -T ./ldscript.i386 -o $@ $(OBJS) $(KPARTS)
	#/usr/bin/strip $@

# Compile the source files
.cc.o:
	$(CXX) -${CFLAGS} $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) ${CFLAGS} $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES)  -c -o $@ $<
 
.c.s:
	$(CC) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(KERNEL) null.c
