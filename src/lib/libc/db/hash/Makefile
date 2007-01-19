# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../../Makefile.inc
#include ../../../Makefile.inc
#include ../Makefile.inc
include ../../libc.inc

INCLUDES += -I../ #-I../../include

#Objects
OBJS = hash.o hash_bigkey.o hash_buf.o hash_func.o hash_log2.o hash_page.o ndbm.o

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

