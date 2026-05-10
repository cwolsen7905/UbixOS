# $Id: Makefile 54 2016-01-11 01:29:55Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

all: core-code ipv4-code ipv6-code net-code api-code netif-code

core-code: core
	(cd core;${MAKE})

ipv4-code: core/ipv4
	(cd core/ipv4;${MAKE})

ipv6-code: core/ipv6
	(cd core/ipv6;${MAKE})

net-code: net
	(cd net;${MAKE})

api-code: api
	(cd api;${MAKE})

netif-code: netif
	(cd netif;${MAKE})

clean:
	(cd core;${MAKE} clean)
	(cd core/ipv4;${MAKE} clean)
	(cd core/ipv6;${MAKE} clean)
	(cd net;${MAKE} clean)
	(cd api;${MAKE} clean)
	(cd netif;${MAKE} clean)
