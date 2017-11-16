# $Id: Makefile 89 2016-01-12 00:20:40Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.inc
include ../../Makefile.inc
include ../Makefile.inc

INCLUDES += -I../locale/ -I../resolv

#Objects
OBJS = addr2ascii.o ascii2addr.o base64.o ether_addr.o eui64.o gai_strerror.o getaddrinfo.o gethostbydns.o gethostbyht.o gethostbynis.o gethostnamadr.o getifaddrs.o getifmaddrs.o getnameinfo.o getnetbydns.o getnetbyht.o getnetbynis.o getnetnamadr.o getproto.o getprotoent.o getprotoname.o getservbyname.o getservbyport.o getservent.o hesiod.o if_indextoname.o if_nameindex.o if_nametoindex.o ip6opt.o linkaddr.o map_v4v6.o name6.o nsdispatch.o nslexer.o nsparser.o nss_compat.o rcmd.o rcmdsh.o recv.o res_mkupdate.o res_update.o rthdr.o send.o sockatmark.o vars.o

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

