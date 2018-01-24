# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# Kernel Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../Makefile.incl
#include ../Makefile.incl

#Delete Program
REMOVE = rm -f

#Objects
OBJS = objgfx.o ogEdgeTable.o ogImage.o ogPixCon.o ogPixelFmt.o vWindow.o ogFont.o vWidget.o
#ogBlit.o

#Include
INCLUDE = -I./objgfx
# -I../../lib/libc/include -I../../lib/libcpp/include

#Output
OUTPUT = objgfx40.so

$(OUTPUT): $(OBJS)
	$(CC) -nostdlib -shared -Wl,-soname,$(OUTPUT) -o $(OUTPUT) $(OBJS)

# Compile the source files
.cpp.o:
	$(CXX)  -Wall -g -fno-inline -fno-exceptions -std=c++11 $(INCLUDE) -c -o $@ $<
 
.cc.o:
	$(CXX)  -Wall -fomit-frame-pointer -std=c++11 -c -o $@ $<
 
.c.o:
	$(CC)  -Wall -O -I../../lib/libc/include -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -fomit-frame-pointer -O -I../../lib/libc/include -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -fomit-frame-pointer -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(BINARY) $(OUTPUT) *.core
