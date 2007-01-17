# $Id$
# Kernel Makefile (C) 2002 The UbixOS Project

include ../../../Makefile.inc

#Objects
OBJS = sStyle.o sTypes.o vContext.o vButton.o vMenuBar.o vView.o vCanvas.o vDesktop.o vTitleTab.o

#Include
INCLUDE = -I./ -I../../..lib/libc/include -I../../../lib/libcpp/include -I../../objgfx40/objgfx40 -I./include

#Output
OUTPUT = views.so

$(OUTPUT): $(OBJS)
	$(CXX) -nostdlib -shared -Wl,-soname,$(OUTPUT) -o $(OUTPUT) $(OBJS)

# Compile the source files
.cpp.o:
	$(CXX)  -Wall -g -fno-inline -nostdlib -fno-exceptions -DNOBOOL $(INCLUDE) -c -o $@ $<
 
.cc.o:
	$(CXX)  -Wall -nostdlib -fomit-frame-pointer -O -I./include -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -O -I../../../lib/libc/include -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -fomit-frame-pointer -O -I../../../lib/libc/include -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -fomit-frame-pointer -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT) *.core
