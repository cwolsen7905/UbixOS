# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.incl
include ../../Makefile.incl

#Objects
OBJS = strtol.o atoi.o abs.o exit.o malloc.o rand.o getopt.o

#Output
OUTPUT = libc.so

$(OUTPUT): $(OBJS)

# Compile the source files
.cc.o:
	$(CXX)  -Wall ${CFLAGS} -nostdinc -O -I../include -c -o $@ $<
 
.cc.s:
	$(CXX)  -Wall ${CFLAGS} -nostdinc -O -I../include -S -o $@ $<
 
.c.o:
	$(CC)  -Wall ${CFLAGS} -nostdinc -O -I../include -I../../../include -c $<

.c.s:
	$(CC)  -Wall ${CFLAGS} -nostdinc -O -I../include -S -o $@ $<
 
.S.o:
	$(CC)  -Wall ${CFLAGS} -nostdinc -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)
