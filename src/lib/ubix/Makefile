# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# Kernel Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Binary File Name
BINARY = none 

#Objects
OBJS = sstart.o #startup.o

CFLAGS = -m32 -fno-builtin -Wno-uninitialized -O2 -fno-strict-aliasing -pipe -Wsystem-headers -Werror -Wall -Wno-format-y2k -W -Wno-unused-parameter -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow -Wcast-align -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls


# Make the Binary
$(BINARY) : $(OBJS)

# Compile the source files
.cc.o:
	$(CXX)  -m32 -Wall -fomit-frame-pointer -O -I../libc/include -c -o $@ $<
 
.cc.s:
	$(CXX)  -m32 -Wall -fomit-frame-pointer -O -I../libc/include -S -o $@ $<
 
.c.o:
	$(CC)  -m32 -Wall -O -I../libc/include -c -o $@ $<
 
.c.s:
	$(CC)  -m32 -Wall -fomit-frame-pointer -O -I../libc/include -S -o $@ $<
 
.S.o:
	$(CC)  -m32 -Wall -fomit-frame-pointer -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(BINARY)
