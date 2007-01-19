# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../Makefile.inc
#include ../../Makefile.inc
#include ../Makefile.inc
include ../libc.inc

#INCLUDES += -I../locale/

#Objects
OBJS = inet_addr.o inet_cidr_ntop.o inet_cidr_pton.o inet_lnaof.o inet_makeaddr.o inet_net_ntop.o inet_net_pton.o inet_neta.o inet_netof.o inet_network.o inet_ntoa.o inet_ntop.o inet_pton.o nsap_addr.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

