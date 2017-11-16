# $Id: Makefile 115 2016-01-14 02:55:19Z reddawg $
# Application Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc

#Compiler Flags
CFLAGS = -I../../lib/libc/include -fno-builtin -nostdlib -nostdinc

#Linker
LD = ld

#Binary File Name
BINARY = views

#Delete Program
REMOVE = rm -f

#Objects
OBJS = main.o

#Libraries
#LIBRARIES = ../../lib/libc/libc.so
LIBRARIES = ../../lib/libc/locale/*.o ../../lib/libc/stdio/*.o ../../lib/libc/stdlib/*.o ../../lib/libc/sys/*.o ../../lib/libc/string/*.o

# Link The Binary
$(BINARY) : $(OBJS)
	$(CC) $(CFLAGS) -o ../build/$@ $(STARTUP) $(LIBRARIES) $(OBJS)
	strip $(BINARY)

# Compile the source files
.cc.o:
	$(CXX)  -Wall -fomit-frame-pointer -O $(CFLAGS) -c -o $@ $<
 
.cc.s:
	$(CXX)  -Wall -fomit-frame-pointer -O $(CFLAGS) -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -O $(CFLAGS) -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -fomit-frame-pointer -O $(CFLAGS) -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -fomit-frame-pointer -c -o $@ $<
 
# Clean Up The junk
clean:
	$(REMOVE) $(OBJS) ../build/$(BINARY)
