# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# Kernel Makefile (C) 2002 The UbixOS Project

Makefile.incl
Makefile.incl

#Delete Program
REMOVE = rm -f

#Objects
OBJS = objgfx40.o ogFont.o ogSprite.o ogBlit.o ogPixCon.o ogPixelFmt.o

#Include
INCLUDE = -I./ -I../../lib/libc/include -I../../lib/libcpp/include

#Output
OUTPUT = objgfx40.so

$(OUTPUT): $(OBJS)
	$(CC) -nostdlib -shared -Wl,-soname,$(OUTPUT) -o $(OUTPUT) $(OBJS)

# Compile the source files
.cpp.o:
	$(CXX)  -Wall -g -nostdlib -fno-inline -fno-exceptions -DNOBOOL $(INCLUDE) -c -o $@ $<
 
.cc.o:
	$(CXX)  -Wall -nostdlib -fomit-frame-pointer -O -nobuilting -I../../lib/libc/include -I./include -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -O -I../../lib/libc/include -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -fomit-frame-pointer -O -I../../lib/libc/include -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -fomit-frame-pointer -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(BINARY) $(OUTPUT) *.core
