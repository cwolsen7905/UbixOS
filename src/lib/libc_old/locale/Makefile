# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.inc
include ../../Makefile.inc

#Objects
OBJS = mbrtowc.o srune.o none.o wcrtomb.o table.o runetype.o toupper.o tolower.o

#Output
OUTPUT = libc.so

$(OUTPUT): $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O -I../include -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O -I../include -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -O -I../include -I../../../include -c $<

.c.s:
	$(CC) $(CFLAGS) -Wall -nostdlib -O -I../include -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)
