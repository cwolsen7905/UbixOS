# $Id: Makefile 54 2016-01-11 01:29:55Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

all: core-code net-code api-code netif-code

core-code: core 
	(cd core;make)

net-code: net
	(cd net;make)

api-code: api
	(cd api;make)

netif-code: netif
	(cd netif;make)

clean:
	(cd core;make clean)
	(cd net;make clean)
	(cd api;make clean)
	(cd netif;make clean)
