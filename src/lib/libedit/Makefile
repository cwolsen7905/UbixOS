# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

INCLUDES = -I. -I../../include

#Objects
OBJS = read.o el.o

#Sub Sections
SUBS = 

#Output
OUTPUT = libedit.so

lib.so: $(OBJS)
	$(CC) -nostdlib -shared -Wl,-soname,libedit.so -o $(OUTPUT) $(OBJS) $(SUBS)

# Compile the source files
.cc.o:
	$(CXX)  -Wall -nostdinc -O -I./include -c -o $@ $<
 
.cc.s:
	$(CXX)  -Wall -nostdinc -O -I./include -S -o $@ $<
 
.c.o:
	$(CC)  -Wall -nostdinc -O $(INCLUDES) -c $<

.c.s:
	$(CC)  -Wall -nostdinc -O -I./include -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -nostdinc -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)
	(cd test;make clean)
