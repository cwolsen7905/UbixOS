#  (C) 2002 The UbixOS Project
#  $Id: Makefile 54 2016-01-11 01:29:55Z reddawg $

# Include Global 'Source' Options
include ../../../Makefile.inc
include ../../Makefile.inc

#  Objects
OBJS = ethernetif.o loopif.o tcpdump.o arp.o

all: $(OBJS)

#  Compile Types
.cc.o:
	$(CXX) ${CFLAGS} -DNOBOOL -I../../include -c -o $@ $<
.cc.s:
	$(CXX) ${CFLAGS} -DNOBOOL -I../../include -S -o $@ $<
.c.o:
	$(CC) ${CFLAGS} -I../../include -c -o $@ $<
.c.s:
	$(CC) ${CFLAGS} -I../../include -S -o $@ $<
.S.o:
	$(CC) ${CFLAGS} -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS)
