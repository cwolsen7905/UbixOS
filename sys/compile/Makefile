# $Id: Makefile 102 2016-01-12 03:59:34Z reddawg $
# Kernel Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.incl
include ../Makefile.incl

#Objects
OBJS = null.o

#Kernel Parts
KPARTS = ../arch/${_ARCH}/*.o ../init/*.o ../sys/*.o ../vmm/*.o ../lib/*.o ../kernel/*.o ../isa/*.o ../fs/vfs/*.o ../pci/*.o ../fs/devfs/*.o ../mpi/*.o ../fs/ufs/*.o ../fs/common/*.o ../net/net/*.o ../net/netif/*.o ../net/api/*.o ../net/core/*.o ../net/core/ipv4/*.o ../sde/*.o
# ../net/core/ipv6/*.o 
# ../fs/ubixfs/*.o
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
