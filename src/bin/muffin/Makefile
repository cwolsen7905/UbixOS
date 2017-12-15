# $Id: Makefile 119 2016-01-14 03:06:36Z reddawg $
# Application Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.incl
include ../Makefile.incl

#Compiler Flags
CFLAGS = -DNOBOOL -fno-builtin -fno-exceptions -nostdlib -nostdinc

#Linker
LD = ld

#Binary File Name
BINARY = muffin

#Delete Program
REMOVE = rm -f

#Objects
OBJS = main.o

#Libraries
LIBRARIES2 = ../../lib/objgfx40/*.o ../../lib/libcpp/*.o

#Include
INCLUDE = -I../../lib/libc/include -I../../lib/libcpp/include -I../../lib/objgfx40

# Link The Binary
$(BINARY) : $(OBJS)
	$(CC) $(CFLAGS) -o ../../build/bin/$@ $(STARTUP) $(LIBRARIES) $(LIBRARIES2) $(OBJS)
	strip $(BINARY)

# Compile the source files
.cc.o:
	$(CXX)  -Wall -fomit-frame-pointer -O $(CFLAGS) $(INCLUDE) -c -o $@ $<
 
.cc.s:
	$(CXX)  -Wall -fomit-frame-pointer -O $(CFLAGS) $(INCLUDE) -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -O $(CFLAGS) $(INCLUDE) -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -fomit-frame-pointer -O $(CFLAGS) $(INCLUDE) -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -fomit-frame-pointer $(INCLUDE) -c -o $@ $<
 
# Clean Up The junk
clean:
	$(REMOVE) $(OBJS) ../../build/bin/$(BINARY)
