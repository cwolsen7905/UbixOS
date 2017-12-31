# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.incl
include ../../Makefile.incl
include ../Makefile.incl

#Objects
#OBJS = anddi3.o adddi3.o qdivrem.o umoddi3.o udivdi3.o divdi3.o
OBJS = adddi3.o anddi3.o ashldi3.o ashrdi3.o cmpdi2.o divdi3.o fixdfdi.o fixsfdi.o fixunsdfdi.o fixunssfdi.o floatdidf.o floatdisf.o floatunsdidf.o iordi3.o lshldi3.o lshrdi3.o moddi3.o muldi3.o negdi2.o notdi2.o qdivrem.o subdi3.o ucmpdi2.o udivdi3.o umoddi3.o xordi3.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

