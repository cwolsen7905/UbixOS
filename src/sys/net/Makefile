# $Id: Makefile 54 2016-01-11 01:29:55Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

all: core-code ipv4-code ipv6-code net-code api-code netif-code

core-code: core
	(cd core;make)

ipv4-code: core/ipv4
	(cd core/ipv4;make)

ipv6-code: core/ipv6
	(cd core/ipv6;make)

net-code: net
	(cd net;make)

api-code: api
	(cd api;make)

netif-code: netif
	(cd netif;make)

clean:
	(cd core;make clean)
	(cd core/ipv4;make clean)
	(cd core/ipv6;make clean)
	(cd net;make clean)
	(cd api;make clean)
	(cd netif;make clean)
