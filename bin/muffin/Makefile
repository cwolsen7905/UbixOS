# $Id: Makefile 119 2016-01-14 03:06:36Z reddawg $
# Application Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../Makefile.incl
#include ../Makefile.incl

#Compiler Flags
CFLAGS =
# -DNOBOOL -fno-builtin -fno-exceptions

#Linker
LD = ld

#Binary File Name
BINARY = muffin

#Delete Program
REMOVE = rm -f

#Objects
OBJS = main.o

#Libraries
LIBRARIES2 = ../../lib/objgfx/*.o -lstdc++ -lm

#Include
INCLUDE = -I../../lib/objgfx

STARTUP=

# Link The Binary
$(BINARY) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(LIBRARIES) $(LIBRARIES2) $(OBJS)
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
	$(REMOVE) $(OBJS) ${BUILD_DIR}/bin/$(BINARY)
