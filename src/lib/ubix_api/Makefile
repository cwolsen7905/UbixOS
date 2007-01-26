# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Objects
OBJS =

#Sub Sections
SUBS = ./test/*.o ./sys/*.o

#Output
OUTPUT = ubix_api.so

lib.so: $(OBJS)
	(cd test;make)
	(cd sys;make)
	$(CC) -nostdlib -shared -Wl,-soname,ubix_api.so -o $(OUTPUT) $(OBJS) $(SUBS)

# Compile the source files
.cc.o:
	$(CXX)  -Wall -nostdinc -O -I./include -c -o $@ $<
 
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
	(cd test;make clean)
	(cd sys;make clean)
