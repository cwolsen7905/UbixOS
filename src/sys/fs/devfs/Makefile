#  (C) 2002 The UbixOS Project
#  $Id: Makefile 54 2016-01-11 01:29:55Z reddawg $

# Include Global 'Source' Options
include ../../../Makefile.incl
include ../../Makefile.incl

#  Linker
LINKER = ld

#  Remove
REMOVE = rm -fr

#  Objects
OBJS = devfs.o

all: $(OBJS)

#  Compile Types
.cc.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<
.cc.s:
	$(CXX) $(CFLAGS) $(INCLUDES) -S -o $@ $<
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<
.c.s:
	$(CC) $(CFLAGS) $(INCLUDES) -S -o $@ $<
.S.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS)
