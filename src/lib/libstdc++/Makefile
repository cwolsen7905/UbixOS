# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Objects
OBJS = sinst.o

#Sub Sections
SUBS = 

#Output
OUTPUT = libstdc++.so

#Includes
INCLUDES = -I./include -I. -I../libio -I../../include

lib.so: $(OBJS)
#	$(LD) $(LDFLAGS) -o $(OUTPUT) $(OBJS) ./stdio/*.o ./sys/*.o ./string/*.o ./stdlib/*.o
	$(CC) -nostdlib -shared -Wl,-soname,libc.so -o $(OUTPUT) $(OBJS) $(SUBS)

# Compile the source files
.cc.o:
	$(CXX)  -Wall -nostdinc -DNOBOOL -DTMP_LIB -O $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX)  -Wall -nostdinc -O -I./include -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -nostdinc -O -I./include -c $<

.c.s:
	$(CC)  -Wall -nostdinc -O -I./include -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -nostdinc -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)
