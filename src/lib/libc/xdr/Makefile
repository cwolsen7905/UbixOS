# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.inc
include ../../Makefile.inc
include ../Makefile.inc

INCLUDES += -I../rpc

#Objects
OBJS = crypt_clnt.o crypt_xdr.o xdr.o xdr_array.o xdr_float.o xdr_mem.o xdr_rec.o xdr_reference.o xdr_sizeof.o xdr_stdio.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

