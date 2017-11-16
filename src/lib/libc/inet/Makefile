# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.inc
include ../../Makefile.inc
include ../Makefile.inc

INCLUDES += -I../locale/

#Objects
OBJS = inet_addr.o inet_cidr_ntop.o inet_cidr_pton.o inet_lnaof.o inet_makeaddr.o inet_net_ntop.o inet_net_pton.o inet_neta.o inet_netof.o inet_network.o inet_ntoa.o inet_ntop.o inet_pton.o nsap_addr.o

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

