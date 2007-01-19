# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../../Makefile.inc
include ../../../Makefile.inc
include ../Makefile.inc

INCLUDES += -I../
CFLAGS += -fpic -DPIC

#Objects
OBJS = abs.o div.o labs.o ldiv.o

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
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

