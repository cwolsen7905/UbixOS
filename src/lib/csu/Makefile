# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

CFLAGS = -nostdlib -nostdinc -fno-builtin -Wno-uninitialized -O2 -fno-strict-aliasing -pipe -Wsystem-headers -Werror -Wall -Wno-format-y2k -W -Wno-unused-parameter -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow -Wcast-align -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls #-DGCRT

INCLUDES = -I../libc/include -I../../include.new

#Objects
OBJS = crtn.o crti.o crt1.o #crtbegin.o crtend.o

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

