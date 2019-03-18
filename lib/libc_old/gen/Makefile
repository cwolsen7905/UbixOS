# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.incl
include ../../Makefile.incl

#Objects
OBJS = setprogname.o getprogname.o errlst.o err.o

#Output
OUTPUT = libc.so

$(OUTPUT): $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O -I../../../include_old -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O -I../../../include_old -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -O -I. -I../../../include_old -c $<

.c.s:
	$(CC) $(CFLAGS) -Wall -nostdlib -O -I../../../include_old -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS) $(OUTPUT)
