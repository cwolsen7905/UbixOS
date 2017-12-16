#  (C) 2002 The UbixOS Project
#  $Id: Makefile,v 1.2 2004/07/20 21:17:05 reddawg Exp $

# Include Global 'Source' Options
include ../../../Makefile.incl
include ../../Makefile.incl

#  Objects
OBJS = inet.o mem.o memp.o netif.o pbuf.o stats.o sys.o tcp.o tcp_input.o tcp_output.o udp.o ip.o icmp.o ip_addr.o 

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
	