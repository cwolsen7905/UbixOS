# $Id$
# Kernel Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Binary File Name
BINARY = none 

#Objects
OBJS = sstart.o #startup.o

CFLAGS = -fno-builtin -Wno-uninitialized -O2 -fno-strict-aliasing -pipe -Wsystem-headers -Werror -Wall -Wno-format-y2k -W -Wno-unused-parameter -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow -Wcast-align -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls


# Make the Binary
$(BINARY) : $(OBJS)

# Compile the source files
.cc.o:
	$(CXX)  -Wall -fomit-frame-pointer -O -I../libc/include -c -o $@ $<
 
.cc.s:
	$(CXX)  -Wall -fomit-frame-pointer -O -I../libc/include -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -O -I../libc/include -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -fomit-frame-pointer -O -I../libc/include -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -fomit-frame-pointer -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(BINARY)
