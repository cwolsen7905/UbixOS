# $Id: Makefile 121 2016-01-14 03:18:20Z reddawg $
# The System Makefile (C) 2002 The UbixOS Project

all: csu-code ubix_api-code libc_old-code ubix-code #libc-code# libcpp-code
# bin tools
# depend kernel tools

csu-code: csu
	(cd csu;make)

ubix_api-code: ubix_api
	(cd ubix_api;make)

libc_old-code: libc_old
	(cd libc_old;make)

libc-code: libc
	(cd libc;make)

objgfx40: src
	(cd objgfx40;make)

views: src
	(cd src/lib/views/sunlight;make)

ubix-code: ubix
	(cd ubix;make)

libcpp-code: libcpp
	(cd libcpp;make)

bin: src
	(cd src/bin;make)

libstdc++: src
	(cd src/lib/libstdc++;make)

tools: src
	(cd src/tools;make)

install:
	(cd src/sys;make install)
	
clean:
	#(cd src/sys;make clean)
	(cd csu;make clean)
	(cd ubix_api;make clean)
	(cd libc_old;make clean)
	(cd libc;make clean)
	(cd ubix;make clean)
