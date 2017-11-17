# $Id: Makefile 148 2016-01-18 19:34:32Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#Objects
OBJS =

#Sub Sections
SUBS = ./stdio/*.o ./sys/*.o ./string/*.o ./stdlib/*.o ./math/*.o ./quad/*.o ./generic/*.o ./locale/*.o ./gen/*.o

#Output
OUTPUT = libc_old

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
#	$(CC) -nostdlib -shared -Wl,-soname,libc_old.so -o ${BUILD_DIR}/$(OUTPUT) $(OBJS) $(SUBS)
# tsort -q for BSD
	$(CC) -m32 -nostdlib -ggdb -shared -Wl,-x -o ${BUILD_DIR}/$(OUTPUT).so -Wl,-soname,$(OUTPUT).so `lorder $(OBJS) $(SUBS) | tsort`
#	$(CC) -nostdlib -ggdb -static -Wl,-x -o ${BUILD_DIR}/$(OUTPUT).static.so -Wl,-soname,$(OUTPUT).static.so `lorder $(OBJS) $(SUBS) | tsort -q`

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
	$(REMOVE) $(OBJS) ${BUILD_DIR}/$(OUTPUT)
	(cd stdio;make clean)
	(cd sys;make clean)
	(cd stdlib;make clean)
	(cd locale;make clean)
	(cd generic;make clean)
	(cd string;make clean)
	(cd math;make clean)
	(cd quad;make clean)
	(cd gen;make clean)
