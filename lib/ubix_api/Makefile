# $Id: Makefile 121 2016-01-14 03:18:20Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../Makefile.incl
include ../Makefile.incl

#Objects
OBJS = ubixcwd.o

#Sub Sections
SUBS = ./test/*.o

#Output
OUTPUT = ubix_api.so

lib.so: $(OBJS)
	(cd test;$(MAKE))
	$(CC) -m32 -nostdlib -shared -Wl,-soname,ubix_api.so -Wl,-m,elf_i386 -o ${BUILD_DIR}/lib/$(OUTPUT) $(OBJS) $(SUBS)

# Compile the source files
.cc.o:
	$(CXX)  -Wall -nostdinc -O -I./include -c -o $@ $<
 
.cc.s:
	$(CXX)  -Wall -nostdinc -O -I./include -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) -Wall -nostdinc -O -I./include -I../../include -c $<

.c.s:
	$(CC)  -Wall -nostdinc -O -I./include -S -o $@ $<
 
.S.o:
	$(CC)  -Wall -nostdinc -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) ${OBJS:.o=.d} ${BUILD_DIR}/$(OUTPUT)
	(cd test;${MAKE} clean)

.for _d in ${OBJS:.o=.d}
.sinclude "${_d}"
.endfor