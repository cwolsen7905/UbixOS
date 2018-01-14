#  (C) 2002 The UbixOS Project
#  $Id: Makefile 54 2016-01-11 01:29:55Z reddawg $

# Include Global 'Source' Options
include ../../Makefile.incl
include ../Makefile.incl

INCLUDES += -I../../lib/objgfx40/

#  Objects
OBJS = strstr.o strlen.o strncpy.o assert.o sqrt.o atan.o divdi3.o libcpp.o strtok.o kmalloc.o kprintf.o vsprintf.o string.o net.o strtol.o
#ogprintf.o

all: $(OBJS)

#  Compile Types
.cpp.o:
	$(CXX) -DNOBOOL $(CFLAGS) -fno-rtti $(INCLUDES) -c -o $@ $<
.cc.o:
	$(CXX) -DNOBOOL -D__UBIXOS_KERNEL__ $(CFLAGS) -fno-rtti $(INCLUDES) -c -o $@ $<
.cc.s:
	$(CXX) $(CFLAGS) $(INCLUDES) -S -o $@ $<
.c.o:
	$(CC)  $(CFLAGS) $(INCLUDES) -c -o $@ $<
.c.s:
	$(CC)  $(CFLAGS) $(INCLUDES) -S -o $@ $<
.S.o:
	$(CC)  $(CFLAGS) -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS)
