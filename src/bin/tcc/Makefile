# $Id$
# Application Makefile (C) 2002-2004 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Linker
LD = ld

#Binary File Name
BINARY = tcc 

#Delete Program
REMOVE = rm -f

#Objects
OBJS = tcc.o

CFLAGS = -O -g -Wall -mpreferred-stack-boundary=2 -march=i386 -falign-functions=0 -fno-strict-aliasing -nostdlib -nostdinc

INCLUDES = -I../../include.new -I./

LIBRARIES = ../../lib/libc/libc.so

STARTUP = ../../lib/csu/*.o

# Link The Binary
$(BINARY) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(LIBRARIES) $(OBJS)
	$(CC) -DLIBTCC -c -o libtcc.o tcc.c
	ar rcs libtcc.a libtcc.o
	$(CC) -O2 -Wall -c -o libtcc1.o libtcc1.c
	ar rcs libtcc1.a libtcc1.o

# Compile the source files
.cc.o:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDES) -c -o $@ $<

.cc.s:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC)  $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -O $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC)  -Wall $(CLFAGS) $(INCLUDES) -c -o $@ $<
 
# Clean Up The junk
clean:
	$(REMOVE) $(OBJS) $(BINARY)
