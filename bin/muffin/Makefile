# $Id: Makefile 119 2016-01-14 03:06:36Z reddawg $
# Application Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../Makefile.incl
include ../Makefile.incl

#Compiler Flags
CFLAGS = -std=c++11
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
LIBRARIES2=../../lib/objgfx/*.o -lstdc++ -lm

#Include
INCLUDE2=-I../../lib/objgfx

STARTUP=

# Link The Binary
$(BINARY) : $(OBJS)
	$(CC) -static $(CFLAGS) -o ${BUILD_DIR}/bin/$@ $(LIBRARIES2) $(OBJS)
	#strip $(BINARY)

# Compile the source files
.cc.o:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDE2) -c -o $@ $<
 
.cc.s:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDE2) -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -O $(CFLAGS) $(INCLUDE) -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -fomit-frame-pointer -O $(CFLAGS) $(INCLUDE) -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -fomit-frame-pointer $(INCLUDE) -c -o $@ $<
 
# Clean Up The junk
clean:
	$(REMOVE) $(OBJS) ${BUILD_DIR}/bin/$(BINARY)
