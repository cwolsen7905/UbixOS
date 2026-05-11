# $Id: Makefile 148 2016-01-18 19:34:32Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.incl
include ../Makefile.incl

#Objects
OBJS =

#Sub Sections
SUBS = ./stdio/*.o ./sys/*.o ./string/*.o ./stdlib/*.o ./math/*.o ./quad/*.o ./generic/*.o ./locale/*.o ./gen/*.o ./dirent/*.o

#Output
OUTPUT = libc

lib.so: $(OBJS)
	(cd stdio;$(MAKE))
	(cd stdlib;$(MAKE))
	(cd math;$(MAKE))
	(cd locale;$(MAKE))
	(cd quad;$(MAKE))
	(cd sys;$(MAKE))
	(cd string;$(MAKE))
	(cd generic;$(MAKE))
	(cd gen;$(MAKE))
	(cd dirent;$(MAKE))
#	$(LD) $(LDFLAGS) -o $(OUTPUT) $(OBJS) ./stdio/*.o ./sys/*.o ./string/*.o ./stdlib/*.o
#	$(CC) -nostdlib -shared -Wl,-soname,libc.so -o ${BUILD_DIR}/lib/$(OUTPUT) $(OBJS) $(SUBS)
# Use ld directly to produce ET_DYN (gcc driver ignores -shared with -nostdlib)
	${CROSS_PREFIX}ld -m elf_i386 -shared -x --soname=$(OUTPUT).so -o ${BUILD_DIR}/lib/$(OUTPUT).so `lorder $(OBJS) $(SUBS) | tsort`
	${CROSS_PREFIX}ar rcs ${BUILD_DIR}/lib/$(OUTPUT).a `lorder $(OBJS) $(SUBS) | tsort`
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
	(cd dirent;make clean)
