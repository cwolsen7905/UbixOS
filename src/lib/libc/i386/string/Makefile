# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../../Makefile.inc
include ../../../Makefile.inc
include ../Makefile.inc

INCLUDES += -I../

#Objects 
OBJS = bcmp.o bcopy.o bzero.o ffs.o index.o memchr.o memcmp.o memcpy.o memmove.o memset.o rindex.o strcat.o strchr.o strcmp.o strcpy.o strlen.o strncmp.o strrchr.o swab.o wcschr.o wcscmp.o wcslen.o wmemchr.o

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
	$(CC) $(CFLAGS) -Wall -nostdlib $(INCLUDES) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

