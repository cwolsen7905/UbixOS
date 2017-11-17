# $Id: Makefile 119 2016-01-14 03:06:36Z reddawg $
# Application Makefile (C) 2002-2004 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Linker
LD = ld

#Binary File Name
OUTPUT = ld.so

#Delete Program
REMOVE = rm -f

#Objects
OBJS = main.o findlibrary.o findfunc.o addlibrary.o entry.o

# Link The Binary
$(OUTPUT) : $(OBJS)
	$(CC) -m32 -nostdlib -shared -Wl,-soname,$(OUTPUT) -Wl,-m,elf_i386  -e _ld -o ../../build/$(OUTPUT) $(OBJS) $(LIBRARIES) $(SUBS)
#	strip $(OUTPUT)

# Compile the source files
.cpp.o:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDES) -c -o $@ $<

.cc.o:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDES) -c -o $@ $<

.cc.s:
	$(CXX)  -Wall -O $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -O $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.c.s:
	$(CC)  -Wall -O $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC)  -Wall $(CLFAGS) $(INCLUDES) -c -o $@ $<
 
# Clean Up The junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)
