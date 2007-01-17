# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Objects
OBJS =

#Sub Sections
SUBS = ./stdio/*.o ./sys/*.o ./string/*.o ./stdlib/*.o ./math/*.o ./quad/*.o ./generic/*.o ./locale/*.o ./gen/*.o

#Output
OUTPUT = libc_old.so

lib.so: $(OBJS)
	(cd stdio;make)
	(cd stdlib;make)
	(cd math;make)
	(cd locale;make)
	(cd quad;make)
	(cd sys;make)
	(cd string;make)
	(cd generic;make)
	(cd gen;make)
#	$(LD) $(LDFLAGS) -o $(OUTPUT) $(OBJS) ./stdio/*.o ./sys/*.o ./string/*.o ./stdlib/*.o
	$(CC) -nostdlib -shared -Wl,-soname,libc_old.so -o $(OUTPUT) $(OBJS) $(SUBS)

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
	(cd stdio;make clean)
	(cd sys;make clean)
	(cd stdlib;make clean)
	(cd locale;make clean)
	(cd generic;make clean)
	(cd string;make clean)
	(cd math;make clean)
	(cd quad;make clean)
	(cd gen;make clean)
