# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../Makefile.inc
#include ../../Makefile.inc
#include ../Makefile.inc
include ../libc.inc

#Objects
#OBJS = anddi3.o adddi3.o qdivrem.o umoddi3.o udivdi3.o divdi3.o
#OBJS = adddi3.o anddi3.o ashldi3.o ashrdi3.o cmpdi2.o divdi3.o fixdfdi.o fixsfdi.o fixunsdfdi.o fixunssfdi.o floatdidf.o floatdisf.o floatunsdidf.o iordi3.o lshldi3.o lshrdi3.o moddi3.o muldi3.o negdi2.o notdi2.o qdivrem.o subdi3.o ucmpdi2.o udivdi3.o umoddi3.o xordi3.o

OBJS = cmpdi2.o divdi3.o moddi3.o qdivrem.o ucmpdi2.o udivdi3.o umoddi3.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

