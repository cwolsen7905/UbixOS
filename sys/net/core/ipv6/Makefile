#  (C) 2002 The UbixOS Project
#  $Id: Makefile,v 1.2 2004/07/20 21:17:05 reddawg Exp $

# Include Global 'Source' Options
include ../../../../Makefile.incl
include ../../../Makefile.incl

#  Objects
OBJS = dhcp6.o ethip6.o icmp6.o inet6.o ip6.o ip6_addr.o ip6_frag.o mld6.o nd6.o

all: $(OBJS)

#  Compile Types
.cc.o:
	$(CXX) ${CFLAGS} -DNOBOOL -I../../../include -c -o $@ $<
.cc.s:
	$(CXX) ${CFLAGS} -DNOBOOL -I../../../include -S -o $@ $<
.c.o:
	$(CC) ${CFLAGS} -I../../../include -c -o $@ $<
.c.s:
	$(CC) ${CFLAGS} -I../../../include -S -o $@ $<
.S.o:
	$(CC) ${CFLAGS} -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS)
	
